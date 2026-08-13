#include "ControllerHandler.h"

ControllerHandler::ControllerHandler()
{
    // Channel Listener for the mapped channel outputs
    _siyiSdk.setChannelListener([this](const std::array<qint16,16>& ch){
        std::array<qint16,16> copy = ch;
        QMetaObject::invokeMethod(this, [this, copy]{ setAllChannelValues(copy); }, Qt::QueuedConnection);
    });

    _siyiSdk.start();

    // The input calibration display
    _rcu.setAnalogListener([this](const std::array<qint16,12>& v){
        std::array<qint16,12> copy = v;
        QMetaObject::invokeMethod(this, [this, copy]{ setRawAnalogValues(copy); }, Qt::QueuedConnection);
    });

    _rcu.open();

    _channels = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    _rawAnalog = {0,0,0,0,0,0,0,0,0,0,0,0};
    _channelMappings = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
    _channelReverses = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
    _inputList = {"J1", "J2", "J3", "J4", "J5", "J6", "SA", "SB", "S1", "S2", "S3", "S4", "RD", "LD", "R1", "R2", "R3", "L1", "L2", "M1", "M2", "M3", "M4", "M5", "M6", " ", "RSSI", "--"};
    _buttonList = {"S1", "S2", "S3", "S4", "L1", "L2", "R1", "R2", "R3", "M1", "M2", "M3", "M4", "M5", "M6"};
    _buttonMap = {{"S1", 0}, {"S2", 0}, {"S3", 0}, {"S4", 0}, {"L1", 0}, {"L2", 0}, {"R1", 0}, {"R2", 0}, {"R3", 0}, {"M1", 0}, {"M2", 0}, {"M3", 0}, {"M4", 0}, {"M5", 0}, {"M6", 0}};
    // Placeholder until the real value arrives via getAllButtonModes()'
    _deadzone = 30;

    QtConcurrent::run(this, &ControllerHandler::setupFrameListener);
    QtConcurrent::run(this, &ControllerHandler::pullChannelMappings);
    QtConcurrent::run(this, &ControllerHandler::pullChannelReverse);
    QtConcurrent::run(this, &ControllerHandler::getAllButtonModes);
    QtConcurrent::run(this, &ControllerHandler::monitorBindingStatus);
}


ControllerHandler::~ControllerHandler()
{
    _siyiSdk.close();
    _rcu.close();
}


void ControllerHandler::setAllChannelValues(std::array<qint16,16> channels)
{
    // qDebug() << "Channel 1: " << channels[0];
    QVariantList tempList;
    tempList.reserve(channels.size());
    for (qint16 val : channels){
        tempList.append(val);
    }
    // _channels = QList(channels.begin(), channels.end());
    _channels = tempList;
    emit channelsChanged();
}


void ControllerHandler::setRawAnalogValues(std::array<qint16,12> values)
{
    QVariantList tempList;
    tempList.reserve(values.size());
    for (qint16 val : values){
        tempList.append(val);
    }
    _rawAnalog = tempList;
    emit rawAnalogChanged();
}


void ControllerHandler::pullChannelMappings()
{
    std::vector<unircsdk::UniRcSdk::Mapping> _tempChannelMappings;
    if (!_siyiSdk.getAllChannelMappings(_tempChannelMappings))
    {
        qDebug() << "Failed to pull channel mappings";
        pullChannelMappings();
        return;
    }

    QVariantList tempList;
    tempList.reserve(_tempChannelMappings.size());
    for (unircsdk::UniRcSdk::Mapping val : _tempChannelMappings){
        tempList.append(val.name().c_str());
    }

    _channelMappings = tempList;
    emit channelMappingsChanged();
}


void ControllerHandler::pullChannelReverse()
{
    std::vector<int> _tempChannelReverses;
    if (!_siyiSdk.getAllReverse(_tempChannelReverses))
    {
        qDebug() << "Failed to pull channel reverse";
        pullChannelReverse();
        return;
    }

    QVariantList tempList;
    tempList.reserve(_tempChannelReverses.size());
    for (int val : _tempChannelReverses)
    {
        tempList.append(val == 1 ? false : true);
    }
    _channelReverses = tempList;
    emit channelReversesChanged();
}

void ControllerHandler::setChannelReverse(int channel, bool reverse)
{
    QtConcurrent::run(this, &ControllerHandler::setChannelReverseThread, channel, reverse);
}


void ControllerHandler::setChannelReverseThread(int channel, bool reverse)
{
    _siyiSdk.setChannelReverse(channel, reverse);
    pullChannelReverse();
}


std::pair<int, int> ControllerHandler::mapInputArrayToControlValues(qint8 inputArrayValue)
{
    switch (inputArrayValue) {
    case 0:
        // J1
        return {0,0};
    case 1:
        // J2
        return {0,1};
    case 2:
        // J3
        return {0,2};
    case 3:
        // J4
        return {0,3};
    case 4:
        // J5
        return {0,8};
    case 5:
        // J6
        return {0,9};
    case 6:
        // SA
        return {5,0};
    case 7:
        // SB
        return {5,1};
    case 8:
        // S1
        return {1,0};
    case 9:
        // S2
        return {1,1};
    case 10:
        // S3
        return {1,2};
    case 11:
        // S4
        return {1,3};
    case 12:
        // RD
        return {0,5};
    case 13:
        // LD
        return {0,4};
    case 14:
        // R1
        return {1,6};
    case 15:
        // R2
        return {1,7};
    case 16:
        // R3
        return {1,8};
    case 17:
        // L1
        return {1,4};
    case 18:
        // L2
        return {1,5};
    case 19:
        // M1
        return {1,9};
    case 20:
        // M2
        return {1,10};
    case 21:
        // M3
        return {1,11};
    case 22:
        // M4
        return {1,12};
    case 23:
        // M5
        return {1,13};
    case 24:
        // M6
        return {1,14};
    case 25:
        // "  "
        return {2,0};
    case 26:
        // RSSI
        return {2,1};
    case 27:
        // --
        return {3,0};
    default:
        return {-1,-1};
    }
}


void ControllerHandler::callSetChannelMapping(qint8 channel, qint8 input)
{
    QtConcurrent::run(this, &ControllerHandler::setChannelMapping, channel, input);
}

void ControllerHandler::setChannelMapping(qint8 channel, qint8 input)
{
    std::pair<int, int> control = mapInputArrayToControlValues(input);
    _siyiSdk.setChannelMapping(channel, control.first, control.second);
    pullChannelMappings();
}

void ControllerHandler::getAllButtonModes()
{
    _rcu.getButtonModes();
    _rcu.getFlightMode();
    _rcu.getDeadzone();
    pullFlightChannel();
}

void ControllerHandler::pullFlightChannel()
{
    _haveFlightChannel.store(false);
    while (!_haveFlightChannel.load())
    {
        _rcu.getFlightChannel();
        for (int i = 0; i < 20 && !_haveFlightChannel.load(); i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void ControllerHandler::setupFrameListener()
{
    _rcu.setFrameListener([this](int cmd, const std::vector<uint8_t>& vect){
        std::vector<uint8_t> v = vect;
        QMetaObject::invokeMethod(this, [this, cmd, v]{
            if (cmd == 0x34) {
                if (v.size() >= 2)
                {
                    for (int i = 0; i < 15; i++)
                    {
                        _buttonMap[_buttonList[v[i*2]]] = QString::number(v[(i*2)+1]);
                    }
                    emit buttonMapChanged();
                }
            }
            else if (cmd == 0x61)
            {
                if (!v.empty())
                {
                    _flightMode = v[0];
                    emit flightModeChanged();
                }
            }
            else if (cmd == 0x53)
            {
                if (!v.empty())
                {
                    _deadzone = v[0];
                    emit deadzoneChanged();
                }
            }
            else if (cmd == 0x63) {
                if (!v.empty())
                {
                    _flightChannel = v[0];
                    _haveFlightChannel.store(true);
                    emit flightChannelChanged();
                }
            }
        }, Qt::QueuedConnection);
    });
}


void ControllerHandler::callStartStickCalibration()
{
    QtConcurrent::run(this, &ControllerHandler::startStickCalibration);
}

void ControllerHandler::startStickCalibration()
{
    int res = _rcu.runCalibration(unircsdk::RcuSession::CAL_JOYSTICKS, [this](int cb){ joystickCalibrationCallback(cb);}, 12000);
    qDebug() << "Joystick Calibration Return Result: " << res;
}


void ControllerHandler::joystickCalibrationCallback(int cb)
{
    qDebug() << "Joystick Calibration Callback: " << cb;
    _stickCalibrationState = cb;
    emit stickCalibrationStateChanged();
}

void ControllerHandler::callStartDialCalibration()
{
    QtConcurrent::run(this, &ControllerHandler::startDialCalibration);
}

void ControllerHandler::startDialCalibration()
{
    int res = _rcu.runCalibration(unircsdk::RcuSession::CAL_DIALS, [this](int cb){ dialCalibrationCallback(cb);}, 20000);
    qDebug() << "Dial Calibration Return Result: " << res;
}


void ControllerHandler::dialCalibrationCallback(int cb)
{
    qDebug() << "Dial Calibration Callback: " << cb;
    _dialCalibrationState = cb;
    emit dialCalibrationStateChanged();
}


void ControllerHandler::toggleButtonMode(QString buttonName)
{
    qint8 buttonId = _buttonList.indexOf(buttonName);
    int newMode = (_buttonMap[buttonName].toInt() + 1) % 3;

    _buttonMap[buttonName] = newMode;
    emit buttonMapChanged();

    _rcu.setButtonMode(buttonId, newMode);
    _rcu.getButtonModes();
}


void ControllerHandler::startBinding()
{
    QtConcurrent::run(this, &ControllerHandler::startBindingThread);
}

void ControllerHandler::startBindingThread()
{
    qDebug() << "Start binding";
    if (!_siyiSdk.startBinding())
    {
        qDebug() << "Failed to call Start binding";
    }
}

void ControllerHandler::stopBinding()
{
    QtConcurrent::run(this, &ControllerHandler::stopBindingThread);
}

void ControllerHandler::stopBindingThread()
{
    qDebug() << "Stop binding";
    if (!_siyiSdk.stopBinding())
    {
        qDebug() << "Failed to call Stop binding";
    }
}

void ControllerHandler::monitorBindingStatus()
{
    qDebug() << "Starting bind monitor";
    while (true)
    {
        qDebug() << "Calling Bind Monitor";
        int status = 0;
        bool success =_siyiSdk.getBindingStatus(status);

        if (success)
        {
            _bindingStatus = status;
            emit bindingStatusChanged();
        }
        else
        {
             qDebug() << "Failed to get binding status";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ControllerHandler::getFlightMode()
{
    _rcu.getFlightMode();
}

void ControllerHandler::getFlightModeChannel()
{
    _rcu.getFlightChannel();
}

void ControllerHandler::setFlightMode(qint8 mode)
{
    _rcu.setFlightMode(mode);
    _rcu.getFlightMode();
}

void ControllerHandler::setFlightModeChannel(qint8 channel)
{
    qDebug() << "Channel " << channel;
    _rcu.setFlightChannel(channel);
    pullFlightChannel();
}

void ControllerHandler::callSetFlightChannel(qint8 channel)
{
    qDebug() << "test";
    QtConcurrent::run(this, &ControllerHandler::setFlightModeChannel, channel);
}

void ControllerHandler::callSetFlightMode(qint8 mode)
{
    QtConcurrent::run(this, &ControllerHandler::setFlightMode, mode);
}

void ControllerHandler::callSetDeadzone(qint8 value)
{
    QtConcurrent::run(this, &ControllerHandler::setDeadzone, value);
}

void ControllerHandler::setDeadzone(qint8 value)
{
    qint8 clamped = qBound<qint8>(10, value, 80);

    _deadzone = clamped;
    emit deadzoneChanged();

    _rcu.setDeadzone(clamped);
    _rcu.getDeadzone();
}


void ControllerHandler::shutdown_sdk()
{
    _siyiSdk.close();
    _rcu.close();
}
