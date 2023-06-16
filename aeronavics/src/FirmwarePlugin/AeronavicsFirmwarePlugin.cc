/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 * @file
 *   @brief Custom Firmware Plugin (PX4)
 *   @author Gus Grubba <gus@auterion.com>
 *
 */

#include "AeronavicsFirmwarePlugin.h"
#include "AeronavicsAutoPilotPlugin.h"

//-----------------------------------------------------------------------------
AeronavicsFirmwarePlugin::AeronavicsFirmwarePlugin()
{
}

//-----------------------------------------------------------------------------
AutoPilotPlugin* AeronavicsFirmwarePlugin::autopilotPlugin(Vehicle* vehicle)
{
    return new AeronavicsAutoPilotPlugin(vehicle, vehicle);
}

const QVariantList& AeronavicsFirmwarePlugin::toolIndicators(const Vehicle* vehicle)
{
    if (_toolIndicatorList.size() == 0) {
        // First call the base class to get the standard QGC list. This way we are guaranteed to always get
        // any new toolbar indicators which are added upstream in our custom build.
        _toolIndicatorList = FirmwarePlugin::toolIndicators(vehicle);
        // Then specifically remove the RC RSSI indicator.
        _toolIndicatorList.removeOne(QVariant::fromValue(QUrl::fromUserInput("qrc:/toolbar/RCRSSIIndicator.qml")));
    }
    return _toolIndicatorList;
}

// Tells QGC that your vehicle has a gimbal on it. This will in turn cause thing like gimbal commands to point
// the camera straight down for surveys to be automatically added to Plans.
//bool AeronavicsFirmwarePlugin::hasGimbal(Vehicle* /*vehicle*/, bool& rollSupported, bool& pitchSupported, bool& yawSupported)
//{
//    rollSupported = false;
//    pitchSupported = true;
//    yawSupported = true;
//
//    return true;
//}
