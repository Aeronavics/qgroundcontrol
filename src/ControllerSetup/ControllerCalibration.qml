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

            Component {
                id: calibrationComponent

                ColumnLayout {
                    spacing: _margins

                    Rectangle {
                        border.color: qgcPal.text
                        border.width: 1
                        width: 700
                        height: 200

                        Column {
                            spacing: _margins
                            QGCLabel {
                                text:
                                    stickCalibrationState == 0 ? "Click to start Joystick Calibration" :
                                    stickCalibrationState == 1 ? "Center Sticks" :
                                    stickCalibrationState == 2 ? "Move sticks to min and max values" :
                                    stickCalibrationState == 3 ? "Joystick Calibration Complete" :
                                    stickCalibrationState == 4 ? "Joystick Calibration Failed" :
                                                            "Click to start Joystick Calibration"
                                font.pointSize: 20
                            }

                            Button {
                                id: stickCalibrationButton
                                text: "Start Stick Calibration"
                                enabled: stickCalibrationState !== 1 && stickCalibrationState !== 2
                                onClicked: {
                                    controller.callStartStickCalibration()
                                }
                            }
                        }
                    }

                    Rectangle {
                        border.color: qgcPal.text
                        border.width: 1
                        width: 700
                        height: 200

                        Column {
                            spacing: _margins
                            QGCLabel {
                                text:
                                    dialCalibrationState == 0 ? "Click to start Dial Calibration" :
                                    dialCalibrationState == 1 ? "Center Dials" :
                                    dialCalibrationState == 2 ? "Move dial to min and max values" :
                                    dialCalibrationState == 3 ? "Dial Calibration Complete" :
                                    dialCalibrationState == 4 ? "Dial Calibration Failed" :
                                                            "Click to start Dial Calibration"
                                font.pointSize: 20
                            }

                            Button {
                                id: dialCalibrationButton
                                text: "Start Dial Calibration"
                                enabled: dialCalibrationState !== 1 && dialCalibrationState !== 2
                                onClicked: {
                                    controller.callStartDialCalibration()
                                }
                            }
                        }
                    }
                } // Column
            } // Component - channelComponent
            Loader {
                sourceComponent:    calibrationComponent
            }
        } // Flow
    } // Component - channelPageCom
}
