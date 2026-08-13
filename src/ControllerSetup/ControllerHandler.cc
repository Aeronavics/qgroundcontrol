#include "ControllerHandler.h"

#include <QFileInfo>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

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
    _running.store(false);
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
    while (!_haveFlightChannel.load() && _running.load())
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
            else if (cmd == unircsdk::RcuSession::CMD_GET_SKY_UPGRADE ||
                     cmd == unircsdk::RcuSession::CMD_SET_SKY_UPGRADE)
            {
                // NB inverted: a payload byte of 0 means ready/ok.
                const bool ready = unircsdk::RcuSession::skyUpgradeReplyIsReady(v);
                _skyUpgradeReady.store(ready);
                _skyUpgradeReplied.store(true);
                qDebug() << "Sky upgrade mode reply: cmd=" << cmd
                         << "raw=" << (v.empty() ? -1 : int(v[0]))
                         << "ready=" << ready;
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
    bool lastSuccess = true;

    while (_running.load())
    {
        int status = 0;
        const bool success = _siyiSdk.getBindingStatus(status);

        if (success)
        {
            if (!lastSuccess) {
                qDebug() << "Bind monitor: binding status readable again";
            }
            if (_bindingStatus != status) {
                _bindingStatus = status;
                emit bindingStatusChanged();
            }
        }
        else if (lastSuccess)
        {
            qDebug() << "Bind monitor: failed to get binding status (further failures suppressed)";
        }
        lastSuccess = success;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    qDebug() << "Bind monitor stopped";
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
    _running.store(false);
    _siyiSdk.close();
    _rcu.close();
}

//-----------------------------------------------------------------------------
// FPV (image-transmission) firmware update.
//
// The transfer is FTP, not the UDP control channel, and the whole thing takes
// minutes — so every call here runs on a worker and reports back through
// postFpvState(). See src/Siyi/FIRMWARE_UPGRADE_PROTOCOL.md.
//-----------------------------------------------------------------------------

void ControllerHandler::postFpvState(qint8 state, int progress, const QString& status)
{
    QMetaObject::invokeMethod(this, [this, state, progress, status] {
        if (_fpvUpdateState != state) {
            _fpvUpdateState = state;
            emit fpvUpdateStateChanged();
        }
        if (_fpvUpdateProgress != progress) {
            _fpvUpdateProgress = progress;
            emit fpvUpdateProgressChanged();
        }
        if (_fpvUpdateStatus != status) {
            _fpvUpdateStatus = status;
            emit fpvUpdateStatusChanged();
        }
    }, Qt::QueuedConnection);
}

void ControllerHandler::callRefreshFpvVersion()
{
    QtConcurrent::run(this, &ControllerHandler::refreshFpvVersion);
}

void ControllerHandler::refreshFpvVersion()
{
    // Don't probe mid-update: the module is rebooting and would look "down".
    if (_fpvUpdateBusy.load()) {
        return;
    }

    bool    up = false;
    QString version;

    unircsdk::FpvUpgradeClient client;
    if (client.open()) {
        std::string v;
        if (client.queryVersion(v, 3000) && !v.empty()) {
            up      = true;
            version = QString::fromStdString(v);
        }
    }

    // Also probe the air unit, so the UI can offer it only when it is present.
    bool    airUp = false;
    QString airVer;
    {
        unircsdk::FpvUpgradeClient airClient;
        if (airClient.open(unircsdk::FpvUpgradeClient::DEFAULT_AIR_IP)) {
            std::string v;
            if (airClient.queryVersion(v, 3000) && !v.empty()) {
                airUp  = true;
                airVer = QString::fromStdString(v);
            }
        }
    }

    QMetaObject::invokeMethod(this, [this, airUp, airVer] {
        if (_airUnitPresent != airUp || _airVersion != airVer) {
            _airUnitPresent = airUp;
            _airVersion     = airVer;
            emit airUnitPresentChanged();
        }
    }, Qt::QueuedConnection);

    QMetaObject::invokeMethod(this, [this, up, version] {
        if (_fpvLinkUp != up) {
            _fpvLinkUp = up;
            emit fpvLinkUpChanged();
        }
        if (_fpvVersion != version) {
            _fpvVersion = version;
            emit fpvVersionChanged();
        }
    }, Qt::QueuedConnection);
}

void ControllerHandler::callSelectFirmware(const QString& url)
{
    _firmwareUrl  = url;
    _firmwareName.clear();
    _firmwareSize = 0;

    if (!url.isEmpty()) {
        // QFile handles plain paths, file:// and — on Android — content:// URIs.
        // A content:// URI has no filesystem path, so never go via toLocalFile().
        const QUrl parsed(url);
        const QString openTarget = parsed.isLocalFile() ? parsed.toLocalFile() : url;

        // The file browser's return value differs per platform (content:// on
        // Android, file:// on desktop) and getting it wrong silently drops the
        // selection, so log exactly what came back.
        qDebug() << "FPV firmware selected:" << url
                 << "scheme:" << parsed.scheme()
                 << "isLocalFile:" << parsed.isLocalFile()
                 << "openTarget:" << openTarget;

        QFileInfo info(openTarget);
        _firmwareName = info.fileName();
        _firmwareSize = info.size();

        // Some providers give no usable name via QFileInfo; fall back to the last
        // path segment of the URI, which usually still carries the file name.
        if (_firmwareName.isEmpty()) {
            _firmwareName = parsed.fileName();
        }
        if (_firmwareName.isEmpty()) {
            _firmwareName = url.section(QLatin1Char('/'), -1);
        }

        // For a content:// URI the "file name" is really the provider's document
        // id, e.g.
        //   primary%3ADownload%2FSome Folder%2FCX6653C-N_A5.1.2.6-….zip1
        // Percent-decode it and keep only the final component, otherwise the
        // whole encoded path gets announced to the module as the firmware name —
        // and the module validates that name, so it rejects the update.
        _firmwareName = QUrl::fromPercentEncoding(_firmwareName.toUtf8());
        if (_firmwareName.contains(QLatin1Char('/'))) {
            _firmwareName = _firmwareName.section(QLatin1Char('/'), -1);
        }
        if (_firmwareName.contains(QLatin1Char(':'))) {
            _firmwareName = _firmwareName.section(QLatin1Char(':'), -1);
        }

        if (_firmwareSize <= 0) {
            QFile probe(openTarget);
            if (probe.open(QIODevice::ReadOnly)) {
                _firmwareSize = probe.size();
                probe.close();
            }
        }
    }

    qDebug() << "FPV firmware resolved: name=" << _firmwareName << "size=" << _firmwareSize
             << (_firmwareSize > 0 ? "(readable)" : "(NOT readable - size unknown)");

    emit firmwareSelectionChanged();
}

void ControllerHandler::callUpgradeFpvFirmware(bool airUnit)
{
    if (_firmwareUrl.isEmpty()) {
        qWarning() << "No firmware selected";
        return;
    }
    bool expected = false;
    if (!_fpvUpdateBusy.compare_exchange_strong(expected, true)) {
        qWarning() << "FPV firmware update already in progress, ignoring request";
        return;
    }
    QtConcurrent::run(this, &ControllerHandler::upgradeFpvFirmware,
                      _firmwareUrl, _firmwareName, airUnit);
}

bool ControllerHandler::prepareAirUnitForUpgrade()
{
    // Mirrors UniGCS's sky path: enable upgrade mode on the RC link, let it
    // settle, then poll until the unit reports ready.
    qDebug() << "Air unit: enabling sky upgrade mode";
    _skyUpgradeReplied.store(false);
    _skyUpgradeReady.store(false);
    _rcu.setSkyUpgradeMode(true);

    // UniGCS waits a flat 12s here before it even starts asking.
    for (int i = 0; i < 120 && _running.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (int attempt = 0; attempt < 4 && _running.load(); ++attempt) {
        _skyUpgradeReplied.store(false);
        _rcu.getSkyUpgradeReady();
        for (int i = 0; i < 10 && !_skyUpgradeReplied.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (_skyUpgradeReplied.load() && _skyUpgradeReady.load()) {
            qDebug() << "Air unit: reported ready after" << (attempt + 1) << "poll(s)";
            return true;
        }
    }
    qWarning() << "Air unit: never reported ready";
    return false;
}

QString ControllerHandler::stageFirmwareLocally(const QString& url, const QString& name, QString& errorOut)
{
    const QUrl parsed(url);

    // Already a real file? Use it where it lies — no point copying 100+ MB.
    if (parsed.isLocalFile() || !parsed.scheme().startsWith(QStringLiteral("content"))) {
        const QString path = parsed.isLocalFile() ? parsed.toLocalFile() : url;
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    // content:// (Android's system file browser). The upload path needs a real
    // file it can fopen(), so copy it into our own storage first. The name is
    // preserved because the module validates the firmware file name.
    QFile src(url);
    if (!src.open(QIODevice::ReadOnly)) {
        qWarning() << "FPV firmware: cannot open" << url << src.errorString();
        errorOut = tr("Cannot read the selected file.");
        return QString();
    }
    qDebug() << "FPV firmware: staging" << url << "(" << src.size() << "bytes )";

    const QString stageDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (stageDir.isEmpty() || !QDir().mkpath(stageDir)) {
        errorOut = tr("No writable temporary location.");
        return QString();
    }
    const QString stagePath = QDir(stageDir).filePath(name.isEmpty() ? QStringLiteral("firmware.bin") : name);

    QFile::remove(stagePath);
    QFile dst(stagePath);
    if (!dst.open(QIODevice::WriteOnly)) {
        errorOut = tr("Cannot write to temporary storage.");
        return QString();
    }

    char buf[64 * 1024];
    qint64 n;
    while ((n = src.read(buf, sizeof(buf))) > 0) {
        if (dst.write(buf, n) != n) {
            dst.close();
            QFile::remove(stagePath);
            errorOut = tr("Ran out of space while copying the firmware.");
            return QString();
        }
    }
    dst.close();
    src.close();

    if (n < 0) {
        QFile::remove(stagePath);
        errorOut = tr("Failed while reading the selected file.");
        return QString();
    }

    qDebug() << "FPV firmware: staged to" << stagePath
             << QFileInfo(stagePath).size() << "bytes";
    return stagePath;
}

void ControllerHandler::upgradeFpvFirmware(QString url, QString name, bool airUnit)
{
    using Client = unircsdk::FpvUpgradeClient;

    const QString target = airUnit ? tr("air unit") : tr("ground unit");
    const char* const ip = airUnit ? Client::DEFAULT_AIR_IP : Client::DEFAULT_GROUND_IP;
    // The air unit is across the RF link, so it still wants a far more generous
    // socket timeout than the wired-speed ground unit. (Idle RF throughput
    // measures ~0.12 MB/s; the upgrade-mode prelude lifts it well above that,
    // but the timeout must cover the slow case.)
    const int ftpTimeout = airUnit ? Client::FTP_TIMEOUT_SLOW_MS : Client::FTP_TIMEOUT_MS;

    postFpvState(FpvAnnouncing, 0, tr("Preparing %1…").arg(name));

    QString stageError;
    const QString filePath = stageFirmwareLocally(url, name, stageError);
    if (filePath.isEmpty()) {
        postFpvState(FpvFailed, 0, stageError.isEmpty() ? tr("Could not read the firmware file.") : stageError);
        _fpvUpdateBusy.store(false);
        return;
    }
    // Only delete what we created ourselves.
    const bool staged = (filePath != url) && !QUrl(url).isLocalFile();

    // Air unit only: put the link into upgrade mode before touching it.
    if (airUnit) {
        postFpvState(FpvAnnouncing, 0, tr("Preparing the air unit link…"));
        if (!prepareAirUnitForUpgrade()) {
            postFpvState(FpvFailed, 0,
                         tr("The air unit did not become ready. Check it is powered and bound."));
            _rcu.setSkyUpgradeMode(false);
            if (staged) QFile::remove(filePath);
            _fpvUpdateBusy.store(false);
            return;
        }
    }

    qDebug() << "FPV firmware: announcing name" << name << "to" << ip
             << (airUnit ? "(air)" : "(ground)");
    postFpvState(FpvAnnouncing, 0, tr("Connecting to the %1…").arg(target));

    Client client;
    if (!client.open(QString::fromLatin1(ip).toStdString())) {
        postFpvState(FpvFailed, 0, tr("Could not open the FPV link."));
        if (airUnit) _rcu.setSkyUpgradeMode(false);
        if (staged) QFile::remove(filePath);
        _fpvUpdateBusy.store(false);
        return;
    }

    // Fail fast with an explanation rather than sitting through timeouts. A
    // plugged-in USB cable is the usual cause: it swaps the Android side away
    // from the radio module, so 192.168.144.x disappears entirely.
    std::string probe;
    if (!client.queryVersion(probe, 3000)) {
        postFpvState(FpvFailed, 0,
                     tr("No response from the ground unit. Unplug any USB cable from the "
                        "controller and try again — USB disconnects the internal radio link."));
        if (airUnit) _rcu.setSkyUpgradeMode(false);
        if (staged) QFile::remove(filePath);
        _fpvUpdateBusy.store(false);
        return;
    }

    std::string newVersion;
    const Client::Result result = client.upgrade(
        filePath.toStdString(),
        [this, name, airUnit](Client::Phase phase, int percent) {
            switch (phase) {
            case Client::Phase::Announce:
                postFpvState(FpvAnnouncing, 0, tr("Preparing %1…").arg(name));
                break;
            case Client::Phase::Upload:
                postFpvState(FpvUploading, percent,
                             airUnit
                                ? tr("Uploading to the air unit… %1%").arg(percent)
                                : tr("Uploading firmware… %1%").arg(percent));
                break;
            case Client::Phase::Verify:
                postFpvState(FpvVerifying, 100, tr("Verifying upload…"));
                break;
            case Client::Phase::Commit:
                postFpvState(FpvCommitting, 100, tr("Writing firmware — do not power off."));
                break;
            case Client::Phase::Reboot:
                postFpvState(FpvRebooting, 100,
                            airUnit
                                 ? tr("Rebooting the air unit. This can take a few minutes.")
                                 : tr("Rebooting the ground unit. This can take a few minutes."));
                break;
            }
        },
        &newVersion,
        Client::FTP_PORT,
        ftpTimeout);

    qDebug() << "FPV firmware: upgrade() returned" << Client::resultName(result);

    if (result == Client::Result::Ok) {
        const QString v = QString::fromStdString(newVersion);
        postFpvState(FpvSuccess, 100,
                     v.isEmpty() ? tr("Update complete.")
                                 : tr("Update complete. The %1 now reports %2.").arg(target).arg(v));
        QMetaObject::invokeMethod(this, [this, v] {
            if (!v.isEmpty() && _fpvVersion != v) {
                _fpvVersion = v;
                emit fpvVersionChanged();
            }
        }, Qt::QueuedConnection);
    } else {
        postFpvState(FpvFailed, 0,
                     tr("Update failed: %1").arg(QString::fromLatin1(Client::resultName(result))));
    }

    if (airUnit) {
        // Leave the link as we found it regardless of outcome.
        qDebug() << "Air unit: disabling sky upgrade mode";
        _rcu.setSkyUpgradeMode(false);
    }
    if (staged) {
        QFile::remove(filePath);
    }
    _fpvUpdateBusy.store(false);
}
