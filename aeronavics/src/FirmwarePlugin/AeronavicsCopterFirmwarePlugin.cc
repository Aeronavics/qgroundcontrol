
///     @author James Morritt

#include "AeronavicsCopterFirmwarePlugin.h"
#include "QGCApplication.h"
#include "MissionManager.h"
#include "ParameterManager.h"

bool AeronavicsCopterFirmwarePlugin::_remapParamNameIntialized = false;
FirmwarePlugin::remapParamNameMajorVersionMap_t AeronavicsCopterFirmwarePlugin::_remapParamName;

AeronavicsCopterMode::AeronavicsCopterMode(uint32_t mode, bool settable) :
    APMCustomMode(mode, settable)
{
    setEnumToStringMapping({
        { STABILIZE,    "Stabilize"},
        { ACRO,         "Acro"},
        { ALT_HOLD,     "Altitude Hold"},
        { AUTO,         "Auto"},
        { GUIDED,       "Guided"},
        { LOITER,       "Loiter"},
        { RTL,          "RTL"},
        { CIRCLE,       "Circle"},
        { LAND,         "Land"},
        { DRIFT,        "Drift"},
        { SPORT,        "Sport"},
        { FLIP,         "Flip"},
        { AUTOTUNE,     "Autotune"},
        { POS_HOLD,     "Position Hold"},
        { BRAKE,        "Brake"},
        { THROW,        "Throw"},
        { AVOID_ADSB,   "Avoid ADSB"},
        { GUIDED_NOGPS, "Guided No GPS"},
        { SMART_RTL,    "Smart RTL"},
        { FLOWHOLD,     "Flow Hold" },
        { FOLLOW,       "Follow" },
        { ZIGZAG,       "ZigZag" },
        { SYSTEMID,     "SystemID" },
        { AUTOROTATE,   "AutoRotate" },
        { AUTO_RTL,     "AutoRTL" },
        { TURTLE,       "Turtle" },
    });
}

AeronavicsCopterFirmwarePlugin::AeronavicsCopterFirmwarePlugin(void)
{
    setSupportedModes({
        AeronavicsCopterMode(AeronavicsCopterMode::ALT_HOLD,      true),
        AeronavicsCopterMode(AeronavicsCopterMode::POS_HOLD,      true),
        AeronavicsCopterMode(AeronavicsCopterMode::LOITER,        true),
        AeronavicsCopterMode(AeronavicsCopterMode::AUTO,          true),
        AeronavicsCopterMode(AeronavicsCopterMode::LAND,          true),
        AeronavicsCopterMode(AeronavicsCopterMode::RTL,           true),
        AeronavicsCopterMode(AeronavicsCopterMode::SMART_RTL,     true),
        AeronavicsCopterMode(AeronavicsCopterMode::GUIDED,        true),


        AeronavicsCopterMode(AeronavicsCopterMode::AVOID_ADSB,    false),
        AeronavicsCopterMode(AeronavicsCopterMode::STABILIZE,     false),
        AeronavicsCopterMode(AeronavicsCopterMode::ACRO,          false),
        AeronavicsCopterMode(AeronavicsCopterMode::CIRCLE,        false),
        AeronavicsCopterMode(AeronavicsCopterMode::DRIFT,         false),
        AeronavicsCopterMode(AeronavicsCopterMode::SPORT,         false),
        AeronavicsCopterMode(AeronavicsCopterMode::FLIP,          false),
        AeronavicsCopterMode(AeronavicsCopterMode::AUTOTUNE,      false),
        AeronavicsCopterMode(AeronavicsCopterMode::BRAKE,         false),
        AeronavicsCopterMode(AeronavicsCopterMode::THROW,         false),
        AeronavicsCopterMode(AeronavicsCopterMode::FLOWHOLD,      false),
        AeronavicsCopterMode(AeronavicsCopterMode::FOLLOW,        false),
        AeronavicsCopterMode(AeronavicsCopterMode::ZIGZAG,        false),
        AeronavicsCopterMode(AeronavicsCopterMode::SYSTEMID,      false),
        AeronavicsCopterMode(AeronavicsCopterMode::AUTOROTATE,    false),
        AeronavicsCopterMode(AeronavicsCopterMode::TURTLE,        false),
        AeronavicsCopterMode(AeronavicsCopterMode::AUTO_RTL,      false),
        AeronavicsCopterMode(AeronavicsCopterMode::GUIDED_NOGPS,  false),
    });

    if (!_remapParamNameIntialized) {
        FirmwarePlugin::remapParamNameMap_t& remapV3_6 = _remapParamName[3][6];

        remapV3_6["BATT_AMP_PERVLT"] =  QStringLiteral("BATT_AMP_PERVOL");
        remapV3_6["BATT2_AMP_PERVLT"] = QStringLiteral("BATT2_AMP_PERVOL");
        remapV3_6["BATT_LOW_MAH"] =     QStringLiteral("FS_BATT_MAH");
        remapV3_6["BATT_LOW_VOLT"] =    QStringLiteral("FS_BATT_VOLTAGE");
        remapV3_6["BATT_FS_LOW_ACT"] =  QStringLiteral("FS_BATT_ENABLE");
        remapV3_6["PSC_ACCZ_P"] =       QStringLiteral("ACCEL_Z_P");
        remapV3_6["PSC_ACCZ_I"] =       QStringLiteral("ACCEL_Z_I");

        FirmwarePlugin::remapParamNameMap_t& remapV3_7 = _remapParamName[3][7];

        remapV3_7["BATT_ARM_VOLT"] =    QStringLiteral("ARMING_VOLT_MIN");
        remapV3_7["BATT2_ARM_VOLT"] =   QStringLiteral("ARMING_VOLT2_MIN");
        remapV3_7["RC7_OPTION"] =       QStringLiteral("CH7_OPT");
        remapV3_7["RC8_OPTION"] =       QStringLiteral("CH8_OPT");
        remapV3_7["RC9_OPTION"] =       QStringLiteral("CH9_OPT");
        remapV3_7["RC10_OPTION"] =      QStringLiteral("CH10_OPT");
        remapV3_7["RC11_OPTION"] =      QStringLiteral("CH11_OPT");
        remapV3_7["RC12_OPTION"] =      QStringLiteral("CH12_OPT");

        FirmwarePlugin::remapParamNameMap_t& remapV4_0 = _remapParamName[4][0];

        remapV4_0["TUNE_MIN"] = QStringLiteral("TUNE_HIGH");
        remapV3_7["TUNE_MAX"] = QStringLiteral("TUNE_LOW");

        _remapParamNameIntialized = true;
    }
}

int AeronavicsCopterFirmwarePlugin::remapParamNameHigestMinorVersionNumber(int majorVersionNumber) const
{
    // Remapping supports up to 3.7
    return majorVersionNumber == 3 ? 7 : Vehicle::versionNotSetValue;
}

void AeronavicsCopterFirmwarePlugin::guidedModeLand(Vehicle* vehicle)
{
    _setFlightModeAndValidate(vehicle, "Land");
}

bool AeronavicsCopterFirmwarePlugin::multiRotorCoaxialMotors(Vehicle* vehicle)
{
    Q_UNUSED(vehicle);
    return _coaxialMotors;
}

bool AeronavicsCopterFirmwarePlugin::multiRotorXConfig(Vehicle* vehicle)
{
    return vehicle->parameterManager()->getParameter(FactSystem::defaultComponentId, "FRAME")->rawValue().toInt() != 0;
}

void AeronavicsCopterFirmwarePlugin::sendGCSMotionReport(Vehicle* vehicle, FollowMe::GCSMotionReport& motionReport, uint8_t estimatationCapabilities)
{
    _sendGCSMotionReport(vehicle, motionReport, estimatationCapabilities);
}
