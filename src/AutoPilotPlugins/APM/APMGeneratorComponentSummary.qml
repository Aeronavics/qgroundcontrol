import QtQuick 2.3
import QtQuick.Controls 1.2

import QGroundControl.FactSystem 1.0
import QGroundControl.FactControls 1.0
import QGroundControl.Controls 1.0
import QGroundControl.Palette 1.0

Item {
    anchors.fill:   parent

    FactPanelController { id: controller; }

    property Fact _generator:                       controller.getParameterFact(-1, "GEN_TYPE")
    property bool _generatorEnabled:                _generator.rawValue === 4

    property Fact _generatorLowFS:                  controller.getParameterFact(-1, "GEN_LOW_FS", false /* reportMissing */)
    property Fact _generatorCritFS:                 controller.getParameterFact(-1, "GEN_CRIT_FS", false /* reportMissing */)
    property Fact _generatorOffFS:                  controller.getParameterFact(-1, "GEN_OFF_FS", false /* reportMissing */)
    property Fact _generatorErrorFS:                controller.getParameterFact(-1, "GEN_ERROR_FS", false /* reportMissing */)


    Column {
        anchors.fill:       parent

        VehicleSummaryRow {
            labelText:  qsTr("Low Fuel Failsafe:")
            valueText:  _generatorEnabled ? _generatorLowFS.enumStringValue : ""
            visible:    _generatorEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Critical Fuel Failsafe:")
            valueText:  _generatorEnabled ? _generatorCritFS.enumStringValue : ""
            visible:    _generatorEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Power Loss Failsafe:")
            valueText:  _generatorEnabled ? _generatorOffFS.enumStringValue : ""
            visible:    _generatorEnabled
        }

        VehicleSummaryRow {
            labelText:  qsTr("Error Failsafe:")
            valueText:  _generatorEnabled ? _generatorErrorFS.enumStringValue : ""
            visible:    _generatorEnabled
        }
    }
}
