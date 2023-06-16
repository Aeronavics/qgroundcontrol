/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 * @file
 *   @brief Custom Autopilot Plugin
 *   @author Gus Grubba <gus@auterion.com>
 */

#include "AeronavicsAutoPilotPlugin.h"

#include "QGCApplication.h"
#include "QGCCorePlugin.h"

#include <APMAirframeComponent.h>
#include <APMCameraComponent.h>
#include <APMFlightModesComponent.h>
#include <APMMotorComponent.h>
#include <APMPowerComponent.h>
#include <APMRadioComponent.h>
#include <APMSafetyComponent.h>
#include <APMSensorsComponent.h>
#include <APMTuningComponent.h>
#include <ParameterManager.h>

AeronavicsAutoPilotPlugin::AeronavicsAutoPilotPlugin(Vehicle* vehicle, QObject* parent)
    : APMAutoPilotPlugin(vehicle, parent)
{
    // Whenever we go on/out of advanced mode the available list of settings pages will change
    connect(qgcApp()->toolbox()->corePlugin(), &QGCCorePlugin::showAdvancedUIChanged, this, &AeronavicsAutoPilotPlugin::_advancedChanged);
}

// This signals that when Advanced Mode changes the list of Vehicle Settings page also changed
void AeronavicsAutoPilotPlugin::_advancedChanged(bool)
{
    _components.clear();
    emit vehicleComponentsChanged();
}

// This allows us to hide most Vehicle Setup pages unless we are in Advanced Mmode
const QVariantList& AeronavicsAutoPilotPlugin::vehicleComponents()
{
    if (_components.count() == 0 && !_incorrectParameterVersion) {
        if (_vehicle) {
            bool showAdvanced = qgcApp()->toolbox()->corePlugin()->showAdvancedUI();
            if (_vehicle->parameterManager()->parametersReady()) {
                if (showAdvanced) {
                    _airframeComponent = new APMAirframeComponent(_vehicle, this);
                    _airframeComponent->setupTriggerSignals();
                    _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_airframeComponent)));

                    _flightModesComponent = new APMFlightModesComponent(_vehicle, this);
                    _flightModesComponent->setupTriggerSignals();
                    _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_flightModesComponent)));

                    _powerComponent = new APMPowerComponent(_vehicle, this);
                    _powerComponent->setupTriggerSignals();
                    _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_powerComponent)));

                    _motorComponent = new APMMotorComponent(_vehicle, this);
                    _motorComponent->setupTriggerSignals();
                    _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_motorComponent)));
                }

                _sensorsComponent = new APMSensorsComponent(_vehicle, this);
                _sensorsComponent->setupTriggerSignals();
                _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_sensorsComponent)));

                _radioComponent = new APMRadioComponent(_vehicle, this);
                _radioComponent->setupTriggerSignals();
                _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_radioComponent)));

                _safetyComponent = new APMSafetyComponent(_vehicle, this);
                _safetyComponent->setupTriggerSignals();
                _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_safetyComponent)));

                if (showAdvanced) {
                    _tuningComponent = new APMTuningComponent(_vehicle, this);
                    _tuningComponent->setupTriggerSignals();
                    _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_tuningComponent)));

                    //-- Is there support for cameras?
                    if(_vehicle->parameterManager()->parameterExists(_vehicle->id(), "TRIG_MODE")) {
                        _cameraComponent = new APMCameraComponent(_vehicle, this);
                        _cameraComponent->setupTriggerSignals();
                        _components.append(QVariant::fromValue(reinterpret_cast<VehicleComponent*>(_cameraComponent)));
                    }
                }
            } else {
                qWarning() << "Call to vehicleCompenents prior to parametersReady";
            }
        } else {
            qWarning() << "Internal error";
        }
    }
    return _components;
}
