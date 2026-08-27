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

            QGCPalette { id: qgcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            //-----------------------------------------------------------------
            //-- Channels
            Component {
                id: channelComponent

                Column {
                    spacing: _margins / 2

                    QGCLabel {
                        text:           qsTr("Channels")
                        font.family:    ScreenTools.demiboldFontFamily
                    }

                    Rectangle {
                        width:  channelColumn.x + channelColumn.width + _margins
                        height: channelColumn.y + channelColumn.height + _margins
                        color:  qgcPal.windowShade

                        Column {
                            id:                 channelColumn
                            anchors.margins:    _margins
                            anchors.top:        parent.top
                            anchors.left:       parent.left
                            spacing:            _innerMargin

                            RowLayout {
                                width:   1300
                                spacing: _margins

                                QGCLabel {
                                    Layout.preferredWidth: 50
                                    text:                   qsTr("Ch")
                                    font.family:            ScreenTools.demiboldFontFamily
                                }
                                Item { Layout.preferredWidth: 500 }
                                QGCLabel {
                                    Layout.preferredWidth: 120
                                    text:                   qsTr("Value")
                                    font.family:            ScreenTools.demiboldFontFamily
                                }
                                QGCLabel {
                                    Layout.preferredWidth: 120
                                    text:                   qsTr("Reverse")
                                    font.family:            ScreenTools.demiboldFontFamily
                                }
                                QGCLabel {
                                    Layout.preferredWidth: 150
                                    text:           qsTr("Button Mode")
                                    font.family:    ScreenTools.demiboldFontFamily
                                }
                                Item { Layout.fillWidth: true }
                                QGCLabel {
                                    Layout.preferredWidth: 150
                                    text:           qsTr("Input")
                                    font.family:    ScreenTools.demiboldFontFamily
                                }
                            }

                            Repeater {
                                id: rowRepeater
                                model: 16

                                RowLayout{
                                    width:   1300
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
                                            radius: Math.min(width, height) / 2
                                            color: qgcPal.window
                                            border.color: qgcPal.text
                                            border.width: 1
                                            implicitHeight: 20
                                        }

                                        contentItem: Item {

                                            Item {
                                                id:     channelProgressClip
                                                clip:   true
                                                height: parent.height

                                                width: Math.max(0, Math.min(1, channelProgress.visualPosition)) * parent.width

                                                Rectangle {
                                                    color:  qgcPal.colorGreen
                                                    radius: channelProgress.height / 2
                                                    width:  channelProgressClip.parent.width
                                                    height: parent.height
                                                }
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
                                            color: qgcPal.text
                                        }
                                        onToggled: controller.setChannelReverse(index + 1, reverseSwitch.checked)
                                    }
                                    QGCButton {
                                        property int buttonValue: functionButton.text in buttonMap ? parseInt(buttonMap[functionButton.text]) : -1
                                        text: buttonValue == 0 ? qsTr("Reset") : buttonValue == 1 ? qsTr("Lock") : buttonValue == 2 ? qsTr("3-Stage") : " "
                                        visible: buttonValue >= 0
                                        Layout.preferredWidth: 150
                                        Layout.fillHeight: true

                                        onClicked: controller.toggleButtonMode(functionButton.text)
                                    }
                                    // Spacer pushes only the function button (not
                                    // the Reset/Lock toggle above) flush right.
                                    Item
                                    {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                    }

                                    QGCButton {
                                        id:                 functionButton
                                        text:               isFlightMode ? qsTr("Flight") : channelMappings[index]
                                        Layout.fillHeight:  true
                                        Layout.preferredWidth: 150
                                        enabled:            isFlightMode ? false : true

                                        onClicked: {
                                            inputPopup.channel = index + 1
                                            inputPopup.open()
                                        }
                                    }
                                }
                            }

                            QGCPopupDialog {
                                id: inputPopup
                                title: qsTr("Change Input")
                                buttons: StandardButton.Cancel
                                destroyOnClose: false

                                property int channel: 0

                                GridLayout {
                                    columns: 7

                                    Repeater {
                                        model: 28

                                        QGCButton {
                                            text: inputNames[index]

                                            onClicked: {
                                                controller.callSetChannelMapping(inputPopup.channel, index)
                                                inputPopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        } // Column
                    } // Rectangle - Channel Settings
                } // Column - Channel Settings
            } // Component - channelComponent
            Loader {
                sourceComponent:    channelComponent
            }
        } // Column
    } // Component - channelPageComponent
} // SetupView
