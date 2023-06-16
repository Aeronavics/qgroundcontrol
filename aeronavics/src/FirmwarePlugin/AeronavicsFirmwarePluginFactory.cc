/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 * @file
 *   @brief Custom Firmware Plugin Factory (APM)
 *   @author Gus Grubba <gus@auterion.com>
 *
 */

#include "AeronavicsFirmwarePluginFactory.h"
#include "AeronavicsCopterFirmwarePlugin.h"

AeronavicsFirmwarePluginFactory AeronavicsFirmwarePluginFactoryImp;

AeronavicsFirmwarePluginFactory::AeronavicsFirmwarePluginFactory()
    : _pluginInstance(nullptr)
{

}

QList<QGCMAVLink::FirmwareClass_t> AeronavicsFirmwarePluginFactory::supportedFirmwareClasses() const
{
    QList<QGCMAVLink::FirmwareClass_t> firmwareClasses;
    firmwareClasses.append(QGCMAVLink::FirmwareClassArduPilot);
    return firmwareClasses;
}

QList<QGCMAVLink::VehicleClass_t> AeronavicsFirmwarePluginFactory::supportedVehicleClasses(void) const
{
    QList<QGCMAVLink::VehicleClass_t> vehicleClasses;
    vehicleClasses.append(QGCMAVLink::VehicleClassMultiRotor);
    return vehicleClasses;
}

FirmwarePlugin* AeronavicsFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE /*vehicleType*/)
{
    if (autopilotType == MAV_AUTOPILOT_ARDUPILOTMEGA) {
        if (!_pluginInstance) {
            _pluginInstance = new AeronavicsCopterFirmwarePlugin;
        }
        return _pluginInstance;
    }
    return nullptr;
}
