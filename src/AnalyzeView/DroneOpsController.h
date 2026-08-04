/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#ifndef DRONEOPSCONTROLLER_H
#define DRONEOPSCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QVariantList>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

Q_DECLARE_LOGGING_CATEGORY(DroneOpsLog)

//-----------------------------------------------------------------------------
// Pushes locally stored flight logs to the DroneOps web app.
//
// Flow (see docs/CONTROLLER_API.md):
//   1. login          POST /api/v1/auth/token   {email,password,mfa_code?}  -> bearer token (24h)
//   2. refreshJobs     GET /api/v1/jobs?status=open                         -> jobs to sync against
//   3. queueUpload     POST /api/v1/uploads      multipart file,job_id      -> parse_job.id
//   4. (auto) poll     GET /api/v1/parse-jobs/{id}                          -> complete | failed
//
// Auth is a plain bearer token (no cookies, CSRF-exempt). There is no refresh
// token: on a 401 we drop the session and the pilot logs in again.
class DroneOpsController : public QObject
{
    Q_OBJECT

public:
    DroneOpsController(QObject* parent = nullptr);

    Q_PROPERTY(QString      baseUrl         READ baseUrl        WRITE setBaseUrl    NOTIFY baseUrlChanged)
    Q_PROPERTY(QString      email           READ email          WRITE setEmail      NOTIFY emailChanged)
    Q_PROPERTY(bool         loggedIn        READ loggedIn                           NOTIFY loggedInChanged)
    Q_PROPERTY(bool         busy            READ busy                               NOTIFY busyChanged)
    Q_PROPERTY(QString      statusText      READ statusText                         NOTIFY statusTextChanged)
    Q_PROPERTY(bool         errorStatus     READ errorStatus                        NOTIFY statusTextChanged)
    Q_PROPERTY(bool         mfaRequired     READ mfaRequired                        NOTIFY mfaRequiredChanged)
    Q_PROPERTY(QString      mfaOtpauthUri   READ mfaOtpauthUri                      NOTIFY mfaRequiredChanged)
    Q_PROPERTY(QString      mfaQrSvg        READ mfaQrSvg                           NOTIFY mfaRequiredChanged)
    Q_PROPERTY(QVariantList jobs            READ jobs                               NOTIFY jobsChanged)
    Q_PROPERTY(QVariantList logFiles        READ logFiles                           NOTIFY logFilesChanged)
    Q_PROPERTY(bool         uploading       READ uploading                          NOTIFY uploadingChanged)
    Q_PROPERTY(double       uploadProgress  READ uploadProgress                     NOTIFY uploadProgressChanged)
    Q_PROPERTY(QString      parseStatus     READ parseStatus                        NOTIFY parseStatusChanged)
    Q_PROPERTY(QVariantList uploadQueue     READ uploadQueue                        NOTIFY uploadQueueChanged)
    Q_PROPERTY(int          pendingCount    READ pendingCount                       NOTIFY uploadQueueChanged)

    QString      baseUrl        () const { return _baseUrl; }
    QString      email          () const { return _email; }
    bool         loggedIn       () const { return _loggedIn; }
    bool         busy           () const { return _busy; }
    QString      statusText     () const { return _statusText; }
    bool         errorStatus    () const { return _errorStatus; }
    bool         mfaRequired    () const { return _mfaRequired; }
    QString      mfaOtpauthUri  () const { return _mfaOtpauthUri; }
    QString      mfaQrSvg       () const { return _mfaQrSvg; }
    QVariantList jobs           () const { return _jobs; }
    QVariantList logFiles       () const { return _logFiles; }
    bool         uploading      () const { return _uploading; }
    double       uploadProgress () const { return _uploadProgress; }
    QString      parseStatus    () const { return _parseStatus; }
    QVariantList uploadQueue    () const { return _queue; }
    int          pendingCount   () const;

    void setBaseUrl (const QString& url);
    void setEmail   (const QString& email);

    // password/mfaCode are never persisted. Pass mfaCode empty on the first attempt.
    Q_INVOKABLE void login          (const QString& password, const QString& mfaCode = QString());
    Q_INVOKABLE void logout         ();
    Q_INVOKABLE void refreshJobs    ();
    Q_INVOKABLE void refreshLogFiles();
    Q_INVOKABLE void cancelUpload   ();

    // Offline queue: add a log + chosen job to the persistent queue and drain it
    // when signed in. Retries are safe (server dedupes by SHA-256).
    Q_INVOKABLE void queueUpload    (const QString& jobId, const QString& jobLabel, const QString& filePath);
    Q_INVOKABLE void processQueue   ();
    Q_INVOKABLE void retryQueued    (int index);
    Q_INVOKABLE void removeQueued   (int index);
    Q_INVOKABLE void clearCompleted ();

signals:
    void baseUrlChanged         ();
    void emailChanged           ();
    void loggedInChanged        ();
    void busyChanged            ();
    void statusTextChanged      ();
    void mfaRequiredChanged     ();
    void jobsChanged            ();
    void logFilesChanged        ();
    void uploadingChanged       ();
    void uploadProgressChanged  ();
    void parseStatusChanged     ();
    void uploadQueueChanged     ();
    void uploadComplete         (bool success, const QString& message);

private:
    QNetworkRequest _authedRequest  (const QString& path) const;
    void            _setBusy        (bool busy);
    void            _setStatus      (const QString& text, bool error = false);
    void            _setLoggedIn    (bool loggedIn);
    void            _setUploading   (bool uploading);
    void            _setUploadProgress(double progress);
    void            _setParseStatus (const QString& status);
    void            _handleExpired  ();          // 401 -> session gone
    void            _startPolling   (const QString& parseJobId);
    void            _pollParseJob   ();
    void            _loadPersisted  ();
    void            _persist        ();

    // Queue helpers
    void            _uploadItem         (int index);
    void            _finishActiveItem   (const QString& status, const QString& error = QString());
    void            _setQueueItemStatus (int index, const QString& status, const QString& error = QString());
    void            _persistQueue       ();
    void            _loadQueue          ();
    void            _persistJobs        ();
    void            _updateRetryTimer   ();   // run the retry ticker only while signed in with work pending

    QNetworkAccessManager   _nam;

    QString     _baseUrl;
    QString     _email;
    QString     _token;
    QDateTime   _tokenExpiry;
    bool        _loggedIn       = false;
    bool        _busy           = false;
    QString     _statusText;
    bool        _errorStatus    = false;
    bool        _mfaRequired    = false;
    QString     _mfaOtpauthUri;
    QString     _mfaQrSvg;
    QVariantList _jobs;
    QVariantList _logFiles;

    bool        _uploading      = false;
    double      _uploadProgress = 0.0;
    QNetworkReply* _uploadReply = nullptr;

    QString     _parseStatus;
    QString     _parseJobId;
    QTimer      _pollTimer;
    int         _pollElapsedSec  = 0;

    QVariantList _queue;                 // persistent offline upload queue
    int          _activeQueueIndex = -1; // index of the item currently uploading/parsing, or -1
    QTimer       _retryTimer;            // periodically retries pending items (e.g. once back online)

    static constexpr int kPollIntervalMs  = 3000;
    static constexpr int kPollTimeoutSec  = 600;    // give up following a parse job after 10 min
    static constexpr int kRetryIntervalMs = 30000;  // how often to retry pending queue items
};

#endif // DRONEOPSCONTROLLER_H
