/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#ifndef SPRAYLOGDOWNLOADCONTROLLER_H
#define SPRAYLOGDOWNLOADCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QAbstractListModel>
#include <QLocale>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <algorithm>

#include "UASInterface.h"
#include "AutoPilotPlugin.h"

class  MultiVehicleManager;
class  UASInterface;
class  Vehicle;
class  SprayLogEntry;

Q_DECLARE_LOGGING_CATEGORY(SprayLogDownloadLog)


//-----------------------------------------------------------------------------
class SprayLogModel : public QAbstractListModel
{
    Q_OBJECT
public:

    enum SprayLogModelRoles {
        ObjectRole = Qt::UserRole + 1
    };

    SprayLogModel(QObject *parent = nullptr);

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_INVOKABLE     SprayLogEntry*  get(int index);

    int             count           (void) const;
    void            append          (SprayLogEntry* entry);
    void            clear           (void);
    SprayLogEntry*  operator[]      (int i);

    int             rowCount        (const QModelIndex & parent = QModelIndex()) const;
    QVariant        data            (const QModelIndex & index, int role = Qt::DisplayRole) const;
    void            sort_by_id      (void);

signals:
    void            countChanged    ();

protected:
    QHash<int, QByteArray> roleNames() const;
private:
    QList<SprayLogEntry*> _logEntries;
};


//-----------------------------------------------------------------------------
class SprayLogEntry : public QObject {
    Q_OBJECT

public:
    SprayLogEntry(QString logId, const QDateTime& dateTime = QDateTime(), uint logSize = 0);

    Q_PROPERTY(QString      id              READ id             WRITE setId             NOTIFY idChanged)
    Q_PROPERTY(QDateTime    time            READ time           WRITE setTime           NOTIFY timeChanged)
    Q_PROPERTY(uint         size            READ size           WRITE setSize           NOTIFY sizeChanged)
    Q_PROPERTY(QString      sizeStr         READ sizeStr                                NOTIFY sizeChanged)
    Q_PROPERTY(bool         received        READ received       WRITE setReceived       NOTIFY receivedChanged)
    Q_PROPERTY(bool         selected        READ selected       WRITE setSelected       NOTIFY selectedChanged)
    Q_PROPERTY(QString      status          READ status         WRITE setStatus         NOTIFY statusChanged)
    Q_PROPERTY(bool         downloading     READ downloading    WRITE setDownloading    NOTIFY statusChanged)
    Q_PROPERTY(bool         queuedDownload  READ queuedDownload WRITE setQueued         NOTIFY statusChanged)

    QString     id              () { return _logID; }
    uint        size            () { return _logSize; }
    QString     sizeStr         () ;
    QDateTime   time            () { return _logTimeUTC; }
    bool        received        () { return _received; }
    bool        selected        () { return _selected; }
    QString     status          () { return _status; }
    bool        downloading     () { return _downloading; }
    bool        queuedDownload  () { return _queuedDownload; }
    bool        isDownloaded    () { return _isDownloaded; }

    int         sort_id         () const { return _logID.toInt(); }

    void        setId           (QString id_)       { _logID = id_;         emit idChanged(); }
    void        setSize         (uint size_)        { _logSize = size_;     emit sizeChanged(); }
    void        setTime         (QDateTime date_)   { _logTimeUTC = date_;  emit timeChanged(); }
    void        setReceived     (bool rec_)         { _received = rec_;     emit receivedChanged(); }
    void        setSelected     (bool sel_)         { _selected = sel_;     emit selectedChanged(); }
    void        setStatus       (QString stat_)     { _status = stat_;      emit statusChanged(); }
    void        setDownloading  (bool download_)    { _downloading = download_; emit downloadingChanged(); }
    void        setQueued       (bool queue_)       { _queuedDownload = queue_; emit queuedChanged(); }
    void        setDownloaded   (bool downloaded_)  { _isDownloaded = downloaded_; }

signals:
    void        idChanged       ();
    void        timeChanged     ();
    void        sizeChanged     ();
    void        receivedChanged ();
    void        selectedChanged ();
    void        statusChanged   ();
    void        downloadingChanged();
    void        queuedChanged   ();

private:
    QString     _logID;
    uint        _logSize;
    QDateTime   _logTimeUTC;
    bool        _received;
    bool        _selected;
    QString     _status;
    bool        _downloading;
    bool        _queuedDownload;
    bool        _isDownloaded;
};


//-----------------------------------------------------------------------------
class SprayLogDownloadController : public QObject
{
    Q_OBJECT

public:
    SprayLogDownloadController(void);

    Q_PROPERTY(SprayLogModel* model         READ model              NOTIFY modelChanged)
    Q_PROPERTY(bool         requestingList  READ requestingList     NOTIFY requestingListChanged)
    Q_PROPERTY(bool         downloadingLogs READ downloadingLogs    NOTIFY downloadingLogsChanged)
    Q_PROPERTY(QGeoCoordinate mapCenter     READ mapCenter          WRITE setMapCenter          NOTIFY mapCenterChanged)
    Q_PROPERTY(double       mapZoomLevel    READ mapZoomLevel       WRITE setMapZoomLevel       NOTIFY mapZoomLevelChanged)
    Q_PROPERTY(QmlObjectListModel*  sprayStartPoints                     READ sprayStartPoints            CONSTANT)
    Q_PROPERTY(QmlObjectListModel*  sprayTrailPoints                     READ sprayTrialPoints            CONSTANT)
    Q_PROPERTY(bool loading                 READ loading            NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingComplete         READ loadingComplete    NOTIFY loadingCompleteChanged)


    SprayLogModel*      model               ()       { return &_logEntriesModel; }
    bool                requestingList      () const { return _requestingLogEntries; }
    bool                downloadingLogs     () const { return _downloadingLogs; }
    void                downloadToDirectory (const QString& dir);
    void                checkForDownloads   ();
    void                download            (SprayLogEntry* entry);
    void                setMapCenter        (QGeoCoordinate& coordinate);
    void                setMapZoomLevel     (double zoom);
    QmlObjectListModel* sprayStartPoints    () { return &_sprayStartPoints; }
    QmlObjectListModel* sprayTrialPoints    () { return &_sprayTrailPoints; }
    bool                loading             () { return _loading; }
    bool                loadingComplete     () { return _loaded; }


    static QGeoCoordinate   mapCenter       () {return _coord;}
    static double           mapZoomLevel    () {return _zoom;}

    Q_INVOKABLE void    refresh             ();
    Q_INVOKABLE void    erase               ();
    Q_INVOKABLE void    eraseAll            ();
    Q_INVOKABLE void    queueForDownload    ();
    Q_INVOKABLE void    addLogsToSprayMap   (int width, int height);
    Q_INVOKABLE void    addLogsToFlightMap  ();




signals:
    void requestingListChanged  ();
    void downloadingLogsChanged ();
    void modelChanged           ();
    void selectionChanged       ();
    void mapCenterChanged       (QGeoCoordinate mapCenter);
    void mapZoomLevelChanged    (double mapZoomLevel);
    void loadingChanged         ();
    void loadingCompleteChanged ();

private slots:
    void error              (QNetworkReply::NetworkError);
    void updateProgress     (qint64 read, qint64 total);
    void downloadFinished   ();

private:
    bool _entriesComplete   ();
    bool _chunkComplete     () const;
    bool _logComplete       () const;
    void _findMissingEntries();
    void _receivedAllEntries();
    void _receivedAllData   ();
    void _resetSelection    (bool canceled = false);
    void _findMissingData   ();
    void _requestLogList    (uint32_t start, uint32_t end);
    void _requestLogData    (uint16_t id, uint32_t offset, uint32_t count, int retryCount = 0);
    bool _prepareLogDownload();
    void _setDownloading    (bool active);
    void _calculateZoomAndCenter (int width, int height);
    void _calculateZoomLevel(int width, int height, QGeoCoordinate min_coords, QGeoCoordinate max_coords);
    void _calculateMapCenter();
    void _setLoading        (bool loading);
    void _setLoadingComplete(bool loaded);

    void _delete            (SprayLogEntry* entry);

    SprayLogEntry* _getNextQueued();
    SprayLogEntry* _getDownloading();

    UASInterface*       _uas;
    QTimer              _timer;
    SprayLogModel       _logEntriesModel;
    Vehicle*            _vehicle;
    bool                _requestingLogEntries;
    bool                _downloadingLogs;
    int                 _retries;
    int                 _apmOneBased;
    QString             _downloadPath;
    bool                _downloadInProgress = false;
    QNetworkReply*      _reply;
    bool                _awaiting_response = false;
    bool                _loading;
    bool                _loaded;

    static QGeoCoordinate   _coord;
    static double           _zoom;
    QmlObjectListModel      _sprayStartPoints;
    QmlObjectListModel      _sprayTrailPoints;
};

#endif
