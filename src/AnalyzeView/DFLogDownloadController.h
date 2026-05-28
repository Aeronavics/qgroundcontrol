/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#ifndef DFLogDownloadController_H
#define DFLogDownloadController_H

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
class  QGCDFLogEntry;

Q_DECLARE_LOGGING_CATEGORY(DFLogDownloadLog)


//-----------------------------------------------------------------------------
class QGCDFLogModel : public QAbstractListModel
{
    Q_OBJECT
public:

    enum QGCDFLogModelRoles {
        ObjectRole = Qt::UserRole + 1
    };

    QGCDFLogModel(QObject *parent = nullptr);

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_INVOKABLE     QGCDFLogEntry*  get(int index);

    int             count           (void) const;
    void            append          (QGCDFLogEntry* entry);
    void            clear           (void);
    QGCDFLogEntry*  operator[]      (int i);

    int             rowCount        (const QModelIndex & parent = QModelIndex()) const;
    QVariant        data            (const QModelIndex & index, int role = Qt::DisplayRole) const;
    void            sort_by_id      (void);

signals:
    void            countChanged    ();

protected:
    QHash<int, QByteArray> roleNames() const;
private:
    QList<QGCDFLogEntry*> _logEntries;
};


//-----------------------------------------------------------------------------
class QGCDFLogEntry : public QObject {
    Q_OBJECT

public:
    QGCDFLogEntry(QString logId, const QDateTime& dateTime = QDateTime(), uint logSize = 0);

    Q_PROPERTY(QString      id              READ id             WRITE setId)
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

    void        setId           (QString id_)       { _logID = id_; }
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
class DFLogDownloadController : public QObject
{
    Q_OBJECT

public:
    DFLogDownloadController(void);

    Q_PROPERTY(QGCDFLogModel* model           READ model              NOTIFY modelChanged)
    Q_PROPERTY(bool         requestingList  READ requestingList     NOTIFY requestingListChanged)
    Q_PROPERTY(bool         downloadingLogs READ downloadingLogs    NOTIFY downloadingLogsChanged)
    Q_PROPERTY(bool loading                 READ loading            NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingComplete         READ loadingComplete    NOTIFY loadingCompleteChanged)

    QGCDFLogModel*      model               ()       { return &_logEntriesModel; }
    bool                requestingList      () const { return _requestingLogEntries; }
    bool                downloadingLogs     () const { return _downloadingLogs; }
    void                downloadToDirectory (const QString& dir);
    void                checkForDownloads   ();
    void                download            (QGCDFLogEntry* entry);
    bool                loading             () { return _loading; }
    bool                loadingComplete     () { return _loaded; }

    Q_INVOKABLE void    refresh             ();
    Q_INVOKABLE void    erase               ();
    Q_INVOKABLE void    eraseAll            ();
    Q_INVOKABLE void    queueForDownload    ();

    

signals:
    void requestingListChanged  ();
    void downloadingLogsChanged ();
    void modelChanged           ();
    void selectionChanged       ();
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
    void _setLoading        (bool loading);
    void _setLoadingComplete(bool loaded);

    void _delete            (QGCDFLogEntry* entry);

    QGCDFLogEntry* _getNextQueued();
    QGCDFLogEntry* _getDownloading();

    UASInterface*       _uas;
    QTimer              _timer;
    QGCDFLogModel       _logEntriesModel;
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


};

#endif
