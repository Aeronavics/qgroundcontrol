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
    id:             channelPage
    pageComponent:  channelPageComponent

    property var channels: controller.channels
    property var channelMappings: controller.channelMappings
    property var channelReverses: controller.channelReverses

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
                        model: 16

                        RowLayout{
                            spacing: _margins
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
                                checked: channelReverses[index]

                                contentItem: Text {
                                    text: reverseSwitch.checked ? "R" : ""
                                    color: reverseSwitch.checked ? "#212121" : "#9E9E9E"
                                }
                                onToggled: controller.setChannelReverse(index + 1, reverseSwitch.checked)
                            }
                            Button {
                                text: channelMappings[index]
                                font.pointSize: 15
                                Layout.fillHeight: true
                            }
                        }
                    }
                } // Column
            } // Component - channelComponent
            Loader {
                sourceComponent:    channelComponent
            }

            // Column {
            //     spacing: _margins / 2

            //     Rectangle {
            //         width:  parent.width
            //         height: channelLoader.y + channelLoader.height + _margins
            //         color:  ggcPal.colorBlue

            //         Loader {
            //             id:                 channelLoader
            //             anchors.margins:    _margins
            //             anchors.top:        parent.top
            //             anchors.left:       parent.left
            //             sourceComponent:    channelComponent
            //         }
            //     } // Rectangle
            // } // Column - channel Settings
        } // Flow
    } // Component - channelPageComponent
} // SetupView
