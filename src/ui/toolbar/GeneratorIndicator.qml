/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.11
import QtQuick.Layouts  1.11

import QGroundControl                       1.0
import QGroundControl.Controls              1.0
import QGroundControl.MultiVehicleManager   1.0
import QGroundControl.ScreenTools           1.0
import QGroundControl.Palette               1.0
import MAVLink                              1.0

//-------------------------------------------------------------------------
//-- Generator Indicator
Item {
    id:             _root
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width: generatorRow.width * 1.1

    property bool showIndicator: true

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    property var generator: _activeVehicle.generator

    function getGeneratorColor() {
        if (
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERTEMP_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_ELECTRONICS_OVERTEMP_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_ELECTRONICS_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_POWERSOURCE_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_POWER_RAIL_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERCURRENT_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_BATTERY_OVERCHARGE_CURRENT_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERVOLTAGE_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_BATTERY_UNDERVOLT_FAULT) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OFF && _activeVehicle.armed())
        ) {
            return qgcPal.colorRed
        }
        else if (
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_WARMING_UP) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_MAINTENANCE_REQUIRED) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_REDUCED_POWER) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_MAXPOWER) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERTEMP_WARNING) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_ELECTRONICS_OVERTEMP_WARNING) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_COMMUNICATION_WARNING) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_COOLING_WARNING)
        ) {
            return qgcPal.colorOrange
        }
        else if (
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_READY) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_GENERATING) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_CHARGING) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_IDLE) ||
            (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OFF && !_activeVehicle.armed())
        ) {
            return qgcPal.text
        }
        else {
            return qgcPal.colorRed
        }
    }

    function getGeneratorIcon() {
        if (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_START_INHIBITED) {
            return "/qmlimages/engine.svg"
        }
        else {
            return "/qmlimages/engine-powered.svg"
        }
    }
    

    Row {
        id:             generatorRow
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth

        QGCColoredImage {
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            width:              height
            sourceSize.width:   width
            source:             getGeneratorIcon()
            fillMode:           Image.PreserveAspectFit
            color:              getGeneratorColor()
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked: {
            mainWindow.showIndicatorPopup(_root, generatorPopup)
        }
    }

    Component {
        id: generatorPopup

        Rectangle {
            width:          mainLayout.width   + mainLayout.anchors.margins * 2
            height:         mainLayout.height  + mainLayout.anchors.margins * 2
            radius:         ScreenTools.defaultFontPixelHeight / 2
            color:          qgcPal.window
            border.color:   qgcPal.text

            ColumnLayout {
                id:                 mainLayout
                anchors.margins:    ScreenTools.defaultFontPixelWidth
                anchors.top:        parent.top
                anchors.right:      parent.right
                spacing:            ScreenTools.defaultFontPixelHeight

                QGCLabel {
                    Layout.alignment:   Qt.AlignCenter
                    text:               qsTr("Generator Status")
                    font.family:        ScreenTools.demiboldFontFamily
                }

                RowLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    ColumnLayout {
                        ColumnLayout {
                            spacing: 0

                            QGCLabel { text: qsTr("State"); }
                            QGCLabel { text: qsTr("Error"); }
                            QGCLabel { text: qsTr("Voltage") }
                            QGCLabel { text: qsTr("Current") }
                            QGCLabel { text: qsTr("RPM"); }
                            QGCLabel { text: qsTr("Engine Temp"); }
                            QGCLabel { text: qsTr("Coil Temp"); }
                            QGCLabel { text: qsTr("Run Time"); }
                            QGCLabel { text: qsTr("Time to Service"); }
                        }
                    }

                    ColumnLayout {

                        ColumnLayout {
                            spacing: 0
                            QGCLabel { text:
                                (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OFF) === MAVLink.MAV_GENERATOR_STATUS_FLAG_OFF ? qsTr("OFF") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_WARMING_UP) === MAVLink.MAV_GENERATOR_STATUS_FLAG_WARMING_UP ? qsTr("Warming Up") :
                                (((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_GENERATING) === MAVLink.MAV_GENERATOR_STATUS_FLAG_GENERATING) || ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_IDLE) === MAVLink.MAV_GENERATOR_STATUS_FLAG_IDLE) ? qsTr("Running") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_START_INHIBITED) === MAVLink.MAV_GENERATOR_STATUS_FLAG_START_INHIBITED ? qsTr("Inhibited"):
                                qsTr("Unknown"))));
                            }
                            QGCLabel { text:
                                (generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERTEMP_WARNING) === MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERTEMP_WARNING ? qsTr("Engine Over Temp") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_REDUCED_POWER) === MAVLink.MAV_GENERATOR_STATUS_FLAG_REDUCED_POWER ? qsTr("Reduced Power") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_ELECTRONICS_OVERTEMP_WARNING) === MAVLink.MAV_GENERATOR_STATUS_FLAG_ELECTRONICS_OVERTEMP_WARNING ? qsTr("Coil Over Temp") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_COMMUNICATION_WARNING) === MAVLink.MAV_GENERATOR_STATUS_FLAG_COMMUNICATION_WARNING ? qsTr("Communication Error") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERCURRENT_FAULT) === MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERCURRENT_FAULT ? qsTr("Over Current") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERVOLTAGE_FAULT) === MAVLink.MAV_GENERATOR_STATUS_FLAG_OVERVOLTAGE_FAULT ? qsTr("Over Voltage") :
                                ((generator.status.rawValue & MAVLink.MAV_GENERATOR_STATUS_FLAG_MAINTENANCE_REQUIRED) === MAVLink.MAV_GENERATOR_STATUS_FLAG_MAINTENANCE_REQUIRED ? qsTr("Maintainance required") :
                                qsTr("None")))))));
                            }
                            QGCLabel { text: generator.voltage.valueString + " " + generator.voltage.units }
                            QGCLabel { text: generator.current.valueString + " " + generator.current.units }
                            QGCLabel { text: generator.rpm.valueString + " " + generator.rpm.units }
                            QGCLabel { text: generator.genTemp.valueString + " " + generator.genTemp.units }
                            QGCLabel { text: generator.coilTemp.valueString + " " + generator.coilTemp.units }
                            QGCLabel { text: generator.runtime.valueString }
                            QGCLabel { text: generator.timeToService.valueString }
                        }
                    }
                }
            }
        }
    }
}
