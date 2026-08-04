/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DroneOpsController.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"
#include "AppSettings.h"

#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocale>
#include <QUrl>
#include <algorithm>

QGC_LOGGING_CATEGORY(DroneOpsLog, "DroneOpsLog")

static const char* kSettingsGroup   = "DroneOps";
static const char* kDefaultBaseUrl  = "https://droneops-dev.aeronavics.com";
static const qint64 kMaxUploadBytes = 512LL * 1024 * 1024;   // server default MAX_UPLOAD_SIZE_MB

// Queue item status values
static const char* kStatusPending    = "pending";
static const char* kStatusUploading  = "uploading";
static const char* kStatusProcessing = "processing";
static const char* kStatusDone       = "done";
static const char* kStatusDuplicate  = "duplicate";
static const char* kStatusUploaded   = "uploaded";   // file accepted, parse outcome unconfirmed
static const char* kStatusError      = "error";

//-----------------------------------------------------------------------------
DroneOpsController::DroneOpsController(QObject* parent)
    : QObject(parent)
    , _nam(this)
{
    _pollTimer.setInterval(kPollIntervalMs);
    _pollTimer.setSingleShot(false);
    connect(&_pollTimer, &QTimer::timeout, this, [this]() {
        _pollElapsedSec += kPollIntervalMs / 1000;
        _pollParseJob();
    });

    _retryTimer.setInterval(kRetryIntervalMs);
    _retryTimer.setSingleShot(false);
    connect(&_retryTimer, &QTimer::timeout, this, [this]() { processQueue(); });

    _loadPersisted();
    _loadQueue();
    if (_baseUrl.isEmpty()) {
        _baseUrl = kDefaultBaseUrl;
    }
    refreshLogFiles();
    _updateRetryTimer();    // resume auto-retry if we restored a session with queued work
}

//-----------------------------------------------------------------------------
void DroneOpsController::_loadPersisted()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    _baseUrl        = settings.value("baseUrl", QString()).toString();
    _email          = settings.value("email", QString()).toString();
    _token          = settings.value("token", QString()).toString();
    _tokenExpiry    = settings.value("tokenExpiry").toDateTime();
    const QByteArray jobsJson = settings.value("jobs").toByteArray();
    settings.endGroup();

    // A stored token is only usable while it is still inside its 24h window.
    if (!_token.isEmpty() && _tokenExpiry.isValid() && _tokenExpiry > QDateTime::currentDateTimeUtc()) {
        _loggedIn = true;
    } else {
        _token.clear();
    }

    // Keep the last-seen job list so a pilot can still queue against it while offline.
    if (!jobsJson.isEmpty()) {
        _jobs = QJsonDocument::fromJson(jobsJson).array().toVariantList();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_persist()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue("baseUrl",     _baseUrl);
    settings.setValue("email",       _email);
    settings.setValue("token",       _token);
    settings.setValue("tokenExpiry", _tokenExpiry);
    settings.endGroup();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_persistJobs()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue("jobs", QJsonDocument(QJsonArray::fromVariantList(_jobs)).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_persistQueue()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue("queue", QJsonDocument(QJsonArray::fromVariantList(_queue)).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_loadQueue()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    const QByteArray json = settings.value("queue").toByteArray();
    settings.endGroup();

    _queue.clear();
    const QJsonArray arr = QJsonDocument::fromJson(json).array();
    for (const QJsonValue& v : arr) {
        QVariantMap item = v.toObject().toVariantMap();
        // Anything left mid-flight from a previous run goes back to pending.
        const QString status = item.value("status").toString();
        if (status == QLatin1String(kStatusUploading) || status == QLatin1String(kStatusProcessing)) {
            item["status"] = kStatusPending;
        }
        _queue.append(item);
    }
    emit uploadQueueChanged();
}

//-----------------------------------------------------------------------------
int DroneOpsController::pendingCount() const
{
    int n = 0;
    for (const QVariant& v : _queue) {
        if (v.toMap().value("status").toString() == QLatin1String(kStatusPending)) {
            ++n;
        }
    }
    return n;
}

//-----------------------------------------------------------------------------
void DroneOpsController::setBaseUrl(const QString& url)
{
    QString trimmed = url.trimmed();
    if (trimmed == _baseUrl) {
        return;
    }
    _baseUrl = trimmed;
    _persist();
    emit baseUrlChanged();
}

//-----------------------------------------------------------------------------
void DroneOpsController::setEmail(const QString& email)
{
    QString trimmed = email.trimmed();
    if (trimmed == _email) {
        return;
    }
    _email = trimmed;
    _persist();
    emit emailChanged();
}

//-----------------------------------------------------------------------------
QNetworkRequest DroneOpsController::_authedRequest(const QString& path) const
{
    QString root = _baseUrl;
    while (root.endsWith('/')) {
        root.chop(1);
    }
    QNetworkRequest request(QUrl(root + QStringLiteral("/api/v1") + path));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + _token.toUtf8());
    return request;
}

//-----------------------------------------------------------------------------
void DroneOpsController::login(const QString& password, const QString& mfaCode)
{
    if (_baseUrl.isEmpty()) {
        _setStatus(tr("Set the DroneOps server address first."), true);
        return;
    }
    if (_email.isEmpty() || password.isEmpty()) {
        _setStatus(tr("Enter your email and password."), true);
        return;
    }

    _setBusy(true);
    _setStatus(tr("Signing in…"));

    QJsonObject body;
    body["email"]    = _email;
    body["password"] = password;
    if (!mfaCode.isEmpty()) {
        body["mfa_code"] = mfaCode;
    }

    QString root = _baseUrl;
    while (root.endsWith('/')) {
        root.chop(1);
    }
    QNetworkRequest request(QUrl(root + QStringLiteral("/api/v1/auth/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = _nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        _setBusy(false);
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const QString retryAfter = QString::fromUtf8(reply->rawHeader("Retry-After"));
        reply->deleteLater();

        const QJsonObject obj = QJsonDocument::fromJson(data).object();

        if (http == 429) {
            QString wait = retryAfter.isEmpty() ? tr("a little while") : tr("%1 seconds").arg(retryAfter);
            _setStatus(tr("Too many attempts. Try again in %1.").arg(wait), true);
            return;
        }
        if (http == 401) {
            QString detail = obj.value("detail").toString(obj.value("message").toString());
            _setStatus(detail.isEmpty() ? tr("Invalid credentials or MFA code.") : detail, true);
            return;
        }
        if (http != 200) {
            _setStatus(tr("Login failed (HTTP %1).").arg(http), true);
            return;
        }

        const QString status = obj.value("status").toString();
        if (status == QLatin1String("ok")) {
            _token       = obj.value("access_token").toString();
            _tokenExpiry = QDateTime::fromString(obj.value("expires_at").toString(), Qt::ISODateWithMs).toUTC();
            _mfaRequired = false;
            _mfaOtpauthUri.clear();
            _mfaQrSvg.clear();
            emit mfaRequiredChanged();
            _persist();
            _setLoggedIn(true);
            _setStatus(tr("Signed in as %1.").arg(_email));
            refreshJobs();
            refreshLogFiles();
            processQueue();     // drain anything queued while offline
        } else if (status == QLatin1String("mfa_required")) {
            _mfaRequired = true;
            _mfaOtpauthUri.clear();
            _mfaQrSvg.clear();
            emit mfaRequiredChanged();
            _setStatus(tr("Enter the 6-digit code from your authenticator app."));
        } else if (status == QLatin1String("mfa_enrollment_required")) {
            _mfaRequired   = true;
            _mfaOtpauthUri = obj.value("otpauth_uri").toString();
            _mfaQrSvg      = obj.value("qr_svg").toString();
            emit mfaRequiredChanged();
            _setStatus(tr("Set up MFA: scan the QR code in your authenticator app, then enter the code."));
        } else {
            _setStatus(tr("Login failed: %1").arg(status.isEmpty() ? tr("unexpected response") : status), true);
        }
    });
}

//-----------------------------------------------------------------------------
void DroneOpsController::logout()
{
    _pollTimer.stop();
    _token.clear();
    _tokenExpiry = QDateTime();
    _mfaRequired = false;
    emit mfaRequiredChanged();
    _persist();
    _setLoggedIn(false);
    // The upload queue and last-seen jobs are intentionally kept across sign-out.
    _setStatus(tr("Signed out."));
}

//-----------------------------------------------------------------------------
void DroneOpsController::_handleExpired()
{
    _pollTimer.stop();
    // Put any in-flight queue item back so it retries after the next sign-in.
    if (_activeQueueIndex >= 0 && _activeQueueIndex < _queue.size()) {
        _setQueueItemStatus(_activeQueueIndex, kStatusPending);
    }
    _activeQueueIndex = -1;
    _parseJobId.clear();
    _setUploading(false);
    _token.clear();
    _tokenExpiry = QDateTime();
    _persist();
    _setLoggedIn(false);
    _setStatus(tr("Session expired — please sign in again."), true);
}

//-----------------------------------------------------------------------------
void DroneOpsController::refreshJobs()
{
    if (!_loggedIn) {
        _setStatus(tr("Sign in to load jobs."), true);
        return;
    }

    _setBusy(true);
    QNetworkReply* reply = _nam.get(_authedRequest(QStringLiteral("/jobs?status=open")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        _setBusy(false);
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        if (http == 401) {
            _handleExpired();
            return;
        }
        if (http != 200) {
            _setStatus(tr("Could not load jobs (HTTP %1).").arg(http), true);
            return;
        }

        const QJsonArray arr = QJsonDocument::fromJson(data).object().value("jobs").toArray();
        _jobs.clear();
        for (const QJsonValue& v : arr) {
            const QJsonObject j = v.toObject();
            QVariantMap m;
            m["id"]          = j.value("id").toString();
            m["job_number"]  = j.value("job_number").toString();
            m["name"]        = j.value("name").toString();
            m["client_name"] = j.value("client_name").toString();
            m["status"]      = j.value("status").toString();
            m["flight_type"] = j.value("flight_type").toString();
            m["job_date"]    = j.value("job_date").toString();
            _jobs.append(m);
        }
        _persistJobs();
        emit jobsChanged();
        _setStatus(_jobs.isEmpty() ? tr("No open jobs assigned to you.")
                                   : tr("%1 open job(s).").arg(_jobs.size()));
    });
}

//-----------------------------------------------------------------------------
void DroneOpsController::refreshLogFiles()
{
    _logFiles.clear();

    QString logRoot = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
    QDir root(logRoot);
    if (root.exists()) {
        const QStringList filters = { "*.bin", "*.BIN", "*.ulg", "*.ULG", "*.log", "*.tlog" };
        QDirIterator it(logRoot, filters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            QVariantMap m;
            m["name"]        = fi.fileName();
            m["path"]        = fi.absoluteFilePath();
            m["size"]        = static_cast<qlonglong>(fi.size());
            m["sizeStr"]     = QLocale().formattedDataSize(fi.size());
            m["modified"]    = fi.lastModified();
            m["modifiedStr"] = fi.lastModified().toLocalTime().toString("yyyy-MM-dd hh:mm");
            _logFiles.append(m);
        }
    }

    // Newest first.
    std::sort(_logFiles.begin(), _logFiles.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("modified").toDateTime() > b.toMap().value("modified").toDateTime();
    });
    emit logFilesChanged();
}

//-----------------------------------------------------------------------------
//-- Offline upload queue
//-----------------------------------------------------------------------------
void DroneOpsController::queueUpload(const QString& jobId, const QString& jobLabel, const QString& filePath)
{
    if (jobId.isEmpty()) {
        _setStatus(tr("Pick a job to sync the log against."), true);
        return;
    }
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        _setStatus(tr("Log file not found: %1").arg(filePath), true);
        return;
    }

    QVariantMap item;
    item["filePath"]   = fi.absoluteFilePath();
    item["fileName"]   = fi.fileName();
    item["jobId"]      = jobId;
    item["jobLabel"]   = jobLabel;
    item["sizeStr"]    = QLocale().formattedDataSize(fi.size());
    item["status"]     = kStatusPending;
    item["error"]      = QString();
    item["parseJobId"] = QString();
    item["queuedAt"]   = QDateTime::currentDateTime().toString(Qt::ISODate);
    _queue.append(item);
    _persistQueue();
    emit uploadQueueChanged();
    _updateRetryTimer();

    _setStatus(tr("Queued %1.").arg(fi.fileName()));
    processQueue();
}

//-----------------------------------------------------------------------------
void DroneOpsController::processQueue()
{
    if (_uploading || _activeQueueIndex >= 0) {
        return;     // an item is already uploading / parsing
    }
    if (!_loggedIn) {
        if (pendingCount() > 0) {
            _setStatus(tr("%1 log(s) queued — sign in to upload.").arg(pendingCount()));
        }
        return;
    }

    for (int i = 0; i < _queue.size(); ++i) {
        if (_queue.at(i).toMap().value("status").toString() == QLatin1String(kStatusPending)) {
            _uploadItem(i);
            return;
        }
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_uploadItem(int index)
{
    if (index < 0 || index >= _queue.size()) {
        return;
    }
    const QVariantMap item = _queue.at(index).toMap();
    const QString filePath = item.value("filePath").toString();
    const QString jobId    = item.value("jobId").toString();
    const QString fileName = item.value("fileName").toString();

    QFile* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        _setQueueItemStatus(index, kStatusError, tr("Cannot open file."));
        _setStatus(tr("Cannot open %1.").arg(fileName), true);
        // Skip this one and try the next.
        processQueue();
        return;
    }
    if (file->size() > kMaxUploadBytes) {
        file->close();
        delete file;
        _setQueueItemStatus(index, kStatusError, tr("Larger than the 512 MB server limit."));
        _setStatus(tr("%1 is larger than the 512 MB server limit.").arg(fileName), true);
        processQueue();
        return;
    }

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart jobPart;
    jobPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"job_id\""));
    jobPart.setBody(jobId.toUtf8());
    multiPart->append(jobPart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileName)));
    file->setParent(multiPart);          // multiPart owns the file, freed with the reply
    filePart.setBodyDevice(file);
    multiPart->append(filePart);

    QNetworkReply* reply = _nam.post(_authedRequest(QStringLiteral("/uploads")), multiPart);
    multiPart->setParent(reply);
    _uploadReply      = reply;
    _activeQueueIndex = index;

    _setUploading(true);
    _setUploadProgress(0.0);
    _setParseStatus(QString());
    _setQueueItemStatus(index, kStatusUploading);
    _setStatus(tr("Uploading %1…").arg(fileName));

    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 sent, qint64 total) {
        if (total > 0) {
            _setUploadProgress(static_cast<double>(sent) / static_cast<double>(total));
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
        _uploadReply = nullptr;
        // Use _activeQueueIndex (kept in sync across queue edits), not a captured index.
        const int idx = _activeQueueIndex;
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const QNetworkReply::NetworkError netErr = reply->error();
        reply->deleteLater();

        if (http == 401) {
            _handleExpired();       // resets this item to pending, keeps it queued
            emit uploadComplete(false, _statusText);
            return;
        }
        if (http == 413) {
            _finishActiveItem(kStatusError, tr("Exceeds the server size limit."));
            _setStatus(tr("Upload of %1 rejected: exceeds the server size limit.").arg(fileName), true);
            emit uploadComplete(false, _statusText);
            return;
        }
        if (netErr != QNetworkReply::NoError) {
            _setUploading(false);
            _setQueueItemStatus(idx, kStatusPending);
            _activeQueueIndex = -1;
            if (netErr == QNetworkReply::OperationCanceledError) {
                _setStatus(tr("Upload canceled — %1 stays queued.").arg(fileName));
            } else {
                // Likely offline. Keep it queued and stop draining until connectivity returns.
                _setStatus(tr("Offline — %1 will upload when the connection returns.").arg(fileName));
            }
            emit uploadComplete(false, _statusText);
            return;
        }

        _setUploading(false);
        _setUploadProgress(1.0);
        const QJsonObject obj  = QJsonDocument::fromJson(data).object();
        const bool duplicate   = obj.value("duplicate").toBool();
        const QJsonValue pjVal = obj.value("parse_job");

        if (pjVal.isNull() || pjVal.isUndefined()) {
            // Duplicate re-upload: the original already parsed — treat as done.
            _setParseStatus(QStringLiteral("complete"));
            _finishActiveItem(duplicate ? kStatusDuplicate : kStatusDone);
            _setStatus(duplicate ? tr("%1 was already uploaded — nothing to do.").arg(fileName)
                                 : tr("%1 uploaded.").arg(fileName));
            emit uploadComplete(true, _statusText);
            return;
        }

        const QString parseJobId = pjVal.toObject().value("id").toString();
        if (parseJobId.isEmpty()) {
            _finishActiveItem(kStatusUploaded);
            _setStatus(tr("%1 uploaded (no parse job id returned).").arg(fileName), true);
            emit uploadComplete(true, _statusText);
            return;
        }

        // Record the parse job id on the item, then follow it to completion.
        _setQueueItemStatus(idx, kStatusProcessing);
        if (idx >= 0 && idx < _queue.size()) {
            QVariantMap m = _queue.at(idx).toMap();
            m["parseJobId"] = parseJobId;
            _queue[idx] = m;
            _persistQueue();
        }
        _setStatus(duplicate ? tr("Duplicate file — following the existing server job…")
                             : tr("%1 uploaded. Processing on the server…").arg(fileName));
        _startPolling(parseJobId);
    });
}

//-----------------------------------------------------------------------------
void DroneOpsController::cancelUpload()
{
    if (_uploadReply) {
        _uploadReply->abort();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::retryQueued(int index)
{
    if (index < 0 || index >= _queue.size()) {
        return;
    }
    const QString status = _queue.at(index).toMap().value("status").toString();
    if (status == QLatin1String(kStatusError) || status == QLatin1String(kStatusUploaded)) {
        _setQueueItemStatus(index, kStatusPending);
        processQueue();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::removeQueued(int index)
{
    if (index < 0 || index >= _queue.size()) {
        return;
    }
    if (index == _activeQueueIndex) {
        _setStatus(tr("Cancel the active upload before removing it."), true);
        return;
    }
    _queue.removeAt(index);
    // The active index may shift when an earlier item is removed.
    if (_activeQueueIndex > index) {
        --_activeQueueIndex;
    }
    _persistQueue();
    emit uploadQueueChanged();
    _updateRetryTimer();
}

//-----------------------------------------------------------------------------
void DroneOpsController::clearCompleted()
{
    for (int i = _queue.size() - 1; i >= 0; --i) {
        if (i == _activeQueueIndex) {
            continue;
        }
        const QString status = _queue.at(i).toMap().value("status").toString();
        if (status == QLatin1String(kStatusDone) ||
            status == QLatin1String(kStatusDuplicate) ||
            status == QLatin1String(kStatusUploaded)) {
            _queue.removeAt(i);
            if (_activeQueueIndex > i) {
                --_activeQueueIndex;
            }
        }
    }
    _persistQueue();
    emit uploadQueueChanged();
    _updateRetryTimer();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_finishActiveItem(const QString& status, const QString& error)
{
    if (_activeQueueIndex >= 0 && _activeQueueIndex < _queue.size()) {
        _setQueueItemStatus(_activeQueueIndex, status, error);
    }
    _activeQueueIndex = -1;
    _setUploading(false);
    processQueue();     // move on to the next pending item, if any
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setQueueItemStatus(int index, const QString& status, const QString& error)
{
    if (index < 0 || index >= _queue.size()) {
        return;
    }
    QVariantMap m = _queue.at(index).toMap();
    m["status"] = status;
    m["error"]  = error;
    _queue[index] = m;
    _persistQueue();
    emit uploadQueueChanged();
    _updateRetryTimer();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_updateRetryTimer()
{
    if (_loggedIn && pendingCount() > 0) {
        if (!_retryTimer.isActive()) {
            _retryTimer.start();
        }
    } else if (_retryTimer.isActive()) {
        _retryTimer.stop();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_startPolling(const QString& parseJobId)
{
    _parseJobId     = parseJobId;
    _pollElapsedSec = 0;
    _setParseStatus(QStringLiteral("queued"));
    _pollParseJob();        // first poll immediately
    _pollTimer.start();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_pollParseJob()
{
    if (_parseJobId.isEmpty()) {
        _pollTimer.stop();
        return;
    }

    QNetworkReply* reply = _nam.get(_authedRequest(QStringLiteral("/parse-jobs/") + _parseJobId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const QNetworkReply::NetworkError netErr = reply->error();
        reply->deleteLater();

        if (http == 401) {
            _handleExpired();
            emit uploadComplete(false, _statusText);
            return;
        }
        if (http == 404) {
            _pollTimer.stop();
            _parseJobId.clear();
            _setParseStatus(QStringLiteral("failed"));
            _setStatus(tr("The server has no record of that parse job."), true);
            _finishActiveItem(kStatusError, tr("Server lost the parse job."));
            emit uploadComplete(false, _statusText);
            return;
        }
        if (netErr != QNetworkReply::NoError) {
            // Transient network hiccup — keep polling until the overall timeout.
            if (_pollElapsedSec >= kPollTimeoutSec) {
                _pollTimer.stop();
                _parseJobId.clear();
                _setStatus(tr("Uploaded, but could not confirm processing (offline)."), true);
                // File is on the server; leave the item as uploaded rather than failed.
                _finishActiveItem(kStatusUploaded, tr("Upload accepted; parse result unconfirmed."));
                emit uploadComplete(true, _statusText);
            }
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        const QString status  = obj.value("status").toString();
        _setParseStatus(status);

        if (status == QLatin1String("complete")) {
            _pollTimer.stop();
            _parseJobId.clear();
            _setStatus(tr("Log synced — flight is attached to the job."));
            _finishActiveItem(kStatusDone);
            emit uploadComplete(true, _statusText);
        } else if (status == QLatin1String("failed")) {
            _pollTimer.stop();
            _parseJobId.clear();
            const QString err = obj.value("error_message").toString();
            _setStatus(tr("Server could not parse the log: %1").arg(err.isEmpty() ? tr("unknown error") : err), true);
            _finishActiveItem(kStatusError, err.isEmpty() ? tr("Parse failed.") : err);
            emit uploadComplete(false, _statusText);
        } else {
            const int pct        = obj.value("progress_percent").toInt();
            const QString msg    = obj.value("progress_message").toString();
            _setStatus(msg.isEmpty() ? tr("Processing on the server… %1%").arg(pct)
                                     : tr("Processing on the server… %1% (%2)").arg(pct).arg(msg));
            if (_pollElapsedSec >= kPollTimeoutSec) {
                _pollTimer.stop();
                _parseJobId.clear();
                _setStatus(tr("Still processing after 10 minutes — check DroneOps later."), true);
                _finishActiveItem(kStatusUploaded, tr("Still processing when we stopped watching."));
                emit uploadComplete(false, _statusText);
            }
        }
    });
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setBusy(bool busy)
{
    if (_busy != busy) {
        _busy = busy;
        emit busyChanged();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setStatus(const QString& text, bool error)
{
    _statusText  = text;
    _errorStatus = error;
    if (error) {
        qCWarning(DroneOpsLog) << text;
    } else {
        qCDebug(DroneOpsLog) << text;
    }
    emit statusTextChanged();
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setLoggedIn(bool loggedIn)
{
    if (_loggedIn != loggedIn) {
        _loggedIn = loggedIn;
        emit loggedInChanged();
        _updateRetryTimer();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setUploading(bool uploading)
{
    if (_uploading != uploading) {
        _uploading = uploading;
        emit uploadingChanged();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setUploadProgress(double progress)
{
    if (_uploadProgress != progress) {
        _uploadProgress = progress;
        emit uploadProgressChanged();
    }
}

//-----------------------------------------------------------------------------
void DroneOpsController::_setParseStatus(const QString& status)
{
    if (_parseStatus != status) {
        _parseStatus = status;
        emit parseStatusChanged();
    }
}
