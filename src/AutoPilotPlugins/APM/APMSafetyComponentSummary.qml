import QtQuick 2.3
import QtQuick.Controls 1.2

import QGroundControl.FactSystem 1.0
import QGroundControl.FactControls 1.0
import QGroundControl.Controls 1.0
import QGroundControl.Palette 1.0

Item {
    anchors.fill:   parent

    FactPanelController { id: controller; }

    property Fact _copterFenceAction:       controller.getParameterFact(-1, "FENCE_ACTION", false /* reportMissing */)
    property Fact _copterFenceEnable:       controller.getParameterFact(-1, "FENCE_ENABLE", false /* reportMissing */)
    property Fact _copterFenceType:         controller.getParameterFact(-1, "FENCE_TYPE", false /* reportMissing */)

    property Fact _batt1Monitor:            controller.getParameterFact(-1, "BATT_MONITOR")
    property bool _batt1MonitorEnabled:     _batt1Monitor.rawValue !== 0

    property Fact _batt1FSLowAct:           controller.getParameterFact(-1, "r.BATT_FS_LOW_ACT", false /* reportMissing */)
    property Fact _batt1FSCritAct:          controller.getParameterFact(-1, "BATT_FS_CRT_ACT", false /* reportMissing */)
    property bool _batt1FSCritActAvailable: controller.parameterExists(-1, "BATT_FS_CRT_ACT")

    property Fact _genFSLowFuelLevel:       controller.getParameterFact(-1, "GEN_LOW_PER", false)
    property Fact _genFSLowFuelAct:         controller.getParameterFact(-1, "GEN_LOW_FS", false)
    property Fact _genFSCritFuelLevel:      controller.getParameterFact(-1, "GEN_CRIT_PER", false)
    property Fact _genFSCritFuelAct:        controller.getParameterFact(-1, "GEN_CRIT_FS", false)
    property Fact _genFSOffAct:             controller.getParameterFact(-1, "GEN_OFF_FS", false)
    property Fact _genFSErrorAct:           controller.getParameterFact(-1, "GEN_ERROR_FS", false)
    property Fact _genType:                 controller.getParameterFact(-1, "GEN_TYPE")
    property bool _genEnabled:              _genType.rawValue !== 0

    property bool _roverFirmware:           controller.parameterExists(-1, "MODE1") // This catches all usage of ArduRover firmware vehicle types: Rover, Boat...


    Column {
        anchors.fill:       parent

        VehicleSummaryRow {
            labelText:  qsTr("Throttle failsafe:")
            valueText:  fact ? fact.enumStringValue : ""
            visible:    controller.vehicle.multiRotor

            property Fact fact: controller.getParameterFact(-1, "FS_THR_ENABLE", false /* reportMissing */)
        }

        VehicleSummaryRow {
            labelText:  qsTr("Battery low failsafe:")
            valueText:  _batt1MonitorEnabled ? _batt1FSLowAct.enumStringValue : ""
            visible:    _batt1MonitorEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Battery critical failsafe:")
            valueText:  _batt1FSCritActAvailable ? _batt1FSCritAct.enumStringValue : ""
            visible:    _batt1FSCritActAvailable
        }

        VehicleSummaryRow {
            labelText:  qsTr("Gen low fuel failsafe:")
            valueText:  _genEnabled ? _genFSLowFuelLevel.valueString + _genFSLowFuelLevel.units + "   " + _genFSLowFuelAct.enumStringValue : ""
            visible:    _genEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Gen crit fuel failsafe:")
            valueText:  _genEnabled ? _genFSCritFuelLevel.valueString + _genFSCritFuelLevel.units + "   " + _genFSCritFuelAct.enumStringValue : ""
            visible:    _genEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Gen off failsafe:")
            valueText:  _genEnabled ? _genFSOffAct.enumStringValue : ""
            visible:    _genEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Gen error failsafe:")
            valueText:  _genEnabled ? _genFSErrorAct.enumStringValue : ""
            visible:    _genEnabled
        }


        VehicleSummaryRow {
            labelText: qsTr("GeoFence:")
            valueText: {
                if(_copterFenceEnable && _copterFenceType) {
                    if(_copterFenceEnable.value == 0 || _copterFenceType == 0) {
                        return qsTr("Disabled")
                    } else {
                        if(_copterFenceType.value == 1) {
                            return qsTr("Altitude")
                        }
                        if(_copterFenceType.value == 2) {
                            return qsTr("Circle")
                        }
                        return qsTr("Altitude,Circle")
                    }
                }
                return ""
            }
            visible: controller.vehicle.multiRotor
        }

        VehicleSummaryRow {
            labelText: qsTr("GeoFence:")
            valueText: _copterFenceAction.value == 0 ?
                           qsTr("Report only") :
                           (_copterFenceAction.value == 1 ? qsTr("RTL or Land") : qsTr("Unknown"))
            visible: controller.vehicle.multiRotor && _copterFenceEnable.value !== 0
        }

        VehicleSummaryRow {
            labelText:  qsTr("RTL min alt:")
            valueText:  fact ? (fact.value == 0 ? qsTr("current") : fact.valueString + " " + fact.units) : ""
            visible:    controller.vehicle.multiRotor

            property Fact fact: controller.getParameterFact(-1, "RTL_ALT", false /* reportMissing */)
        }
    }
}
