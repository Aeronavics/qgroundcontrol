import QtQuick              2.15
import QtQuick.Controls     2.15
import QtGraphicalEffects   1.0
import QtQuick.Layouts      1.15
import QtQuick.Dialogs  1.2

import QGroundControl               1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controllers   1.0

SetupPage {
    id:             channelPage
    pageComponent:  channelPageComponent

    property var channels: controller.channels
    property var channelMappings: controller.channelMappings
    property var channelReverses: controller.channelReverses
    property int flightChannel: controller.flightChannel
    property var inputNames: controller.inputList
    property var buttonMap: controller.buttonMap

    Component {
        id: channelPageComponent

        Column {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            QGCPalette { id: ggcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            Component {
                id: channelComponent

                Column {
                    spacing: _margins
                    Repeater {
                        id: rowRepeater
                        model: 16

                        RowLayout{
                            spacing: _margins

                            property bool isFlightMode: index + 1 === flightChannel

                            Item {
                                Layout.preferredWidth: 50
                                Layout.fillHeight: true

                                QGCLabel {
                                    text: index + 1
                                    font.pointSize: 20
                                }
                            }
                            ProgressBar {
                                id: channelProgress
                                width: 500
                                Layout.fillHeight: true
                                from: 1050
                                to: 1950
                                value: channels[index]

                                background: Rectangle {
                                    radius: 25
                                    color: "lightgray"
                                    border.color: "gray"
                                    border.width: 1
                                    implicitHeight: 20
                                }

                                contentItem: Item {
                                    implicitWidth: channelProgress.implicitWidth
                                    implicitHeight: channelProgress.implicitHeight
                                    clip: true

                                    Rectangle
                                    {
                                        color: ggcPal.colorGreen
                                        radius: 25
                                        width: channelProgress.visualPosition * parent.width
                                        height: parent.height
                                    }
                                }
                            }
                            Item {
                                Layout.preferredWidth: 120
                                Layout.fillHeight: true

                                QGCLabel {
                                    text: channels[index]
                                    font.pointSize: 20
                                }
                            }
                            Switch {
                                id: reverseSwitch
                                Layout.preferredWidth: 120
                                Layout.fillHeight: true
                                checked: isFlightMode ? false : channelReverses[index]

                                enabled: isFlightMode ? false : true

                                contentItem: Text {
                                    text: reverseSwitch.checked ? "R" : ""
                                    color: reverseSwitch.checked ? "#212121" : "#9E9E9E"
                                }
                                onToggled: controller.setChannelReverse(index + 1, reverseSwitch.checked)
                            }
                            Item
                            {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }
                            Button {
                                property int buttonValue: functionButton.text in buttonMap ? parseInt(buttonMap[functionButton.text]) : -1
                                text: buttonValue == 0 ? "Reset" : buttonValue == 1 ? "Lock" : buttonValue == 2 ? "3-Stage" : " "
                                visible: buttonValue >= 0

                                onClicked:
                                {
                                    controller.toggleButtonMode(functionButton.text)
                                    buttonMap[functionButton.text] = toString((parseInt(buttonMap[functionButton.text]) + 1) % 3)
                                }
                            }

                            Button {
                                id: functionButton
                                text: isFlightMode ? "Flight" : channelMappings[index]
                                font.pointSize: 15
                                Layout.fillHeight: true
                                enabled: isFlightMode ? false : true

                                onClicked: {
                                    buttonPopup.channel = index + 1
                                    buttonPopup.open()
                                }
                            }

                            QGCPopupDialog {
                                id: buttonPopup
                                title: "Change Input"
                                buttons: StandardButton.Cancel
                                destroyOnClose: false

                                property int channel: 0

                                GridLayout {
                                    columns: 7

                                    Repeater {
                                        model: 28

                                        Button {
                                            text: inputNames[index]
                                            font.pointSize: 15

                                            onClicked: {
                                                controller.callSetChannelMapping(buttonPopup.channel, index)
                                                functionButton.text = inputNames[index]
                                                buttonPopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } // Column
            } // Component - channelComponent
            Loader {
                sourceComponent:    channelComponent
            }
        } // Flow
    } // Component - channelPageComponent
} // SetupView
