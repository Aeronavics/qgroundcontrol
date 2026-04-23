/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QUrl>
#include <QDir>
#include <QQuickWindow>
#include <QTcpSocket>
// #if defined(__android__)
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
// #endif

#ifndef QGC_DISABLE_UVC
#include <QCameraInfo>
#endif

#include "ScreenToolsController.h"
#include "VideoManager.h"
#include "QGCToolbox.h"
#include "QGCCorePlugin.h"
#include "QGCOptions.h"
#include "MultiVehicleManager.h"
#include "Settings/SettingsManager.h"
#include "Vehicle.h"
#include "QGCCameraManager.h"

#if defined(QGC_GST_STREAMING)
#include "GStreamer.h"
#include "VideoSettings.h"
#else
#include "GLVideoItemStub.h"
#endif

#ifdef QGC_GST_TAISYNC_ENABLED
#include "TaisyncHandler.h"
#endif

QGC_LOGGING_CATEGORY(VideoManagerLog, "VideoManagerLog")

#if defined(QGC_GST_STREAMING)
static const char* kFileExtension[VideoReceiver::FILE_FORMAT_MAX - VideoReceiver::FILE_FORMAT_MIN] = {
    "mkv",
    "mov",
    "mp4"
};
#endif

//-----------------------------------------------------------------------------
VideoManager::VideoManager(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool(app, toolbox),
    tertiaryCheckerMutex()
{
#if !defined(QGC_GST_STREAMING)
    static bool once = false;
    if (!once) {
        qmlRegisterType<GLVideoItemStub>("org.freedesktop.gstreamer.GLVideoItem", 1, 0, "GstGLVideoItem");
        once = true;
    }
#endif
    // Start thread checking for third camera
    tertiaryVideoChecker = std::thread(&VideoManager::checkForTertiaryStream, this, &_tertiaryStreamAvailable);
}

//-----------------------------------------------------------------------------
VideoManager::~VideoManager()
{

    for (int i = 0; i < 3; i++) {
        if (_videoReceiver[i] != nullptr) {
            delete _videoReceiver[i];
            _videoReceiver[i] = nullptr;
        }
#if defined(QGC_GST_STREAMING)
        if (_videoSink[i] != nullptr) {
            // FIXME: AV: we need some interaface for video sink with .release() call
            // Currently VideoManager is destroyed after corePlugin() and we are crashing on app exit
            // calling qgcApp()->toolbox()->corePlugin()->releaseVideoSink(_videoSink[i]);
            // As for now let's call GStreamer::releaseVideoSink() directly
            GStreamer::releaseVideoSink(_videoSink[i]);
            _videoSink[i] = nullptr;
        }
#endif
    }
}

//-----------------------------------------------------------------------------
void
VideoManager::setToolbox(QGCToolbox *toolbox)
{
   QGCTool::setToolbox(toolbox);
   QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
   qmlRegisterUncreatableType<VideoManager> ("QGroundControl.VideoManager", 1, 0, "VideoManager", "Reference only");
   qmlRegisterUncreatableType<VideoReceiver>("QGroundControl",              1, 0, "VideoReceiver","Reference only");

   // TODO: Those connections should be Per Video, not per VideoManager.
   _videoSettings = toolbox->settingsManager()->videoSettings();
   QString videoSource = _videoSettings->videoSource()->rawValue().toString();
   connect(_videoSettings->videoSource(),   &Fact::rawValueChanged, this, &VideoManager::_videoSourceChanged);
   connect(_videoSettings->udpPort(),       &Fact::rawValueChanged, this, &VideoManager::_udpPortChanged);
   connect(_videoSettings->rtspUrl(),       &Fact::rawValueChanged, this, &VideoManager::_rtspUrlChanged);
   connect(_videoSettings->tcpUrl(),        &Fact::rawValueChanged, this, &VideoManager::_tcpUrlChanged);
   connect(_videoSettings->aspectRatio(),   &Fact::rawValueChanged, this, &VideoManager::_aspectRatioChanged);
   connect(_videoSettings->lowLatencyMode(),&Fact::rawValueChanged, this, &VideoManager::_lowLatencyModeChanged);
   MultiVehicleManager *pVehicleMgr = qgcApp()->toolbox()->multiVehicleManager();
   connect(pVehicleMgr, &MultiVehicleManager::activeVehicleChanged, this, &VideoManager::_setActiveVehicle);

#if defined(QGC_GST_STREAMING)
    GStreamer::blacklist(static_cast<VideoSettings::VideoDecoderOptions>(_videoSettings->forceVideoDecoder()->rawValue().toInt()));
#ifndef QGC_DISABLE_UVC
   // If we are using a UVC camera setup the device name
   _updateUVC();
#endif

    emit isGStreamerChanged();
    qCDebug(VideoManagerLog) << "New Video Source:" << videoSource;
#if defined(QGC_GST_STREAMING)
    _videoReceiver[0] = toolbox->corePlugin()->createVideoReceiver(this);
    _videoReceiver[1] = toolbox->corePlugin()->createVideoReceiver(this);
    _videoReceiver[2] = toolbox->corePlugin()->createVideoReceiver(this);


    ///////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////   Video 0   /////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////
    connect(_videoReceiver[0], &VideoReceiver::streamingChanged, this, [this](bool active){
        _streaming = active;
        emit streamingChanged();
    });

    connect(_videoReceiver[0], &VideoReceiver::onStartComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 0 Start complete, status: " << status;
        if (status == VideoReceiver::STATUS_OK) {
            _videoStarted[0] = true;
            if (_videoSink[0] != nullptr) {
                qCDebug(VideoManagerLog) << "Video 0 start decoding";
                // It is absolutely ok to have video receiver active (streaming) and decoding not active
                // It should be handy for cases when you have many streams and want to show only some of them
                // NOTE that even if decoder did not start it is still possible to record video
                _videoReceiver[0]->startDecoding(_videoSink[0]);
            }
        } else if (status == VideoReceiver::STATUS_INVALID_URL) {
            // Invalid URL - don't restart
        } else if (status == VideoReceiver::STATUS_INVALID_STATE) {
            // Already running
        } else {
            _restartVideo(0);
        }
    });

    connect(_videoReceiver[0], &VideoReceiver::onStopComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 0 Stop complete, status: " << status;
        _videoStarted[0] = false;
        if (status == VideoReceiver::STATUS_INVALID_URL) {
            qCDebug(VideoManagerLog) << "Invalid video URL. Not restarting";
        } else {
            _startReceiver(0);
        }
    });

    connect(_videoReceiver[0], &VideoReceiver::decodingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 0 decoding changed, active: " << (active ? "yes" : "no");
        _decoding = active;
        emit decodingChanged();
    });

    connect(_videoReceiver[0], &VideoReceiver::recordingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 0 recording changed, active: " << (active ? "yes" : "no");
        _recording = active;
        if (!active) {
            _subtitleWriter.stopCapturingTelemetry();
        }
        emit recordingChanged();
    });

    connect(_videoReceiver[0], &VideoReceiver::recordingStarted, this, [this](){
        qCDebug(VideoManagerLog) << "Video 0 recording started";
        _subtitleWriter.startCapturingTelemetry(_videoFile);
    });

    connect(_videoReceiver[0], &VideoReceiver::videoSizeChanged, this, [this](QSize size){
        qCDebug(VideoManagerLog) << "Video 0 resized. New resolution: " << size.width() << "x" << size.height();
        _videoSize = ((quint32)size.width() << 16) | (quint32)size.height();
        emit videoSizeChanged();
    });

    ///////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////   Video 1   /////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////
    connect(_videoReceiver[1], &VideoReceiver::streamingChanged, this, [this](bool active){
        _streaming = active;
        emit streamingChanged();
    });

    connect(_videoReceiver[1], &VideoReceiver::onStartComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 1 Start complete, status: " << status;
        if (status == VideoReceiver::STATUS_OK) {
            _videoStarted[1] = true;
            if (_videoSink[1] != nullptr) {
                qCDebug(VideoManagerLog) << "Video 1 start decoding";
                // It is absolutely ok to have video receiver active (streaming) and decoding not active
                // It should be handy for cases when you have many streams and want to show only some of them
                // NOTE that even if decoder did not start it is still possible to record video
                _videoReceiver[1]->startDecoding(_videoSink[1]);
            }
        } else if (status == VideoReceiver::STATUS_INVALID_URL) {
            // Invalid URL - don't restart
        } else if (status == VideoReceiver::STATUS_INVALID_STATE) {
            // Already running
        } else {
            _restartVideo(1);
        }
    });

    connect(_videoReceiver[1], &VideoReceiver::onStopComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 1 Stop complete, status: " << status;
        _videoStarted[1] = false;
        if (status == VideoReceiver::STATUS_INVALID_URL) {
            qCDebug(VideoManagerLog) << "Invalid video URL. Not restarting";
        } else {
            _startReceiver(1);
        }
    });

    connect(_videoReceiver[1], &VideoReceiver::decodingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 1 decoding changed, active: " << (active ? "yes" : "no");
        _secondaryDecoding = active;
        emit secondaryDecodingChanged();
    });

    connect(_videoReceiver[1], &VideoReceiver::recordingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 1 recording changed, active: " << (active ? "yes" : "no");
        _recording = active;
        if (!active) {
            _subtitleWriter.stopCapturingTelemetry();
        }
        emit recordingChanged();
    });

    connect(_videoReceiver[1], &VideoReceiver::recordingStarted, this, [this](){
        qCDebug(VideoManagerLog) << "Video 1 recording started";
        _subtitleWriter.startCapturingTelemetry(_videoFile);
    });

    connect(_videoReceiver[1], &VideoReceiver::videoSizeChanged, this, [this](QSize size){
        qCDebug(VideoManagerLog) << "Video 1 resized. New resolution: " << size.width() << "x" << size.height();
        _videoSize = ((quint32)size.width() << 16) | (quint32)size.height();
        emit videoSizeChanged();
    });

    ///////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////   Video 2   /////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////
    connect(_videoReceiver[2], &VideoReceiver::streamingChanged, this, [this](bool active){
        _streaming = active;
        emit streamingChanged();
    });

    connect(_videoReceiver[2], &VideoReceiver::onStartComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 2 Start complete, status: " << status;
        if (status == VideoReceiver::STATUS_OK) {
            _videoStarted[2] = true;
            if (_videoSink[2] != nullptr) {
                qCDebug(VideoManagerLog) << "Video 2 start decoding";
                // It is absolutely ok to have video receiver active (streaming) and decoding not active
                // It should be handy for cases when you have many streams and want to show only some of them
                // NOTE that even if decoder did not start it is still possible to record video
                _videoReceiver[2]->startDecoding(_videoSink[2]);
            }
        } else if (status == VideoReceiver::STATUS_INVALID_URL) {
            // Invalid URL - don't restart
        } else if (status == VideoReceiver::STATUS_INVALID_STATE) {
            // Already running
        } else {
            _restartVideo(2);
        }
    });

    connect(_videoReceiver[2], &VideoReceiver::onStopComplete, this, [this](VideoReceiver::STATUS status) {
        qCDebug(VideoManagerLog) << "Video 2 Stop complete, status: " << status;
        _videoStarted[2] = false;
        if (status == VideoReceiver::STATUS_INVALID_URL) {
            qCDebug(VideoManagerLog) << "Invalid video URL. Not restarting";
        } else {
            _startReceiver(2);
        }
    });

    connect(_videoReceiver[2], &VideoReceiver::decodingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 2 decoding changed, active: " << (active ? "yes" : "no");
        _tertiaryDecoding = active;
        emit tertiaryDecodingChanged();
    });

    connect(_videoReceiver[2], &VideoReceiver::recordingChanged, this, [this](bool active){
        qCDebug(VideoManagerLog) << "Video 2 recording changed, active: " << (active ? "yes" : "no");
        _recording = active;
        if (!active) {
            _subtitleWriter.stopCapturingTelemetry();
        }
        emit recordingChanged();
    });

    connect(_videoReceiver[2], &VideoReceiver::recordingStarted, this, [this](){
        qCDebug(VideoManagerLog) << "Video 2 recording started";
        _subtitleWriter.startCapturingTelemetry(_videoFile);
    });

    connect(_videoReceiver[2], &VideoReceiver::videoSizeChanged, this, [this](QSize size){
        qCDebug(VideoManagerLog) << "Video 2 resized. New resolution: " << size.width() << "x" << size.height();
        _videoSize = ((quint32)size.width() << 16) | (quint32)size.height();
        emit videoSizeChanged();
    });

    // if (_videoReceiver[1] != nullptr) {
    //     connect(_videoReceiver[1], &VideoReceiver::onStartComplete, this, [this](VideoReceiver::STATUS status) {
    //         if (status == VideoReceiver::STATUS_OK) {
    //             _videoStarted[1] = true;
    //             if (_videoSink[1] != nullptr) {
    //                 _videoReceiver[1]->startDecoding(_videoSink[1]);
    //             }
    //         } else if (status == VideoReceiver::STATUS_INVALID_URL) {
    //             // Invalid URL - don't restart
    //         } else if (status == VideoReceiver::STATUS_INVALID_STATE) {
    //             // Already running
    //         } else {
    //             _restartVideo(1);
    //         }
    //     });

    //     connect(_videoReceiver[1], &VideoReceiver::onStopComplete, this, [this](VideoReceiver::STATUS) {
    //         _videoStarted[1] = false;
    //         _startReceiver(1);
    //     });
    // }
#endif
    _updateSettings(0);
    _updateSettings(1);
    _updateSettings(2);

    if(isGStreamer()) {
        startVideo();
    } else {
        stopVideo();
    }

#endif
}

void VideoManager::_cleanupOldVideos()
{
#if defined(QGC_GST_STREAMING)
    //-- Only perform cleanup if storage limit is enabled
    if(!_videoSettings->enableStorageLimit()->rawValue().toBool()) {
        return;
    }
    QString savePath = qgcApp()->toolbox()->settingsManager()->appSettings()->videoSavePath();
    QDir videoDir = QDir(savePath);
    videoDir.setFilter(QDir::Files | QDir::Readable | QDir::NoSymLinks | QDir::Writable);
    videoDir.setSorting(QDir::Time);

    QStringList nameFilters;

    for(size_t i = 0; i < sizeof(kFileExtension) / sizeof(kFileExtension[0]); i += 1) {
        nameFilters << QString("*.") + kFileExtension[i];
    }

    videoDir.setNameFilters(nameFilters);
    //-- get the list of videos stored
    QFileInfoList vidList = videoDir.entryInfoList();
    if(!vidList.isEmpty()) {
        uint64_t total   = 0;
        //-- Settings are stored using MB
        uint64_t maxSize = _videoSettings->maxVideoSize()->rawValue().toUInt() * 1024 * 1024;
        //-- Compute total used storage
        for(int i = 0; i < vidList.size(); i++) {
            total += vidList[i].size();
        }
        //-- Remove old movies until max size is satisfied.
        while(total >= maxSize && !vidList.isEmpty()) {
            total -= vidList.last().size();
            qCDebug(VideoManagerLog) << "Removing old video file:" << vidList.last().filePath();
            QFile file (vidList.last().filePath());
            file.remove();
            vidList.removeLast();
        }
    }
#endif
}

//-----------------------------------------------------------------------------
void
VideoManager::startVideo()
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }

    if(!_videoSettings->streamEnabled()->rawValue().toBool() || !_videoSettings->streamConfigured()) {
        qCDebug(VideoManagerLog) << "Stream not enabled/configured";
        return;
    }

    _startReceiver(0);
    _startReceiver(1);
    _startReceiver(2);
}

//-----------------------------------------------------------------------------
void
VideoManager::stopVideo()
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }

    _stopReceiver(2);
    _stopReceiver(1);
    _stopReceiver(0);
}

void
VideoManager::startRecording(const QString& videoFile)
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#if defined(QGC_GST_STREAMING)
    if (!_videoReceiver[0]) {
        qgcApp()->showAppMessage(tr("Video receiver is not ready."));
        return;
    }

    const VideoReceiver::FILE_FORMAT fileFormat = static_cast<VideoReceiver::FILE_FORMAT>(_videoSettings->recordingFormat()->rawValue().toInt());

    if(fileFormat < VideoReceiver::FILE_FORMAT_MIN || fileFormat >= VideoReceiver::FILE_FORMAT_MAX) {
        qgcApp()->showAppMessage(tr("Invalid video format defined."));
        return;
    }
    QString ext = kFileExtension[fileFormat - VideoReceiver::FILE_FORMAT_MIN];

    //-- Disk usage maintenance
    _cleanupOldVideos();

    QString savePath = qgcApp()->toolbox()->settingsManager()->appSettings()->videoSavePath();

    if (savePath.isEmpty()) {
        qgcApp()->showAppMessage(tr("Unabled to record video. Video save path must be specified in Settings."));
        return;
    }

    _videoFile = savePath + "/"
            + (videoFile.isEmpty() ? QDateTime::currentDateTime().toString("yyyy-MM-dd_hh.mm.ss") : videoFile)
            + ".";
    QString videoFile2 = _videoFile + "2." + ext;
    QString videoFile3 = _videoFile + "3." + ext;
    _videoFile += ext;

    if (_videoReceiver[0] && _videoStarted[0]) {
        _videoReceiver[0]->startRecording(_videoFile, fileFormat);
    }
    if (_videoReceiver[1] && _videoStarted[1]) {
        _videoReceiver[1]->startRecording(videoFile2, fileFormat);
    }
    if (_videoReceiver[2] && _videoStarted[2]) {
        _videoReceiver[2]->startRecording(videoFile3, fileFormat);
    }

#else
    Q_UNUSED(videoFile)
#endif
}

void
VideoManager::stopRecording()
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#if defined(QGC_GST_STREAMING)

    for (int i = 0; i < 3; i++) {
        if (_videoReceiver[i]) {
            _videoReceiver[i]->stopRecording();
        }
    }
#endif
}

void
VideoManager::grabImage(const QString& imageFile)
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#if defined(QGC_GST_STREAMING)
    if (!_videoReceiver[0]) {
        return;
    }

    if (imageFile.isEmpty()) {
        _imageFile = qgcApp()->toolbox()->settingsManager()->appSettings()->photoSavePath();
        _imageFile += + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh.mm.ss.zzz") + ".jpg";
    } else {
        _imageFile = imageFile;
    }

    emit imageFileChanged();
    _videoReceiver[0]->takeScreenshot(_imageFile);

#else
    Q_UNUSED(imageFile)
#endif
}

void
VideoManager::secondaryGrabImage(const QString& secondaryImageFile)
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#if defined(QGC_GST_STREAMING)
    if (!_videoReceiver[1]) {
        return;
    }

    if (secondaryImageFile.isEmpty()) {
        _secondaryImageFile = qgcApp()->toolbox()->settingsManager()->appSettings()->photoSavePath();
        _secondaryImageFile += + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh.mm.ss.zzz") + ".jpg";
    } else {
        _secondaryImageFile = secondaryImageFile;
    }

    emit secondaryImageFileChanged();
    _videoReceiver[1]->takeScreenshot(_secondaryImageFile);


#else
    Q_UNUSED(imageFile)
#endif
}

void
VideoManager::tertiaryGrabImage(const QString& tertiaryImageFile)
{
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#if defined(QGC_GST_STREAMING)
    if (!_videoReceiver[2]) {
        return;
    }

    if (tertiaryImageFile.isEmpty()) {
        _tertiaryImageFile = qgcApp()->toolbox()->settingsManager()->appSettings()->photoSavePath();
        _tertiaryImageFile += + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh.mm.ss.zzz") + ".jpg";
    } else {
        _tertiaryImageFile = tertiaryImageFile;
    }

    emit tertiaryImageFileChanged();
    _videoReceiver[2]->takeScreenshot(_tertiaryImageFile);


#else
    Q_UNUSED(imageFile)
#endif
}

//-----------------------------------------------------------------------------
double VideoManager::aspectRatio()
{
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->currentStreamInstance();
        if(pInfo) {
            qCDebug(VideoManagerLog) << "Primary AR: " << pInfo->aspectRatio();
            return pInfo->aspectRatio();
        }
    }
    // FIXME: AV: use _videoReceiver->videoSize() to calculate AR (if AR is not specified in the settings?)
    return _videoSettings->aspectRatio()->rawValue().toDouble();
}

//-----------------------------------------------------------------------------
double VideoManager::thermalAspectRatio()
{
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->thermalStreamInstance();
        if(pInfo) {
            qCDebug(VideoManagerLog) << "Thermal AR: " << pInfo->aspectRatio();
            return pInfo->aspectRatio();
        }
    }
    return 1.0;
}

//-----------------------------------------------------------------------------
double VideoManager::hfov()
{
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->currentStreamInstance();
        if(pInfo) {
            return pInfo->hfov();
        }
    }
    return 1.0;
}

//-----------------------------------------------------------------------------
double VideoManager::thermalHfov()
{
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->thermalStreamInstance();
        if(pInfo) {
            return pInfo->aspectRatio();
        }
    }
    return _videoSettings->aspectRatio()->rawValue().toDouble();
}

//-----------------------------------------------------------------------------
bool
VideoManager::hasThermal()
{
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->thermalStreamInstance();
        if(pInfo) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
QString
VideoManager::imageFile()
{
    return _imageFile;
}

//-----------------------------------------------------------------------------
QString
VideoManager::secondaryImageFile()
{
    return _secondaryImageFile;
}

//-----------------------------------------------------------------------------
QString
VideoManager::tertiaryImageFile()
{
    return _tertiaryImageFile;
}

//-----------------------------------------------------------------------------
bool
VideoManager::autoStreamConfigured()
{
#if defined(QGC_GST_STREAMING)
    if(_activeVehicle && _activeVehicle->cameraManager()) {
        QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->currentStreamInstance();
        if(pInfo) {
            return !pInfo->uri().isEmpty();
        }
    }
#endif
    return false;
}

//-----------------------------------------------------------------------------
void
VideoManager::_updateUVC()
{
#ifndef QGC_DISABLE_UVC
    QString oldUvcVideoSrcID = _uvcVideoSourceID;
    if (!hasVideo() || isGStreamer()) {
        _uvcVideoSourceID = "";
    } else {
        QString videoSource = _videoSettings->videoSource()->rawValue().toString();
        QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
        for (const QCameraInfo &cameraInfo : cameras) {
            if (cameraInfo.description() == videoSource) {
                _uvcVideoSourceID = cameraInfo.deviceName();
                qCDebug(VideoManagerLog)
                    << "Found USB source:" << _uvcVideoSourceID << " Name:" << videoSource;
                break;
            }
        }
    }

    if (oldUvcVideoSrcID != _uvcVideoSourceID) {
        qCDebug(VideoManagerLog) << "UVC changed from [" << oldUvcVideoSrcID << "] to [" << _uvcVideoSourceID << "]";
        emit uvcVideoSourceIDChanged();
        emit isUvcChanged();
    }

#endif
}

//-----------------------------------------------------------------------------
void
VideoManager::_videoSourceChanged()
{
    _updateUVC();
    _updateSettings(0);
    emit hasVideoChanged();
    emit isGStreamerChanged();
    emit isUvcChanged();
    emit isAutoStreamChanged();
    if (hasVideo()) {
        _restartVideo(0);
    } else {
        stopVideo();
    }
}

//-----------------------------------------------------------------------------
void
VideoManager::_udpPortChanged()
{
    _restartVideo(0);
}

//-----------------------------------------------------------------------------
void
VideoManager::_rtspUrlChanged()
{
    _restartVideo(0);
}

//-----------------------------------------------------------------------------
void
VideoManager::_tcpUrlChanged()
{
    _restartVideo(0);
}

//-----------------------------------------------------------------------------
void
VideoManager::_lowLatencyModeChanged()
{
    _restartAllVideos();
}

//-----------------------------------------------------------------------------
bool
VideoManager::hasVideo()
{
    if(autoStreamConfigured()) {
        return true;
    }
    QString videoSource = _videoSettings->videoSource()->rawValue().toString();
    return !videoSource.isEmpty() && videoSource != VideoSettings::videoSourceNoVideo && videoSource != VideoSettings::videoDisabled;
}

//-----------------------------------------------------------------------------
bool
VideoManager::isGStreamer()
{
#if defined(QGC_GST_STREAMING)
    QString videoSource = _videoSettings->videoSource()->rawValue().toString();
    return videoSource == VideoSettings::videoSourceUDPH264 ||
            videoSource == VideoSettings::videoSourceUDPH265 ||
            videoSource == VideoSettings::videoSourceRTSP ||
            videoSource == VideoSettings::videoSourceTCP ||
            videoSource == VideoSettings::videoSourceMPEGTS ||
            videoSource == VideoSettings::videoSource3DRSolo ||
            videoSource == VideoSettings::videoSourceParrotDiscovery ||
            videoSource == VideoSettings::videoSourceYuneecMantisG ||
            videoSource == VideoSettings::videoSourceHerelinkAirUnit ||
            videoSource == VideoSettings::videoSourceHerelinkHotspot ||
            autoStreamConfigured();
#else
    return false;
#endif
}

bool
VideoManager::isUvc()
{
#ifndef QGC_DISABLE_UVC
    auto isUvc = hasVideo() && !_uvcVideoSourceID.isEmpty();
    qCDebug(VideoManagerLog) << "Is Video source UVC: " << (isUvc ? "yes" : "no");
    return isUvc;
#else
    return false;
#endif
}

//-----------------------------------------------------------------------------
#ifndef QGC_DISABLE_UVC
bool
VideoManager::uvcEnabled()
{
    return QCameraInfo::availableCameras().count() > 0;
}
#endif

//-----------------------------------------------------------------------------
void
VideoManager::setfullScreen(bool f)
{
    if(f) {
        //-- No can do if no vehicle or connection lost
        if(!_activeVehicle || _activeVehicle->vehicleLinkManager()->communicationLost()) {
            f = false;
        }
    }
    _fullScreen = f;
    emit fullScreenChanged();
}

//-----------------------------------------------------------------------------
void
VideoManager::_initVideo()
{
#if defined(QGC_GST_STREAMING)
    QQuickWindow* root = qgcApp()->mainRootWindow();

    if (root == nullptr) {
        qCDebug(VideoManagerLog) << "mainRootWindow() failed. No root window";
        return;
    }

    QQuickItem* widget = root->findChild<QQuickItem*>("videoContent");

    if (widget != nullptr && _videoReceiver[0] != nullptr) {
        _videoSink[0] = qgcApp()->toolbox()->corePlugin()->createVideoSink(this, widget);
        if (_videoSink[0] != nullptr) {
            if (_videoStarted[0]) {
                _videoReceiver[0]->startDecoding(_videoSink[0]);
            }
        } else {
            qCDebug(VideoManagerLog) << "createVideoSink() failed";
        }
    } else {
        qCDebug(VideoManagerLog) << "video receiver disabled";
    }

    widget = root->findChild<QQuickItem*>("secondVideoContent");

    if (widget != nullptr && _videoReceiver[1] != nullptr) {
        _videoSink[1] = qgcApp()->toolbox()->corePlugin()->createVideoSink(this, widget);
        if (_videoSink[1] != nullptr) {
            if (_videoStarted[1]) {
                _videoReceiver[1]->startDecoding(_videoSink[1]);
            }
        } else {
            qCDebug(VideoManagerLog) << "createVideoSink() failed";
        }
    } else {
        qCDebug(VideoManagerLog) << "second video receiver disabled";
    }

    widget = root->findChild<QQuickItem*>("thirdVideoContent");

    if (widget != nullptr && _videoReceiver[2] != nullptr) {
        _videoSink[2] = qgcApp()->toolbox()->corePlugin()->createVideoSink(this, widget);
        if (_videoSink[2] != nullptr) {
            if (_videoStarted[2]) {
                _videoReceiver[2]->startDecoding(_videoSink[2]);
            }
        } else {
            qCDebug(VideoManagerLog) << "createVideoSink() failed";
        }
    } else {
        qCDebug(VideoManagerLog) << "third video receiver disabled";
    }
#endif
}

//-----------------------------------------------------------------------------
bool
VideoManager::_updateSettings(unsigned id)
{
    if(!_videoSettings)
        return false;

    const bool lowLatencyStreaming  =_videoSettings->lowLatencyMode()->rawValue().toBool();

    bool settingsChanged = _lowLatencyStreaming[id] != lowLatencyStreaming;

    _lowLatencyStreaming[id] = lowLatencyStreaming;

    //-- Auto discovery

    // if(_activeVehicle && _activeVehicle->cameraManager()) {
    //     QGCVideoStreamInfo* pInfo = _activeVehicle->cameraManager()->currentStreamInstance();
    //     if(pInfo) {
    //         if (id == 0) {
    //             qCDebug(VideoManagerLog) << "Configure primary stream:" << pInfo->uri();
    //             switch(pInfo->type()) {
    //                 case VIDEO_STREAM_TYPE_RTSP:
    //                     if ((settingsChanged |= _updateVideoUri(id, pInfo->uri()))) {
    //                         _toolbox->settingsManager()->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceRTSP);
    //                     }
    //                     break;
    //                 case VIDEO_STREAM_TYPE_TCP_MPEG:
    //                     if ((settingsChanged |= _updateVideoUri(id, pInfo->uri()))) {
    //                         _toolbox->settingsManager()->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceTCP);
    //                     }
    //                     break;
    //                 case VIDEO_STREAM_TYPE_RTPUDP:
    //                     if ((settingsChanged |= _updateVideoUri(
    //                                     id,
    //                                     pInfo->uri().contains("udp://")
    //                                         ? pInfo->uri() // Specced case
    //                                         : QStringLiteral("udp://0.0.0.0:%1").arg(pInfo->uri())))) {
    //                         _toolbox->settingsManager()->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceUDPH264);
    //                     }
    //                     break;
    //                 case VIDEO_STREAM_TYPE_MPEG_TS_H264:
    //                     if ((settingsChanged |= _updateVideoUri(id, QStringLiteral("mpegts://0.0.0.0:%1").arg(pInfo->uri())))) {
    //                         _toolbox->settingsManager()->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceMPEGTS);
    //                     }
    //                     break;
    //                 default:
    //                     settingsChanged |= _updateVideoUri(id, pInfo->uri());
    //                     break;
    //             }
    //         }
    //         else if (id == 1) { //-- Thermal stream (if any)
    //             QGCVideoStreamInfo* pTinfo = _activeVehicle->cameraManager()->thermalStreamInstance();
    //             if (pTinfo) {
    //                 qCDebug(VideoManagerLog) << "Configure secondary stream:" << pTinfo->uri();
    //                 switch(pTinfo->type()) {
    //                     case VIDEO_STREAM_TYPE_RTSP:
    //                     case VIDEO_STREAM_TYPE_TCP_MPEG:
    //                         settingsChanged |= _updateVideoUri(id, pTinfo->uri());
    //                         break;
    //                     case VIDEO_STREAM_TYPE_RTPUDP:
    //                         settingsChanged |= _updateVideoUri(id, QStringLiteral("udp://0.0.0.0:%1").arg(pTinfo->uri()));
    //                         break;
    //                     case VIDEO_STREAM_TYPE_MPEG_TS_H264:
    //                         settingsChanged |= _updateVideoUri(id, QStringLiteral("mpegts://0.0.0.0:%1").arg(pTinfo->uri()));
    //                         break;
    //                     default:
    //                         settingsChanged |= _updateVideoUri(id, pTinfo->uri());
    //                         break;
    //                 }
    //             }
    //         }
    //         return settingsChanged;
    //     }
    // }

    _toolbox->settingsManager()->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceRTSP);
    // if (!_secondaryStream)
    // {
        settingsChanged |= _updateVideoUri(0, QStringLiteral("rtsp://192.168.144.53/stream1"));
        settingsChanged |= _updateVideoUri(1, QStringLiteral("rtsp://192.168.144.54/stream1"));
        settingsChanged |= _updateVideoUri(2, QStringLiteral("rtsp://192.168.144.55/stream1"));
    // }
    // else
    // {
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("rtsp://192.168.144.54/stream1"));
    //     settingsChanged |= _updateVideoUri(1, QStringLiteral("rtsp://192.168.144.53/stream1"));
    // }
    // settingsChanged |= _updateVideoUri(1, QStringLiteral("rtsp://192.168.144.54/stream1"));


    // QString source = _videoSettings->videoSource()->rawValue().toString();
    // if (source == VideoSettings::videoSourceUDPH264)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("udp://0.0.0.0:%1").arg(_videoSettings->udpPort()->rawValue().toInt()));
    // else if (source == VideoSettings::videoSourceUDPH265)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("udp265://0.0.0.0:%1").arg(_videoSettings->udpPort()->rawValue().toInt()));
    // else if (source == VideoSettings::videoSourceMPEGTS)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("mpegts://0.0.0.0:%1").arg(_videoSettings->udpPort()->rawValue().toInt()));
    // else if (source == VideoSettings::videoSourceRTSP)
    //     settingsChanged |= _updateVideoUri(0, _videoSettings->rtspUrl()->rawValue().toString());
    // else if (source == VideoSettings::videoSourceTCP)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("tcp://%1").arg(_videoSettings->tcpUrl()->rawValue().toString()));
    // else if (source == VideoSettings::videoSource3DRSolo)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("udp://0.0.0.0:5600"));
    // else if (source == VideoSettings::videoSourceParrotDiscovery)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("udp://0.0.0.0:8888"));
    // else if (source == VideoSettings::videoSourceYuneecMantisG)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("rtsp://192.168.42.1:554/live"));
    // else if (source == VideoSettings::videoSourceHerelinkAirUnit)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("rtsp://192.168.0.10:8554/H264Video"));
    // else if (source == VideoSettings::videoSourceHerelinkHotspot)
    //     settingsChanged |= _updateVideoUri(0, QStringLiteral("rtsp://192.168.43.1:8554/fpv_stream"));
    // else if (source == VideoSettings::videoDisabled || source == VideoSettings::videoSourceNoVideo)
    //     settingsChanged |= _updateVideoUri(0, "");
    // else {
    //     settingsChanged |= _updateVideoUri(0, "");
    //     if (!isUvc()) {
    //         qCCritical(VideoManagerLog)
    //             << "Video source URI \"" << source << "\" is not supported. Please add support!";
    //     }
    // }

    return settingsChanged;
}

//-----------------------------------------------------------------------------
bool
VideoManager::_updateVideoUri(unsigned id, const QString& uri)
{
#if defined(QGC_GST_TAISYNC_ENABLED) && (defined(__android__) || defined(__ios__))
    //-- Taisync on iOS or Android sends a raw h.264 stream
    if (isTaisync()) {
        if (id == 0) {
            return _updateVideoUri(0, QString("tsusb://0.0.0.0:%1").arg(TAISYNC_VIDEO_UDP_PORT));
        } if (id == 1) {
            // FIXME: AV: TAISYNC_VIDEO_UDP_PORT is used by video stream, thermal stream should go via its own proxy
            if (!_videoUri[1].isEmpty()) {
                _videoUri[1].clear();
                return true;
            } else {
                return false;
            }
        }
    }
#endif
    if (uri == _videoUri[id]) {
        return false;
    }

    _videoUri[id] = uri;

    return true;
}

//-----------------------------------------------------------------------------
void
VideoManager::_restartVideo(unsigned id)
{
#if !defined(QGC_GST_STREAMING)
    Q_UNUSED(id);
#endif

    if (qgcApp()->runningUnitTests()) {
        return;
    }

#if defined(QGC_GST_STREAMING)
    bool oldLowLatencyStreaming = _lowLatencyStreaming[id];
    QString oldUri = _videoUri[id];
    _updateSettings(id);
    bool newLowLatencyStreaming = _lowLatencyStreaming[id];
    QString newUri = _videoUri[id];
    qCDebug(VideoManagerLog) << "New Video URI " << newUri;
    // FIXME: AV: use _updateSettings() result to check if settings were changed
    if (_videoStarted[id] && oldUri == newUri && oldLowLatencyStreaming == newLowLatencyStreaming) {
        qCDebug(VideoManagerLog) << "No sense to restart video streaming, skipped"  << id;
        return;
    }

    qCDebug(VideoManagerLog) << "Restart video streaming"  << id;

    if (_videoStarted[id]) {
        _stopReceiver(id);
    } else {
        _startReceiver(id);
    }
#endif
}

//-----------------------------------------------------------------------------
void
VideoManager::_restartAllVideos()
{
    _restartVideo(0);
    _restartVideo(1);
    _restartVideo(2);
}

//----------------------------------------------------------------------------------------
void
VideoManager::_startReceiver(unsigned id)
{
#if defined(QGC_GST_STREAMING)
    const QString source = _videoSettings->videoSource()->rawValue().toString();
    const unsigned rtsptimeout = _videoSettings->rtspTimeout()->rawValue().toUInt();
    /* The gstreamer rtsp source will switch to tcp if udp is not available after 5 seconds.
       So we should allow for some negotiation time for rtsp */
    const unsigned timeout = (source == VideoSettings::videoSourceRTSP ? rtsptimeout : 2 );

    if (id > 2) {
        qCDebug(VideoManagerLog) << "Unsupported receiver id" << id;
    } else if (_videoReceiver[id] != nullptr/* && _videoSink[id] != nullptr*/) {
        if (!_videoUri[id].isEmpty()) {
            _videoReceiver[id]->start(_videoUri[id], timeout, _lowLatencyStreaming[id] ? -1 : 0);
        }
    }
#else
    Q_UNUSED(id);
#endif
}

//----------------------------------------------------------------------------------------
void
VideoManager::_stopReceiver(unsigned id)
{
#if defined(QGC_GST_STREAMING)
    if (id > 2) {
        qCDebug(VideoManagerLog) << "Unsupported receiver id" << id;
    } else if (_videoReceiver[id] != nullptr) {
        _videoReceiver[id]->stop();
    }
#else
    Q_UNUSED(id);
#endif
}

//----------------------------------------------------------------------------------------
void
VideoManager::_setActiveVehicle(Vehicle* vehicle)
{
    if(_activeVehicle) {
        disconnect(_activeVehicle->vehicleLinkManager(), &VehicleLinkManager::communicationLostChanged, this, &VideoManager::_communicationLostChanged);
        if(_activeVehicle->cameraManager()) {
            QGCCameraControl* pCamera = _activeVehicle->cameraManager()->currentCameraInstance();
            if(pCamera) {
                pCamera->stopStream();
            }
            disconnect(_activeVehicle->cameraManager(), &QGCCameraManager::streamChanged, this, &VideoManager::_restartAllVideos);
        }
    }
    _activeVehicle = vehicle;
    if(_activeVehicle) {
        connect(_activeVehicle->vehicleLinkManager(), &VehicleLinkManager::communicationLostChanged, this, &VideoManager::_communicationLostChanged);
        if(_activeVehicle->cameraManager()) {
            connect(_activeVehicle->cameraManager(), &QGCCameraManager::streamChanged, this, &VideoManager::_restartAllVideos);
            QGCCameraControl* pCamera = _activeVehicle->cameraManager()->currentCameraInstance();
            if(pCamera) {
                pCamera->resumeStream();
            }
        }
    } else {
        //-- Disable full screen video if vehicle is gone
        setfullScreen(false);
    }
    emit autoStreamConfiguredChanged();
    _restartAllVideos();
}

//----------------------------------------------------------------------------------------
void
VideoManager::_communicationLostChanged(bool connectionLost)
{
    if(connectionLost) {
        //-- Disable full screen video if connection is lost
        setfullScreen(false);
    }
}

//----------------------------------------------------------------------------------------
void
VideoManager::_aspectRatioChanged()
{
    emit aspectRatioChanged();
}

//----------------------------------------------------------------------------------------
void
VideoManager::toggleStreams()
{
    QQuickWindow* root = qgcApp()->mainRootWindow();
    if (_currentStream == 0)
    {
        QQuickItem* widget = root->findChild<QQuickItem*>("videoContent");
        widget->setVisible(false);
        widget = root->findChild<QQuickItem*>("secondVideoContent");
        widget->setVisible(true);
        widget = root->findChild<QQuickItem*>("thirdVideoContent");
        widget->setVisible(false);
        _currentStream = 1;
    }
    else if (_currentStream == 1)
    {
        if (_tertiaryStreamAvailable)
        {
            QQuickItem* widget = root->findChild<QQuickItem*>("videoContent");
            widget->setVisible(false);
            widget = root->findChild<QQuickItem*>("secondVideoContent");
            widget->setVisible(false);
            widget = root->findChild<QQuickItem*>("thirdVideoContent");
            widget->setVisible(true);
            _currentStream = 2;
        }
        else
        {
            QQuickItem* widget = root->findChild<QQuickItem*>("videoContent");
            widget->setVisible(true);
            widget = root->findChild<QQuickItem*>("secondVideoContent");
            widget->setVisible(false);
            widget = root->findChild<QQuickItem*>("thirdVideoContent");
            widget->setVisible(false);
            _currentStream = 0;
        }
    }
    else if (_currentStream == 2)
    {
        QQuickItem* widget = root->findChild<QQuickItem*>("videoContent");
        widget->setVisible(true);
        widget = root->findChild<QQuickItem*>("secondVideoContent");
        widget->setVisible(false);
        widget = root->findChild<QQuickItem*>("thirdVideoContent");
        widget->setVisible(false);
        _currentStream = 0;
    }

    // _restartAllVideos();
}

//----------------------------------------------------------------------------------------
void
VideoManager::checkForTertiaryStream(bool* connected)
{
    while (true)
    {
        QTcpSocket socket;
        socket.connectToHost("192.168.144.55", 80);
        if (socket.waitForConnected(1000)) { // 1 second timeout
            if (!*connected)
            {
                _restartVideo(2);
            }
            *connected = true;
        } else {
            *connected = false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

//----------------------------------------------------------------------------------------
std::string
VideoManager::toExifString(double d, bool bLat)
{
    const char* NS = d >= 0.0 ? "N" : "S";
    const char* EW = d >= 0.0 ? "E" : "W";
    const char* NSEW = bLat ? NS : EW;
    if (d < 0)
        d = -d;
    auto deg = static_cast<int>(d);
    d -= deg;
    d *= 60;
    auto min = static_cast<int>(d);
    d -= min;
    d *= 60 * 100;
    int sec = static_cast<int>(d);

    char result[200];
    snprintf(result, sizeof(result), "%d/1,%d/1,%d/100", deg, min, sec);
    return result;
}

//----------------------------------------------------------------------------------------
std::string
VideoManager::toExifAltString(double d)
{
    char result[200];
    d *= 100;
    snprintf(result, sizeof(result), "%d/100", abs(static_cast<int>(d)));
    return result;
}

//----------------------------------------------------------------------------------------
void
VideoManager::writeEXIFDataToFile(QString path)
{
#if defined(__android__)

    QAndroidJniObject exifObject("android/media/ExifInterface", "(Ljava/lang/String;)V",
                                 QAndroidJniObject::fromString(path).object<jstring>());

    if (exifObject.isValid()) {

        QDateTime current = QDateTime::currentDateTime();
        QString datetime = current.toString("yyyy:MM:dd HH:mm:ss");

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("Make").object<jstring>(),
                                    QAndroidJniObject::fromString("SPS Automation").object<jstring>());

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("Model").object<jstring>(),
                                    QAndroidJniObject::fromString("AC-16").object<jstring>());

        exifObject.callMethod<void>("setAttribute", 
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("DateTime").object<jstring>(),
                                    QAndroidJniObject::fromString(datetime).object<jstring>());

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSLatitude").object<jstring>(),
                                    QAndroidJniObject::fromString(toExifString(_activeVehicle->latitude(), true).c_str()).object<jstring>());

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSLongitude").object<jstring>(),
                                    QAndroidJniObject::fromString(toExifString(_activeVehicle->longitude(), false).c_str()).object<jstring>());

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSAltitude").object<jstring>(),
                                    QAndroidJniObject::fromString(toExifAltString(_activeVehicle->altitudeAMSL()->rawValue().toDouble()).c_str()).object<jstring>());

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSAltitudeRef").object<jstring>(),
                                    QAndroidJniObject::fromString(_activeVehicle->altitudeAMSL()->rawValue().toDouble() < 0.0 ? "1" : "0").object<jstring>());
        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSLatitudeRef").object<jstring>(),
                                    QAndroidJniObject::fromString(_activeVehicle->latitude() > 0 ? "N" : "S").object<jstring>());
        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSLongitudeRef").object<jstring>(),
                                    QAndroidJniObject::fromString(_activeVehicle->longitude() > 0 ? "E" : "W").object<jstring>());

        char headingString[200];
        snprintf(headingString, sizeof(headingString), "%d/100", abs(static_cast<int>(_activeVehicle->heading()->rawValue().toDouble() * 100)));

        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSImgDirection").object<jstring>(),
                                    QAndroidJniObject::fromString(headingString).object<jstring>());
        exifObject.callMethod<void>("setAttribute",
                                    "(Ljava/lang/String;Ljava/lang/String;)V",
                                    QAndroidJniObject::fromString("GPSImgDirectionRef").object<jstring>(),
                                    QAndroidJniObject::fromString("T").object<jstring>());

        exifObject.callMethod<void>("saveAttributes", "()V");
    }
#endif
}
