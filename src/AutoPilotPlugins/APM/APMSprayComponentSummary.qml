import QtQuick 2.3
import QtQuick.Controls 1.2

import QGroundControl.FactSystem 1.0
import QGroundControl.FactControls 1.0
import QGroundControl.Controls 1.0
import QGroundControl.Palette 1.0

Item {
    anchors.fill:   parent

    FactPanelController { id: controller; }

    property Fact _sprayer:                         controller.getParameterFact(-1, "SPOT_ENABLE")
    property bool _SprayEnabled:                    _sprayer.rawValue !== 0

    property Fact _sprayMode:                       controller.getParameterFact(-1, "SPOT_MODE", false /* reportMissing */)
    property Fact _sprayFlowrateHigh:               controller.getParameterFact(-1, "SPOT_RATE_HIGH", false /* reportMissing */)
    property Fact _sprayFlowrateMid:                controller.getParameterFact(-1, "SPOT_RATE_MID", false /* reportMissing */)
    property Fact _sprayFlowrateLow:                controller.getParameterFact(-1, "SPOT_RATE_LOW", false /* reportMissing */)
    property Fact _sprayVolumeHigh:                 controller.getParameterFact(-1, "SPOT_VOL_HIGH", false /* reportMissing */)
    property Fact _sprayVolumeMid:                  controller.getParameterFact(-1, "SPOT_VOL_MID", false /* reportMissing */)
    property Fact _sprayVolumeLow:                  controller.getParameterFact(-1, "SPOT_VOL_LOW", false /* reportMissing */)
    property Fact _sprayPulse:                      controller.getParameterFact(-1, "SPOT_PULSE", false /* reportMissing */)


    Column {
        anchors.fill:       parent

        VehicleSummaryRow {
            labelText:  qsTr("Spray Mode:")
            valueText:  _SprayEnabled ? _sprayMode.enumStringValue : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("High Flowrate:")
            valueText:  _SprayEnabled ? _sprayFlowrateHigh.valueString + _sprayFlowrateHigh.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Medium Flowrate:")
            valueText:  _SprayEnabled ? _sprayFlowrateMid.valueString + _sprayFlowrateMid.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Low Flowrate:")
            valueText:  _SprayEnabled ? _sprayFlowrateLow.valueString + _sprayFlowrateLow.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("High Volume:")
            valueText:  _SprayEnabled ? _sprayVolumeHigh.valueString + _sprayVolumeHigh.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Medium Volume:")
            valueText:  _SprayEnabled ? _sprayVolumeMid.valueString + _sprayVolumeMid.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Low Volume:")
            valueText:  _SprayEnabled ? _sprayVolumeLow.valueString + _sprayVolumeLow.units : ""
            visible:    _SprayEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Pulse Volume:")
            valueText:  _SprayEnabled ? _sprayPulse.valueString + _sprayPulse.units : ""
            visible:    _SprayEnabled
        }
    }
}
