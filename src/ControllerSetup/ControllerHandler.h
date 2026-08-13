/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#ifndef ControllerHandler_H
#define ControllerHandler_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QFileInfoList>
#include <QElapsedTimer>
#include <QDebug>
#include <QList>
#include <array>
#include <QVariantList>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <atomic>

#include <Siyi/unirc_sdk.h>
#include <Siyi/rcu_session.h>
#include <Siyi/fpv_upgrade_client.h>

/// Controller for ControllerHandler.qml.
class ControllerHandler : public QObject
{
    Q_OBJECT
public:
    ControllerHandler();
    ~ControllerHandler();

    Q_PROPERTY(QVariantList channels                READ channels               NOTIFY channelsChanged)
    Q_PROPERTY(QVariantList rawAnalog               READ rawAnalog              NOTIFY rawAnalogChanged)
    Q_PROPERTY(QVariantList channelMappings         READ channelMappings        NOTIFY channelMappingsChanged)
    Q_PROPERTY(QVariantList channelReverses         READ channelReverses        NOTIFY channelReversesChanged)
    Q_PROPERTY(qint8        flightChannel           READ flightChannel          NOTIFY flightChannelChanged)
    Q_PROPERTY(QVariantList inputList               READ inputList              NOTIFY inputListChanged)
    Q_PROPERTY(QVariantMap  buttonMap               READ buttonMap              NOTIFY buttonMapChanged)
    Q_PROPERTY(qint8        stickCalibrationState   READ stickCalibrationState  NOTIFY stickCalibrationStateChanged)
    Q_PROPERTY(qint8        dialCalibrationState    READ dialCalibrationState   NOTIFY dialCalibrationStateChanged)
    Q_PROPERTY(qint8        bindingStatus           READ bindingStatus          NOTIFY bindingStatusChanged)
    Q_PROPERTY(qint8        flightMode              READ flightMode             NOTIFY flightModeChanged)
    Q_PROPERTY(qint8        deadzone                READ deadzone               NOTIFY deadzoneChanged)

    // ---- FPV (image-transmission) firmware update ----
    Q_PROPERTY(QString      fpvVersion              READ fpvVersion             NOTIFY fpvVersionChanged)
    Q_PROPERTY(bool         fpvLinkUp               READ fpvLinkUp              NOTIFY fpvLinkUpChanged)
    Q_PROPERTY(qint8        fpvUpdateState          READ fpvUpdateState         NOTIFY fpvUpdateStateChanged)
    Q_PROPERTY(int          fpvUpdateProgress       READ fpvUpdateProgress      NOTIFY fpvUpdateProgressChanged)
    Q_PROPERTY(QString      fpvUpdateStatus         READ fpvUpdateStatus        NOTIFY fpvUpdateStatusChanged)
    Q_PROPERTY(QString      firmwareName            READ firmwareName           NOTIFY firmwareSelectionChanged)
    Q_PROPERTY(qint64       firmwareSize            READ firmwareSize           NOTIFY firmwareSelectionChanged)
    Q_PROPERTY(bool         firmwareSelected        READ firmwareSelected       NOTIFY firmwareSelectionChanged)
    Q_PROPERTY(bool         airUnitPresent          READ airUnitPresent         NOTIFY airUnitPresentChanged)
    Q_PROPERTY(QString      airVersion              READ airVersion             NOTIFY airUnitPresentChanged)

    QVariantList    channels                ()  {return _channels;}
    QVariantList    rawAnalog               ()  {return _rawAnalog;}
    QVariantList    channelMappings         ()  {return _channelMappings;}
    QVariantList    channelReverses         ()  {return _channelReverses;}
    qint8           flightChannel           ()  {return _flightChannel;}
    QVariantList    inputList               ()  {return _inputList;}
    QVariantMap     buttonMap               ()  {return _buttonMap;}
    qint8           stickCalibrationState   ()  {return _stickCalibrationState;}
    qint8           dialCalibrationState    ()  {return _dialCalibrationState;}
    qint8           bindingStatus           ()  {return _bindingStatus;}
    qint8           flightMode              ()  {return _flightMode;}
    qint8           deadzone                ()  {return _deadzone;}
    QString         fpvVersion              ()  {return _fpvVersion;}
    bool            fpvLinkUp               ()  {return _fpvLinkUp;}
    qint8           fpvUpdateState          ()  {return _fpvUpdateState;}
    int             fpvUpdateProgress       ()  {return _fpvUpdateProgress;}
    QString         fpvUpdateStatus         ()  {return _fpvUpdateStatus;}
    QString         firmwareName            ()  {return _firmwareName;}
    qint64          firmwareSize            ()  {return _firmwareSize;}
    bool            firmwareSelected        ()  {return !_firmwareUrl.isEmpty();}
    bool            airUnitPresent          ()  {return _airUnitPresent;}
    QString         airVersion              ()  {return _airVersion;}

    /// Mirrors FirmwareUpdate.qml's state handling. Keep in sync with the QML.
    enum FpvUpdateState {
        FpvIdle         = 0,
        FpvAnnouncing   = 1,
        FpvUploading    = 2,
        FpvVerifying    = 3,
        FpvCommitting   = 4,
        FpvRebooting    = 5,
        FpvSuccess      = 6,
        FpvFailed       = 7
    };
    Q_ENUM(FpvUpdateState)

    Q_INVOKABLE void setChannelReverse(int channel, bool reverse);
    void setChannelReverseThread(int channel, bool reverse);

    Q_INVOKABLE void shutdown_sdk();

    Q_INVOKABLE void callSetChannelMapping(qint8 channel, qint8 input);

    Q_INVOKABLE void toggleButtonMode(QString buttonName);
    Q_INVOKABLE void callStartStickCalibration();
    Q_INVOKABLE void callStartDialCalibration();
    Q_INVOKABLE void startBinding();
    Q_INVOKABLE void stopBinding();
    Q_INVOKABLE void callSetFlightMode(qint8 mode);
    Q_INVOKABLE void callSetFlightChannel(qint8 channelId);
    Q_INVOKABLE void callSetDeadzone(qint8 value);

    /// Probe the FPV module: refreshes fpvLinkUp + fpvVersion.
    Q_INVOKABLE void callRefreshFpvVersion();

    /// Remember a firmware file chosen in the file browser and publish its
    /// resolved name/size for display.
    Q_INVOKABLE void callSelectFirmware(const QString& url);

    /// Run the full ground-unit FPV firmware update on the file most recently
    /// passed to callSelectFirmware().
    Q_INVOKABLE void callUpgradeFpvFirmware(bool airUnit = false);

signals:
    void channelsChanged();
    void rawAnalogChanged();
    void channelMappingsChanged();
    void channelReversesChanged();
    void flightChannelChanged();
    void inputListChanged();
    void buttonMapChanged();
    void stickCalibrationStateChanged();
    void dialCalibrationStateChanged();
    void bindingStatusChanged();
    void flightModeChanged();
    void deadzoneChanged();
    void fpvVersionChanged();
    void fpvLinkUpChanged();
    void fpvUpdateStateChanged();
    void fpvUpdateProgressChanged();
    void fpvUpdateStatusChanged();
    void firmwareSelectionChanged();
    void airUnitPresentChanged();

private slots:

private:
    unircsdk::UniRcSdk  _siyiSdk;
    unircsdk::RcuSession _rcu;
    QVariantList _channels;
    QVariantList _rawAnalog;
    QVariantList _channelMappings;
    QVariantList _channelReverses;
    qint8 _flightChannel;
    qint8 _flightMode;
    QVariantList _inputList;
    QVariantMap _buttonMap;
    QList<QString> _buttonList;
    qint8 _stickCalibrationState = 0;
    qint8 _dialCalibrationState  = 0;
    qint8 _bindingStatus;
    qint8 _deadzone;
    std::atomic<bool> _haveFlightChannel{false};
    std::atomic<bool> _running{true};

    QString _fpvVersion;
    bool    _fpvLinkUp          = false;
    qint8   _fpvUpdateState     = FpvIdle;
    int     _fpvUpdateProgress  = 0;
    QString _fpvUpdateStatus;
    std::atomic<bool> _fpvUpdateBusy{false};

    QString _firmwareUrl;
    QString _firmwareName;
    qint64  _firmwareSize = 0;

    bool    _airUnitPresent = false;
    QString _airVersion;

    std::atomic<bool> _skyUpgradeReady{false};
    std::atomic<bool> _skyUpgradeReplied{false};

    std::pair<int, int> mapInputArrayToControlValues(qint8 inputArrayValue);
    void setChannelMapping(qint8 channel, qint8 input);
    void getAllButtonModes();
    void setupFrameListener();

    void startStickCalibration();
    void startDialCalibration();

    void joystickCalibrationCallback(int cb);
    void dialCalibrationCallback(int cb);

    void monitorBindingStatus();
    void startBindingThread();
    void stopBindingThread();

    void setAllChannelValues    (std::array<qint16,16> channels);
    void setRawAnalogValues     (std::array<qint16,12> values);
    void pullChannelMappings    ();
    void pullChannelReverse     ();
    void getFlightModeChannel   ();
    void pullFlightChannel      ();
    void setFlightModeChannel   (qint8 channelId);
    void getFlightMode          ();
    void setFlightMode          (qint8 mode);
    void setDeadzone            (qint8 value);

    void refreshFpvVersion      ();
    void upgradeFpvFirmware     (QString url, QString name, bool airUnit);
    /// Enable air-unit upgrade mode and wait for the unit to report ready.
    /// Mirrors UniGCS: SET(1) -> 12s settle -> poll GET up to 4x at 1s.
    bool prepareAirUnitForUpgrade();
    /// content:// cannot be fopen()'d, so stage it to a real file first.
    /// Returns the staged path, or an empty string on failure.
    QString stageFirmwareLocally(const QString& url, const QString& name, QString& errorOut);
    /// Marshals an FPV update state change onto the GUI thread.
    void postFpvState           (qint8 state, int progress, const QString& status);
};

#endif
