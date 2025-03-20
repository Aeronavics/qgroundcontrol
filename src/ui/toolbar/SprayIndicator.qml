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
//-- Spray Indicator
Item {
    id:             _root
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width: (sprayValuesColumn.x + sprayValuesColumn.width) * 1.1

    property bool showIndicator: true

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    property var sprayer: _activeVehicle.spray

    function getSprayColor() {
        if (sprayer.mesFlowrate.rawValue > 0)
        {
            return qgcPal.colorGreen
        }
        else if (sprayer.sprayRemaining.rawValue < 10)
        {
            return qgcPal.colorRed
        }
        else if (sprayer.error.rawValue > 0)
        {
            if (sprayer.sprayRemaining.rawValue >= 10)
            {
                return qgcPal.colorOrange
            }
            else
            {
                return qgcPal.colorRed
            }
        }
        else if (sprayer.sprayRemaining.rawValue < 25)
        {
                return qgcPal.colorOrange
        }
        else if (sprayer.sprayRemaining.rawValue >= 25)
        {
                return qgcPal.text
        }
        else
        {
            return qgcPal.colorRed
        }
    }

    function getSprayPercentageText() {
        if (!isNaN(sprayer.sprayRemaining.rawValue)) {
            return sprayer.sprayRemaining.valueString + sprayer.sprayRemaining.units
        }
        return "__%"
    }

    function getSprayFlowrateText() {
        if (!isNaN(sprayer.mesFlowrate.rawValue)) {
            return sprayer.mesFlowrate.valueString + sprayer.mesFlowrate.units
        }
        return "__.__ ml/s"
    }

    QGCColoredImage {
        id:                 sprayImage
        anchors.top:        parent.top
        anchors.bottom:     parent.bottom
        width:              height
        sourceSize.height:  height
        source:             "/qmlimages/spray.svg"
        fillMode:           Image.PreserveAspectFit
        color:              getSprayColor()
    }

    Column {
        id:                     sprayValuesColumn
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin:     ScreenTools.defaultFontPixelWidth / 2
        anchors.left:           sprayImage.right

        QGCLabel {
            anchors.horizontalCenter:   flowrateValue.horizontalCenter
            text:                       getSprayPercentageText()
            color:                      getSprayColor()
        }

        QGCLabel {
            id:                     flowrateValue
            text:                   getSprayFlowrateText()
            color:                  getSprayColor()
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked: {
            mainWindow.showIndicatorPopup(_root, sprayPopup)
        }
    }

    Component {
        id: sprayPopup

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
                    text:               qsTr("Spray Status")
                    font.family:        ScreenTools.demiboldFontFamily
                }

                RowLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    ColumnLayout {
                        ColumnLayout {
                            spacing: 0

                            QGCLabel { text: qsTr("State"); }
                            QGCLabel { text: qsTr("Error"); }
                            QGCLabel { text: qsTr("Spray Remaining"); }
                            QGCLabel { text: qsTr("Volume Sprayed") }
                            QGCLabel { text: qsTr("Measured Flowrate") }
                            QGCLabel { text: qsTr("Desired Flowrate"); }
                            QGCLabel { text: qsTr("Pressure"); }
                        }
                    }

                    ColumnLayout {

                        ColumnLayout {
                            spacing: 0
                            QGCLabel { text:
                                    (sprayer.mesFlowrate.rawValue > 0) ? qsTr("Spraying") : qsTr("Stopped");
                            }
                            QGCLabel { text:
                                    (sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_1) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_1 ? qsTr("Possible Blockage") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_2) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_2 ? qsTr("Possible Blockage") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_3) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_3 ? qsTr("Possible Blockage") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_4) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_FLOW_RATE_4 ? qsTr("Possible Blockage") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_LOW_PRESSURE) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_LOW_PRESSURE ? qsTr("Low Pressure") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_OVER_PRESSURE) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_OVER_PRESSURE ? qsTr("Over Pressure") :
                                        ((sprayer.error.rawValue & MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_NO_SPRAY) === MAVLink.COM_AERONAVICS_SPRAYINFO_ERROR_NO_SPRAY ? qsTr("No Spray Remaining") :
                                        qsTr("None")))))));
                            }
                            QGCLabel { text: sprayer.sprayRemaining.valueString + " " + sprayer.sprayRemaining.units }
                            QGCLabel { text: sprayer.sprayedVolume.valueString + " " + sprayer.sprayedVolume.units }
                            QGCLabel { text: sprayer.mesFlowrate.valueString + " " + sprayer.mesFlowrate.units }
                            QGCLabel { text: sprayer.desFlowrate.valueString + " " + sprayer.desFlowrate.units }
                            QGCLabel { text: sprayer.mesPressure.valueString + " " + sprayer.mesPressure.units }
                        }
                    }
                }
            }
        }
    }
}
