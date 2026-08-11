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

#include <Siyi/unirc_sdk.h>
#include <Siyi/rcu_session.h>

/// Controller for ControllerHandler.qml.
class ControllerHandler : public QObject
{
    Q_OBJECT
public:
    ControllerHandler();
    ~ControllerHandler();

    Q_PROPERTY(QVariantList channels                READ channels               NOTIFY channelsChanged)
    Q_PROPERTY(QVariantList channelMappings         READ channelMappings        NOTIFY channelMappingsChanged)
    Q_PROPERTY(QVariantList channelReverses         READ channelReverses        NOTIFY channelReversesChanged)
    Q_PROPERTY(qint8        flightChannel           READ flightChannel          NOTIFY flightChannelChanged)
    Q_PROPERTY(QVariantList inputList               READ inputList              NOTIFY inputListChanged)
    Q_PROPERTY(QVariantMap  buttonMap               READ buttonMap              NOTIFY buttonMapChanged)
    Q_PROPERTY(qint8        stickCalibrationState   READ stickCalibrationState  NOTIFY stickCalibrationStateChanged)
    Q_PROPERTY(qint8        dialCalibrationState    READ dialCalibrationState   NOTIFY dialCalibrationStateChanged)
    Q_PROPERTY(qint8        bindingStatus           READ bindingStatus          NOTIFY bindingStatusChanged)
    Q_PROPERTY(qint8        flightMode              READ flightMode             NOTIFY flightModeChanged)

    QVariantList    channels                ()  {return _channels;}
    QVariantList    channelMappings         ()  {return _channelMappings;}
    QVariantList    channelReverses         ()  {return _channelReverses;}
    qint8           flightChannel           ()  {return _flightChannel;}
    QVariantList    inputList               ()  {return _inputList;}
    QVariantMap     buttonMap               ()  {return _buttonMap;}
    qint8           stickCalibrationState   ()  {return _stickCalibrationState;}
    qint8           dialCalibrationState    ()  {return _dialCalibrationState;}
    qint8           bindingStatus           ()  {return _bindingStatus;}
    qint8           flightMode              ()  {return _flightMode;}

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

signals:
    void channelsChanged();
    void channelMappingsChanged();
    void channelReversesChanged();
    void flightChannelChanged();
    void inputListChanged();
    void buttonMapChanged();
    void stickCalibrationStateChanged();
    void dialCalibrationStateChanged();
    void bindingStatusChanged();
    void flightModeChanged();

private slots:

private:
    unircsdk::UniRcSdk  _siyiSdk;
    unircsdk::RcuSession _rcu;
    QVariantList _channels;
    QVariantList _channelMappings;
    QVariantList _channelReverses;
    qint8 _flightChannel;
    qint8 _flightMode;
    QVariantList _inputList;
    QVariantMap _buttonMap;
    QList<QString> _buttonList;
    qint8 _stickCalibrationState;
    qint8 _dialCalibrationState;
    qint8 _bindingStatus;

    std::pair<int, int> mapInputArrayToControlValues(qint8 inputArrayValue);
    void setChannelMapping(qint8 channel, qint8 input);
    void getAllButtonModes();
    void setupFrameListener();

    void startStickCalibration();
    void startDialCalibration();

    void joystickCalibrationCallback(int cb);
    void dialCalibrationCallback(int cb);

    void monitorBindingStatus();

    void setAllChannelValues    (std::array<qint16,16> channels);
    void pullChannelMappings    ();
    void pullChannelReverse     ();
    void getFlightModeChannel   ();
    void setFlightModeChannel   (qint8 channelId);
    void getFlightMode          ();
    void setFlightMode          (qint8 mode);
};

#endif
