import QtQuick              2.15
import QtQuick.Controls     2.15
import QtGraphicalEffects   1.0
import QtQuick.Layouts      1.2
import QtQuick.Dialogs  1.2

import QGroundControl               1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controllers   1.0

SetupPage {
    id:             calibrationPage
    pageComponent:  calibrationPageComponent

    property int stickCalibrationState: controller.stickCalibrationState
    property int dialCalibrationState: controller.dialCalibrationState
    property var rawAnalog: controller.rawAnalog

    readonly property int _j1: 0
    readonly property int _j2: 1
    readonly property int _j3: 2
    readonly property int _j4: 3
    readonly property int _ld: 4
    readonly property int _rd: 5
    readonly property int _hatHorizontal: 9
    readonly property int _hatVertical:   8

    readonly property real _rawMax: 100

    Component {
        id: calibrationPageComponent

        Column {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            QGCPalette { id: qgcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen
            // 0 = Joystick Calibration tab, 1 = Dial tab.
            property int  _tab:             0

            // -------------------------------------------------------------
            //-- Joystick / Dial segmented toggle
            Rectangle {
                id:                         tabBar
                width:                      ScreenTools.defaultFontPixelWidth * 40
                height:                     ScreenTools.defaultFontPixelHeight * 2
                radius:                     height / 2
                color:                      qgcPal.windowShade
                anchors.horizontalCenter:   parent.horizontalCenter

                RowLayout {
                    anchors.fill:       parent
                    anchors.margins:    4
                    spacing:            4

                    Rectangle {
                        Layout.fillWidth:   true
                        Layout.fillHeight:  true
                        radius:             height / 2
                        color:              flowLayout._tab === 0 ? qgcPal.window : "transparent"

                        QGCLabel {
                            anchors.centerIn:       parent
                            width:                  parent.width
                            horizontalAlignment:    Text.AlignHCenter
                            wrapMode:               Text.WordWrap
                            font.family:            ScreenTools.demiboldFontFamily
                            text:                   qsTr("Joystick")
                        }

                        MouseArea { anchors.fill: parent; onClicked: flowLayout._tab = 0 }
                    }

                    Rectangle {
                        Layout.fillWidth:   true
                        Layout.fillHeight:  true
                        radius:             height / 2
                        color:              flowLayout._tab === 1 ? qgcPal.window : "transparent"

                        QGCLabel {
                            anchors.centerIn:   parent
                            font.family:        ScreenTools.demiboldFontFamily
                            text:               qsTr("Dial")
                        }

                        MouseArea { anchors.fill: parent; onClicked: flowLayout._tab = 1 }
                    }
                }
            } // Rectangle - tabBar

            //-----------------------------------------------------------------
            //-- Joystick Calibration
            Column {
                width:      flowLayout.width
                spacing:    _margins
                visible:    flowLayout._tab === 0

                Row {
                    id:                         sticksRow
                    spacing:                    _margins * 2
                    anchors.horizontalCenter:   parent.horizontalCenter

                    CalibrationStickIndicator { id: leftStick; xValue: rawAnalog[_j4]; yValue: rawAnalog[_j3]; rawMax: _rawMax; diameter: ScreenTools.defaultFontPixelHeight * 8 }
                    CalibrationStickIndicator { xValue: rawAnalog[_j1]; yValue: rawAnalog[_j2]; rawMax: _rawMax; diameter: ScreenTools.defaultFontPixelHeight * 8 }
                }

                CalibrationStickIndicator {
                    x:                          sticksRow.x + leftStick.width / 2 - width / 2
                    xValue:                     rawAnalog[_hatHorizontal]
                    yValue:                     rawAnalog[_hatVertical]
                    rawMax:                     _rawMax
                    diameter:                   ScreenTools.defaultFontPixelHeight * 5
                }

                QGCLabel {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    text:
                        stickCalibrationState == 0 ? qsTr("Stop both joysticks at the middle position") :
                        stickCalibrationState == 1 ? qsTr("Center Sticks") :
                        stickCalibrationState == 2 ? qsTr("Move sticks to min and max values") :
                        stickCalibrationState == 3 ? qsTr("Joystick Calibration Complete") :
                        stickCalibrationState == 4 ? qsTr("Joystick Calibration Failed") :
                                                qsTr("Stop both joysticks at the middle position")
                }

                Rectangle { width: parent.width; height: 1; color: qgcPal.windowShade }

                QGCButton {
                    id:                         stickCalibrationButton
                    anchors.horizontalCenter:   parent.horizontalCenter
                    text:                       qsTr("Calibrate Now")
                    backRadius:                 height / 2
                    primary:                    true
                    enabled:                    stickCalibrationState !== 1 && stickCalibrationState !== 2

                    onClicked: controller.callStartStickCalibration()
                }
            } // Column - Joystick Calibration

            //-----------------------------------------------------------------
            //-- Dial Calibration
            Column {
                width:      flowLayout.width
                spacing:    _margins
                visible:    flowLayout._tab === 1

                QGCLabel {
                    text:           qsTr("Left Dial Calibration")
                    font.family:    ScreenTools.demiboldFontFamily
                    anchors.horizontalCenter:   parent.horizontalCenter
                }
                CalibrationDialIndicator { label: "LD"; value: rawAnalog[_ld]; rawMax: _rawMax; anchors.horizontalCenter:   parent.horizontalCenter}

                QGCLabel {
                    text:           qsTr("Right Dial Wheel Calibration")
                    font.family:    ScreenTools.demiboldFontFamily
                    anchors.horizontalCenter:   parent.horizontalCenter
                }
                CalibrationDialIndicator { label: "RD"; value: rawAnalog[_rd]; rawMax: _rawMax; anchors.horizontalCenter:   parent.horizontalCenter }

                QGCLabel {
                    text:
                        dialCalibrationState == 0 ? qsTr("Stop all dials at the middle position") :
                        dialCalibrationState == 1 ? qsTr("Center Dials") :
                        dialCalibrationState == 2 ? qsTr("Move dial to min and max values") :
                        dialCalibrationState == 3 ? qsTr("Dial Calibration Complete") :
                        dialCalibrationState == 4 ? qsTr("Dial Calibration Failed") :
                                                qsTr("Stop all dials at the middle position")
                    anchors.horizontalCenter:   parent.horizontalCenter
                }

                Rectangle { width: parent.width; height: 1; color: qgcPal.windowShade }

                QGCButton {
                    id:         dialCalibrationButton
                    text:       qsTr("Calibrate Now")
                    backRadius: height / 2
                    primary:    true
                    enabled:    dialCalibrationState !== 1 && dialCalibrationState !== 2
                    anchors.horizontalCenter:   parent.horizontalCenter

                    onClicked: controller.callStartDialCalibration()
                }
            } // Column - Dial Calibration
        } // Column
    } // Component - calibrationPageComponent
} // SetupView
