/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include "DFLogDownloadController.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCToolbox.h"
#include "QGCMapEngine.h"
#include "Vehicle.h"
#include "SettingsManager.h"

#include <QDebug>
#include <QSettings>
#include <QUrl>
#include <QBitArray>
#include <QtCore/qmath.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#define kTimeOutMilliseconds 500
#define kGUIRateMilliseconds 17
#define kTableBins           512

QGC_LOGGING_CATEGORY(DFLogDownloadLog, "DFLogDownloadLog")


//----------------------------------------------------------------------------------------
QGCDFLogEntry::QGCDFLogEntry(QString logId, const QDateTime& dateTime, uint logSize)
{
    _logID = logId;
    _logSize = logSize;
    _logTimeUTC = dateTime;
    _received = false;
    _selected = false;
    _status = tr("");
    _downloading = false;
    _queuedDownload = false;
    _isDownloaded = false;


    QString dowloaded_path =qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
    if(!dowloaded_path.endsWith(QDir::separator())) {
        dowloaded_path += QDir::separator();
    }
    dowloaded_path += _logID + ".BIN";

    if (QFile::exists(dowloaded_path))
    {
        _status = tr("Downloaded");
        _isDownloaded = true;
    }

}


//----------------------------------------------------------------------------------------
QString
QGCDFLogEntry::sizeStr()
{
    return QGCMapEngine::bigSizeToString(_logSize);
}


//----------------------------------------------------------------------------------------
DFLogDownloadController::DFLogDownloadController(void)
    : _uas(nullptr)
    , _vehicle(nullptr)
    , _requestingLogEntries(false)
    , _downloadingLogs(false)
    , _retries(0)
    , _apmOneBased(0)
{
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::refresh(void)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/log"));
    manager->get(request);
    QObject::connect(manager, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (jsonDoc.isArray()) {
                _logEntriesModel.clear();
                QJsonArray log_list = jsonDoc.array();
                for(int i = 0; i < log_list.size(); i++)
                {
                    QJsonObject logItem = log_list.at(i).toObject();
                    QDateTime logTime = QDateTime::fromSecsSinceEpoch(logItem.value("date").toDouble());
                    QGCDFLogEntry* logEntry = new QGCDFLogEntry(QString::number(logItem.value("id").toInt()), logTime, logItem.value("size").toInt());
                    _logEntriesModel.append(logEntry);
                }
            }
            qDebug() << "Request finished successfully";
        } else {
            qDebug() << "Error:" << reply->errorString();
            qDebug() << "Error:" << reply->error();
        }
        reply->deleteLater(); // Ensure the reply object is deleted
    });
}

//----------------------------------------------------------------------------------------
void
DFLogDownloadController::_delete(QGCDFLogEntry* entry)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/log/" + entry->id()));
    manager->deleteResource(request);
    QObject::connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request finished successfully";
        } else {
            qDebug() << "Error:" << reply->errorString();
        }
        reply->deleteLater(); // Ensure the reply object is deleted
        refresh();
    });
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::erase()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        QGCDFLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->selected()) {
                _delete(entry);
            }
        }
    }
    refresh();
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::eraseAll(void)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/log"));
    manager->deleteResource(request);
    QObject::connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request finished successfully";
        } else {
            qDebug() << "Error:" << reply->errorString();
        }
        reply->deleteLater(); // Ensure the reply object is deleted
        refresh();
    });
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::download(QGCDFLogEntry* entry)
{
    entry->setQueued(false);
    entry->setDownloading(true);
    _downloadInProgress = true;
    entry->setStatus("Downloading");
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/log/" + entry->id()));
    _reply = manager->get(request); // Manager is my QNetworkAccessManager
    connect(_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(error(QNetworkReply::NetworkError)));
    connect(_reply, SIGNAL(downloadProgress(qint64,qint64)),
                this, SLOT(updateProgress(qint64,qint64)));
    connect(_reply, SIGNAL(finished()),
                this, SLOT(downloadFinished()));
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::error(QNetworkReply::NetworkError err)
{
    // Manage error here.
    QGCDFLogEntry* entry = _getDownloading();
    entry->setStatus("Error");
    // _reply->deleteLater();
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::updateProgress(qint64 read, qint64 total)
{
    // This is where you can use the progress info for, lets say, update a progress bar:
    // progressBar->setMaximum(total);
    // progressBar->setValue(read);
    const QString status = QString("%1 / %2").arg(QGCMapEngine::bigSizeToString(read),
                                                    QGCMapEngine::bigSizeToString(total));

    QGCDFLogEntry* entry = _getDownloading();
    entry->setStatus(status);
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::downloadFinished()
{
    QGCDFLogEntry* entry = _getDownloading();
    if (_reply->error() == QNetworkReply::NoError) {
        _downloadPath = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
        if(!_downloadPath.endsWith(QDir::separator())) {
            _downloadPath += QDir::separator();
        }
        QByteArray b = _reply->readAll();
        qDebug() << "byte array length" << b.length();
        QFile file(_downloadPath + entry->id() + ".BIN");
        file.open(QIODevice::WriteOnly);
        QDataStream out(&file);
        out << b;
        entry->setStatus("Downloaded");
        _downloadInProgress = false;
        entry->setDownloading(false);
    } else {
        qDebug() << "Error:" << _reply->errorString();
        qDebug() << "Error:" << _reply->error();
        entry->setStatus("Error");
    }
    _reply->deleteLater();

    checkForDownloads();
}


//----------------------------------------------------------------------------------------
QGCDFLogEntry*
DFLogDownloadController::_getDownloading()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        QGCDFLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->downloading()) {
               return entry;
            }
        }
    }
    return nullptr;
}


//----------------------------------------------------------------------------------------
QGCDFLogEntry*
DFLogDownloadController::_getNextQueued()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        QGCDFLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->queuedDownload()) {
               return entry;
            }
        }
    }
    return nullptr;
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::checkForDownloads()
{
    if (!_downloadInProgress)
    {
        QGCDFLogEntry* entry = _getNextQueued();
        if (entry != nullptr)
        {
            download(entry);
        }
    }
}


//----------------------------------------------------------------------------------------
void
DFLogDownloadController::queueForDownload()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        QGCDFLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->selected()) {
                if (!entry->isDownloaded()) {
                    entry->setQueued(true);
                    entry->setStatus("Pending");
                }
            }
        }
    }
    checkForDownloads();
}


//-----------------------------------------------------------------------------
QGCDFLogModel::QGCDFLogModel(QObject* parent)
    : QAbstractListModel(parent)
{

}


//-----------------------------------------------------------------------------
QGCDFLogEntry*
QGCDFLogModel::get(int index)
{
    if (index < 0 || index >= _logEntries.count()) {
        qDebug() << "returning nullptr";
        return nullptr;
    }
    return _logEntries[index];
}


//-----------------------------------------------------------------------------
int
QGCDFLogModel::count() const
{
    return _logEntries.count();
}


//-----------------------------------------------------------------------------
void
QGCDFLogModel::append(QGCDFLogEntry* object)
{
    // _logEntries.append(object);
    // emit countChanged();
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
    _logEntries.append(object);
    endInsertRows();
    emit countChanged();
}


//-----------------------------------------------------------------------------
void
QGCDFLogModel::clear(void)
{
    // if (!_logEntries.empty())
    // {
    //     _logEntries.clear();
    //     emit countChanged();
    // }
    if(!_logEntries.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, _logEntries.count());
        while (_logEntries.count()) {
            QGCDFLogEntry* entry = _logEntries.last();
            if(entry) entry->deleteLater();
            _logEntries.removeLast();
        }
        endRemoveRows();
        emit countChanged();
    }
}


//-----------------------------------------------------------------------------
QGCDFLogEntry*
QGCDFLogModel::operator[](int index)
{
    return get(index);
}


//-----------------------------------------------------------------------------
int
QGCDFLogModel::rowCount(const QModelIndex& /*parent*/) const
{
    return _logEntries.count();
}


//-----------------------------------------------------------------------------
QVariant
QGCDFLogModel::data(const QModelIndex & index, int role) const {
    if (index.row() < 0 || index.row() >= _logEntries.count())
        return QVariant();
    if (role == ObjectRole)
        return QVariant::fromValue(_logEntries[index.row()]);
    return QVariant();
}


//-----------------------------------------------------------------------------
QHash<int, QByteArray>
QGCDFLogModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ObjectRole] = "logEntry";
    return roles;
}
