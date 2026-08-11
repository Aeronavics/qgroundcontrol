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
    id:             systemPage
    pageComponent:  systemPageComponent

    property int flightMode: controller.flightMode
    property int flightChannel: controller.flightChannel
    property int bindingStatus: controller.bindingStatus

    Component {
        id: systemPageComponent

        Column {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            QGCPalette { id: ggcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            Component {
                id: systemComponent

                Column {
                    spacing: _margins

                    RowLayout {
                        Item {
                            Layout.preferredWidth: 300
                            Layout.fillHeight: true

                            QGCLabel {
                                text: "Flight Mode"
                                font.pointSize: 20
                            }
                        }
                        Button {
                            id: flightModeButton
                            text: flightMode == 0 ? "OFF" : flightMode == 1 ? "3-Gear" : flightMode == 2 ? "6-Gear" : " "
                            font.pointSize: 15
                            Layout.fillHeight: true

                            onClicked: {
                                buttonPopup.open()
                            }
                        }

                        QGCPopupDialog {
                            id: buttonPopup
                            title: "Change Input"
                            buttons: StandardButton.Cancel
                            destroyOnClose: false

                            GridLayout {
                                columns: 3
                                Button {
                                    text: "OFF"
                                    font.pointSize: 15

                                    onClicked: {
                                        controller.callSetFlightMode(0)
                                        flightModeButton.text = "OFF"
                                        buttonPopup.close()
                                    }
                                }

                                Button {
                                    text: "3-Gear"
                                    font.pointSize: 15

                                    onClicked: {
                                        controller.callSetFlightMode(1)
                                        flightModeButton.text = "3-Gear"
                                        buttonPopup.close()
                                    }
                                }

                                Button {
                                    text: "6-Gear"
                                    font.pointSize: 15

                                    onClicked: {
                                        controller.callSetFlightMode(2)
                                        flightModeButton.text = "6-Gear"
                                        buttonPopup.close()
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Item {
                            Layout.preferredWidth: 300
                            Layout.fillHeight: true

                            QGCLabel {
                                text: "Flight Channel"
                                font.pointSize: 20
                            }
                        }
                        ComboBox {
                            model: 16
                            currentIndex: flightChannel - 1
                            delegate: ItemDelegate {
                                text: index + 1
                                width: parent.width
                            }
                            displayText: currentIndex + 1
                            onActivated: {
                                controller.callSetFlightChannel(currentIndex + 1)
                            }
                        }
                    }
                    Button {
                        text: bindingStatus == 0 ? "Bind" : bindingStatus == 1 || bindingStatus == 2 ? "Stop Binding" : bindingStatus == 3 ? "Bound" : "Unknown"
                        onClicked: {
                            bindingStatus == 0 || bindingStatus == 3 ? controller.startBinding() : controller.stopBinding()
                        }
                    }

                    Button {
                        text: "Stop Binding"
                        onClicked: {
                           controller.stopBinding()
                        }
                    }
                } // Column
            } // Component - channelComponent
            Loader {
                sourceComponent:    systemComponent
            }
        } // Flow
    } // Component - channelPageComponent
} // SetupView
