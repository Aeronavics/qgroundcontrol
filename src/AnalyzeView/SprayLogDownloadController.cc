/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include "SprayLogDownloadController.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCToolbox.h"
#include "QGCMapEngine.h"
#include "Vehicle.h"
#include "SettingsManager.h"
#include "QGCQGeoCoordinate.h"
#include <cmath>
#include <algorithm>

#include <QDebug>
#include <QSettings>
#include <QUrl>
#include <QBitArray>
#include <QtCore/qmath.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDir>
#include <QStringList>

#define kTimeOutMilliseconds 500
#define kGUIRateMilliseconds 17
#define kTableBins           512
#define minZoomLevel         19.0


QGC_LOGGING_CATEGORY(SprayLogDownloadLog, "SprayLogDownloadLog")

QGeoCoordinate   SprayLogDownloadController::_coord = QGeoCoordinate(0.0,0.0);
double           SprayLogDownloadController::_zoom = 2;

//----------------------------------------------------------------------------------------
SprayLogEntry::SprayLogEntry(QString logId, const QDateTime& dateTime, uint logSize)
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


    QString dowloaded_path = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
    if(!dowloaded_path.endsWith(QDir::separator())) {
        dowloaded_path += QDir::separator();
    }
    dowloaded_path += "spray_logs/" + _logID + ".txt";

    if (QFile::exists(dowloaded_path))
    {
        _status = tr("Downloaded");
        _isDownloaded = true;
    }

}


//----------------------------------------------------------------------------------------
QString
SprayLogEntry::sizeStr()
{
    return QGCMapEngine::bigSizeToString(_logSize);
}


//----------------------------------------------------------------------------------------
SprayLogDownloadController::SprayLogDownloadController(void)
    : _uas(nullptr)
    , _vehicle(nullptr)
    , _requestingLogEntries(false)
    , _downloadingLogs(false)
    , _retries(0)
    , _apmOneBased(0)
    , _loading(false)
    , _loaded(false)
{
    connect(this, &SprayLogDownloadController::mapCenterChanged, this, [](QGeoCoordinate){});
    connect(this, &SprayLogDownloadController::mapZoomLevelChanged, this, [](double){});
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::refresh(void)
{
    _setLoading(true);
    _setLoadingComplete(false);
    if (qgcApp()->toolbox()->multiVehicleManager()->activeVehicle())
    {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QNetworkRequest request(QUrl("http://192.168.144.1/spray_log"));
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
                        SprayLogEntry* logEntry = new SprayLogEntry(QString::number(logItem.value("id").toInt()), logTime, logItem.value("size").toInt());
                        _logEntriesModel.append(logEntry);
                    }
                }
                _logEntriesModel.sort_by_id();
                _setLoadingComplete(true);
                _setLoading(false);
                qDebug() << "Request finished successfully";
            } else {
                qDebug() << "Error:" << reply->errorString();
                qDebug() << "Error:" << reply->error();

                _logEntriesModel.clear();

                // Check Downloaded
                QString dowload_path = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
                if(!dowload_path.endsWith(QDir::separator())) {
                    dowload_path += QDir::separator();
                }
                dowload_path += "spray_logs";

                QDir directory(dowload_path);
                QStringList files = directory.entryList(QDir::Files | QDir::NoDotAndDotDot);

                foreach(QString filename, files) {
                    qDebug() << filename;
                    QFileInfo file_info(dowload_path + QDir::separator() + filename);

                    SprayLogEntry* logEntry = new SprayLogEntry(QString::number(file_info.baseName().toInt()), file_info.lastModified(), file_info.size());

                    _logEntriesModel.append(logEntry);
                }
                _logEntriesModel.sort_by_id();
                _setLoadingComplete(true);
                _setLoading(false);
            }
            reply->deleteLater(); // Ensure the reply object is deleted
        });
    }
    else
    {
        _logEntriesModel.clear();

        // Check Downloaded
        QString dowload_path = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
        if(!dowload_path.endsWith(QDir::separator())) {
            dowload_path += QDir::separator();
        }
        dowload_path += "spray_logs";

        QDir directory(dowload_path);
        QStringList files = directory.entryList(QDir::Files | QDir::NoDotAndDotDot);

        foreach(QString filename, files) {
            qDebug() << filename;
            QFileInfo file_info(dowload_path + QDir::separator() + filename);

            SprayLogEntry* logEntry = new SprayLogEntry(QString::number(file_info.baseName().toInt()), file_info.lastModified(), file_info.size());

            _logEntriesModel.append(logEntry);
        }
        _logEntriesModel.sort_by_id();
        _setLoadingComplete(true);
        _setLoading(false);
    }
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::_setLoading(bool loading)
{
    _loading = loading;
    emit loadingChanged();
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::_setLoadingComplete(bool loaded)
{
    _loaded = loaded;
    emit loadingCompleteChanged();
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::_delete(SprayLogEntry* entry)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/spray_log/" + entry->id()));
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
SprayLogDownloadController::erase()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
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
SprayLogDownloadController::eraseAll(void)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/spray_log"));
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
SprayLogDownloadController::download(SprayLogEntry* entry)
{
    entry->setQueued(false);
    entry->setDownloading(true);
    _downloadInProgress = true;
    entry->setStatus("Downloading");
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://192.168.144.1/spray_log/" + entry->id()));
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
SprayLogDownloadController::error(QNetworkReply::NetworkError err)
{
    // Manage error here.
    SprayLogEntry* entry = _getDownloading();
    qDebug() << err;
    entry->setStatus("Error");
    _downloadInProgress = false;
    entry->setDownloading(false);
    // _reply->deleteLater();
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::updateProgress(qint64 read, qint64 total)
{
    // This is where you can use the progress info for, lets say, update a progress bar:
    // progressBar->setMaximum(total);
    // progressBar->setValue(read);
    const QString status = QString("%1 / %2").arg(QGCMapEngine::bigSizeToString(read),
                                                  QGCMapEngine::bigSizeToString(total));

    SprayLogEntry* entry = _getDownloading();
    entry->setStatus(status);
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::downloadFinished()
{
    SprayLogEntry* entry = _getDownloading();
    if (_reply->error() == QNetworkReply::NoError) {
        _downloadPath = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
        if(!_downloadPath.endsWith(QDir::separator())) {
            _downloadPath += QDir::separator();
        }
        QByteArray b = _reply->readAll();
        qDebug() << "byte array length" << b.length();\

        if (!QDir(_downloadPath + "spray_logs").exists())
        {
            QDir().mkdir(_downloadPath + "spray_logs");
        }
        if (!QDir(_downloadPath + "spray_maps").exists())
        {
            QDir().mkdir(_downloadPath + "spray_maps");
        }

        QFile file(_downloadPath + "spray_logs/" + entry->id() + ".txt");
        file.open(QIODevice::WriteOnly);
        QDataStream out(&file);
        out << b;
        entry->setStatus("Downloaded");
    } else {
        qDebug() << "Error:" << _reply->errorString();
        qDebug() << "Error:" << _reply->error();
        entry->setStatus("Error");
    }
    _downloadInProgress = false;
    entry->setDownloading(false);
    _reply->deleteLater();

    checkForDownloads();
}


//----------------------------------------------------------------------------------------
SprayLogEntry*
SprayLogDownloadController::_getDownloading()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->downloading()) {
                return entry;
            }
        }
    }
    return nullptr;
}


//----------------------------------------------------------------------------------------
SprayLogEntry*
SprayLogDownloadController::_getNextQueued()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
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
SprayLogDownloadController::checkForDownloads()
{
    if (!_downloadInProgress)
    {
        SprayLogEntry* entry = _getNextQueued();
        if (entry != nullptr)
        {
            download(entry);
        }
    }
}


//----------------------------------------------------------------------------------------
void
SprayLogDownloadController::queueForDownload()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
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
void
SprayLogDownloadController::setMapZoomLevel(double zoom)
{
    if (zoom != mapZoomLevel()) {
        _zoom = zoom;
        emit mapZoomLevelChanged(zoom);
    }
}

//-----------------------------------------------------------------------------
void
SprayLogDownloadController::setMapCenter(QGeoCoordinate& coordinate)
{
    if (coordinate != mapCenter()) {
        _coord.setLatitude(coordinate.latitude());
        _coord.setLongitude(coordinate.longitude());
        emit mapCenterChanged(coordinate);
    }
}

//-----------------------------------------------------------------------------
void
SprayLogDownloadController::addLogsToFlightMap()
{
    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->selected()) {
                _downloadPath = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
                if(!_downloadPath.endsWith(QDir::separator())) {
                    _downloadPath += QDir::separator();
                }

                QFile file(_downloadPath + "spray_logs/" + entry->id() + ".txt");
                if (file.open(QIODevice::ReadOnly)) {
                    QTextStream in(&file);
                    while (!in.atEnd())
                    {
                        QString line = in.readLine();
                        line = line.remove(QChar('\n'));
                        line = line.remove(QChar('\0'));
                        line = line.remove(QChar('\u0017'));

                        if (line.isEmpty())
                        {
                            continue;
                        }

                        QStringList coords = line.split(",");

                        QString sprayStatus = coords[0];
                        double longitude = coords[1].toDouble();
                        double latitiude = coords[2].toDouble();

                        QGeoCoordinate sprayCoordinate(latitiude, longitude, 0.0);

                        if (sprayStatus == "start")
                        {
                            qgcApp()->toolbox()->multiVehicleManager()->activeVehicle()->addSprayTriggerPoint(sprayCoordinate);
                        }
                        else if (sprayStatus == "spraying")
                        {
                            qgcApp()->toolbox()->multiVehicleManager()->activeVehicle()->addSprayingPoint(sprayCoordinate);
                        }
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
void
SprayLogDownloadController::addLogsToSprayMap(int width, int height)
{
    //-- Clear the current points
    _sprayStartPoints.clearAndDeleteContents();
    _sprayTrailPoints.clearAndDeleteContents();

    //-- Iterate entries and look for a selected file
    int num_logs = _logEntriesModel.count();
    for(int i = 0; i < num_logs; i++) {
        SprayLogEntry* entry = _logEntriesModel[i];
        if(entry) {
            if(entry->selected()) {
                _downloadPath = qgcApp()->toolbox()->settingsManager()->appSettings()->logSavePath();
                if(!_downloadPath.endsWith(QDir::separator())) {
                    _downloadPath += QDir::separator();
                }
                QFile file(_downloadPath + "spray_logs/" + entry->id() + ".txt");
                if (file.open(QIODevice::ReadOnly)) {
                    QTextStream in(&file);
                    while (!in.atEnd())
                    {
                        QString line = in.readLine();
                        line = line.remove(QChar('\n'));
                        line = line.remove(QChar('\0'));
                        line = line.remove(QChar('\u0017'));
                        line = line.remove("\\00");

                        if (line.isEmpty())
                        {
                            continue;
                        }
                        
                        QStringList coords = line.split(",");

                        if (coords.count() < 3)
                        {
                            continue;
                        }

                        QString sprayStatus = coords[0];

                        double longitude = coords[1].toDouble();
                        double latitiude = coords[2].toDouble();

                        QGeoCoordinate sprayCoordinate(latitiude, longitude, 0.0);

                        if (sprayStatus == "start")
                        {
                            _sprayStartPoints.append(new QGCQGeoCoordinate(sprayCoordinate, this));
                        }
                        else if (sprayStatus == "spraying")
                        {
                            _sprayTrailPoints.append(new QGCQGeoCoordinate(sprayCoordinate, this));
                        }
                    }
                }
            }
        }
    }
    //-- Calculate zoom level and map center
    _calculateZoomAndCenter(width, height);

}

//-----------------------------------------------------------------------------
void
SprayLogDownloadController::_calculateZoomAndCenter(int width, int height)
{
    if (_sprayStartPoints.count() < 1)
    {
        return;
    }

    QGCQGeoCoordinate* firstLocation = _sprayStartPoints.value<QGCQGeoCoordinate*>(0);
    double min_lat = firstLocation->coordinate().latitude();
    double max_lat = firstLocation->coordinate().latitude();
    double summed_lat = firstLocation->coordinate().latitude();
    double min_lon = firstLocation->coordinate().longitude();
    double max_lon = firstLocation->coordinate().longitude();
    double summed_lon = firstLocation->coordinate().longitude();

    for(int i = 1; i < _sprayStartPoints.count(); i++) {
        QGCQGeoCoordinate* sprayLocation = _sprayStartPoints.value<QGCQGeoCoordinate*>(i);
        if (sprayLocation->coordinate().latitude() < min_lat) {
            min_lat = sprayLocation->coordinate().latitude();
        }
        else if (sprayLocation->coordinate().latitude() > max_lat) {
            max_lat = sprayLocation->coordinate().latitude();
        }

        if (sprayLocation->coordinate().longitude() < min_lon) {
            min_lon = sprayLocation->coordinate().longitude();
        }
        else if (sprayLocation->coordinate().longitude() > max_lon) {
            max_lon = sprayLocation->coordinate().longitude();
        }

        summed_lat += sprayLocation->coordinate().latitude();
        summed_lon += sprayLocation->coordinate().longitude();
    }

    for(int i = 0; i < _sprayTrailPoints.count(); i++) {
        QGCQGeoCoordinate* sprayLocation = _sprayTrailPoints.value<QGCQGeoCoordinate*>(i);
        if (sprayLocation->coordinate().latitude() < min_lat) {
            min_lat = sprayLocation->coordinate().latitude();
        }
        else if (sprayLocation->coordinate().latitude() > max_lat) {
            max_lat = sprayLocation->coordinate().latitude();
        }

        if (sprayLocation->coordinate().longitude() < min_lon) {
            min_lon = sprayLocation->coordinate().longitude();
        }
        else if (sprayLocation->coordinate().longitude() > max_lon) {
            max_lon = sprayLocation->coordinate().longitude();
        }

        summed_lat += sprayLocation->coordinate().latitude();
        summed_lon += sprayLocation->coordinate().longitude();
    }

    double average_lat = summed_lat / (_sprayStartPoints.count() + _sprayTrailPoints.count());
    double average_lon = summed_lon / (_sprayStartPoints.count() + _sprayTrailPoints.count());
    QGeoCoordinate map_center = QGeoCoordinate(average_lat, average_lon, 0.0);

    QGeoCoordinate min_coord = QGeoCoordinate(min_lat, min_lon);
    QGeoCoordinate max_coord = QGeoCoordinate(max_lat, max_lon);

    setMapCenter(map_center);

    _calculateZoomLevel(width, height, min_coord, max_coord);
}


//-----------------------------------------------------------------------------
void
SprayLogDownloadController::_calculateZoomLevel(int width, int height, QGeoCoordinate min_coords, QGeoCoordinate max_coords)
{
    double minYRads = std::log((std::sin(min_coords.latitude() * M_PI / 180.0) + 1) / std::cos(min_coords.latitude() * M_PI / 180.0));
    double maxYRads = std::log((std::sin(max_coords.latitude() * M_PI / 180.0) + 1) / std::cos(max_coords.latitude() * M_PI / 180.0));
    double centerYRads = (minYRads + maxYRads) / 2;
    double centerY = std::atan(std::sinh(centerYRads)) * 180.0 / M_PI;

    double resolutionHorizontal = std::abs(max_coords.longitude() - min_coords.longitude()) / width;

    double viewYCenter = std::log(std::tan(M_PI * (0.25 + centerY / 360)));
    double viewYMax = std::log(std::tan(M_PI * (0.25 + max_coords.latitude() / 360)));
    double viewHeightHalf = height / 2.0;
    double zoomFactorPowered = viewHeightHalf / (40.7436654315252 * (viewYMax - viewYCenter));
    double resolutionVertical = 360.0 / (zoomFactorPowered * 256);

    double resolution = std::max(resolutionHorizontal, resolutionVertical) * 1.2;
    double zoom = std::log2(360 / (resolution * 256));

    setMapZoomLevel(std::min(zoom, minZoomLevel));
}

//-----------------------------------------------------------------------------
void
SprayLogDownloadController::_calculateMapCenter()
{
    double summed_lat = 0.0;
    double summed_lon = 0.0;

    for(int i = 0; i < _sprayStartPoints.count(); i++) {
        QGCQGeoCoordinate* sprayLocation = _sprayStartPoints.value<QGCQGeoCoordinate*>(i);
        summed_lat += sprayLocation->coordinate().latitude();
        summed_lon += sprayLocation->coordinate().longitude();
    }

    double average_lat = summed_lat / _sprayStartPoints.count();
    double average_lon = summed_lon / _sprayStartPoints.count();
    QGeoCoordinate map_center = QGeoCoordinate(average_lat, average_lon, 0.0);

    setMapCenter(map_center);
}

//-----------------------------------------------------------------------------
SprayLogModel::SprayLogModel(QObject* parent)
    : QAbstractListModel(parent)
{

}


//-----------------------------------------------------------------------------
SprayLogEntry*
SprayLogModel::get(int index)
{
    if (index < 0 || index >= _logEntries.count()) {
        return nullptr;
    }
    return _logEntries[index];
}


//-----------------------------------------------------------------------------
int
SprayLogModel::count() const
{
    return _logEntries.count();
}


//-----------------------------------------------------------------------------
void
SprayLogModel::append(SprayLogEntry* object)
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
SprayLogModel::clear(void)
{
    // if (!_logEntries.empty())
    // {
    //     _logEntries.clear();
    //     emit countChanged();
    // }
    if(!_logEntries.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, _logEntries.count());
        while (_logEntries.count()) {
            SprayLogEntry* entry = _logEntries.last();
            if(entry) entry->deleteLater();
            _logEntries.removeLast();
        }
        endRemoveRows();
        emit countChanged();
    }
}


//-----------------------------------------------------------------------------
SprayLogEntry*
SprayLogModel::operator[](int index)
{
    return get(index);
}


//-----------------------------------------------------------------------------
int
SprayLogModel::rowCount(const QModelIndex& /*parent*/) const
{
    return _logEntries.count();
}


//-----------------------------------------------------------------------------
QVariant
SprayLogModel::data(const QModelIndex & index, int role) const {
    if (index.row() < 0 || index.row() >= _logEntries.count())
        return QVariant();
    if (role == ObjectRole)
        return QVariant::fromValue(_logEntries[index.row()]);
    return QVariant();
}


//-----------------------------------------------------------------------------
QHash<int, QByteArray>
SprayLogModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ObjectRole] = "logEntry";
    return roles;
}

//-----------------------------------------------------------------------------
void
SprayLogModel::sort_by_id()
{
    std::sort(_logEntries.begin(), _logEntries.end(), [](SprayLogEntry* e1, SprayLogEntry* e2)
              {
                  return e1->sort_id() < e2->sort_id();
              });
}
