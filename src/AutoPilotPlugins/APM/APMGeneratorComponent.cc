/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/// @file
///     @author Don Gagne <don@thegagnes.com>

#include "APMGeneratorComponent.h"
#include "APMAutoPilotPlugin.h"
#include "APMAirframeComponent.h"

APMGeneratorComponent::APMGeneratorComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent)
    : VehicleComponent(vehicle, autopilot, parent)
    , _name(tr("Generator"))
{
}

QString APMGeneratorComponent::name(void) const
{
    return _name;
}

QString APMGeneratorComponent::description(void) const
{
    switch (_vehicle->vehicleType()) {
    case MAV_TYPE_SUBMARINE:
    case MAV_TYPE_GROUND_ROVER:
    case MAV_TYPE_FIXED_WING:
    case MAV_TYPE_QUADROTOR:
    case MAV_TYPE_COAXIAL:
    case MAV_TYPE_HELICOPTER:
    case MAV_TYPE_HEXAROTOR:
    case MAV_TYPE_OCTOROTOR:
    case MAV_TYPE_TRICOPTER:
    default:
        return tr("Generator Setup is used to configure the generator failsafes.");
        break;
    }
}

QString APMGeneratorComponent::iconResource(void) const
{
    return QStringLiteral("/qmlimages/engine.svg");
}

bool APMGeneratorComponent::requiresSetup(void) const
{
    return false;
}

bool APMGeneratorComponent::setupComplete(void) const
{
    // FIXME: What aboout invalid settings?
    return true;
}

QStringList APMGeneratorComponent::setupCompleteChangedTriggerList(void) const
{
    return QStringList();
}

QUrl APMGeneratorComponent::setupSource(void) const
{
    QString qmlFile;

    switch (_vehicle->vehicleType()) {
    case MAV_TYPE_FIXED_WING:
    case MAV_TYPE_QUADROTOR:
    case MAV_TYPE_COAXIAL:
    case MAV_TYPE_HELICOPTER:
    case MAV_TYPE_HEXAROTOR:
    case MAV_TYPE_OCTOROTOR:
    case MAV_TYPE_TRICOPTER:
    case MAV_TYPE_GROUND_ROVER:
    case MAV_TYPE_SUBMARINE:
    default:
        qmlFile = QStringLiteral("qrc:/qml/APMGeneratorComponent.qml");
        break;
    }

    return QUrl::fromUserInput(qmlFile);
}

QUrl APMGeneratorComponent::summaryQmlSource(void) const
{
    QString qmlFile;

    switch (_vehicle->vehicleType()) {
    case MAV_TYPE_FIXED_WING:
    case MAV_TYPE_QUADROTOR:
    case MAV_TYPE_COAXIAL:
    case MAV_TYPE_HELICOPTER:
    case MAV_TYPE_HEXAROTOR:
    case MAV_TYPE_OCTOROTOR:
    case MAV_TYPE_TRICOPTER:
    case MAV_TYPE_GROUND_ROVER:
    case MAV_TYPE_SUBMARINE:
    default:
        qmlFile = QStringLiteral("qrc:/qml/APMGeneratorComponentSummary.qml");
        break;
    }

    return QUrl::fromUserInput(qmlFile);
}
