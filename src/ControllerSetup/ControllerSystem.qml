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
    property int deadzone: controller.deadzone

    Component {
        id: systemPageComponent

        Column {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            QGCPalette { id: qgcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen
            property real _cardWidth:       ScreenTools.defaultFontPixelWidth * 55
            property real _labelWidth:      ScreenTools.defaultFontPixelWidth * 21

            //-----------------------------------------------------------------
            //-- Flight Mode
            Component {
                id: flightModeComponent

                Column {
                    spacing: _margins / 2

                    QGCLabel {
                        text:           qsTr("Flight Mode")
                        font.family:    ScreenTools.demiboldFontFamily
                    }
                    QGCLabel {
                        text:       qsTr("Assign the M1-M6 flight-mode switch to a channel and set its gearing.")
                        opacity:    0.7
                        width:      _cardWidth
                        wrapMode:   Text.WordWrap
                    }

                    Rectangle {
                        width:  _cardWidth
                        height: flightModeGrid.y + flightModeGrid.height + _margins
                        color:  qgcPal.windowShade

                        GridLayout {
                            id:                 flightModeGrid
                            anchors.margins:    _margins
                            anchors.top:        parent.top
                            anchors.left:       parent.left
                            width:              parent.width - (_margins * 2)
                            columnSpacing:      _margins
                            rowSpacing:         _innerMargin
                            columns:            2

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Mode:") }
                            QGCButton {
                                id:                 flightModeButton
                                text:               flightMode === 0 ? qsTr("OFF") : flightMode === 1 ? qsTr("3 Button") : flightMode === 2 ? qsTr("6 Button") : " "
                                Layout.fillWidth:   true

                                onClicked: flightModePopup.open()
                            }

                            QGCPopupDialog {
                                id:                 flightModePopup
                                title:              qsTr("Change Mode")
                                buttons:            StandardButton.Cancel
                                destroyOnClose:     false

                                GridLayout {
                                    columns: 3

                                    QGCButton {
                                        text: qsTr("OFF")
                                        onClicked: {
                                            controller.callSetFlightMode(0)
                                            flightModePopup.close()
                                        }
                                    }

                                    QGCButton {
                                        text: qsTr("3 Button")
                                        onClicked: {
                                            controller.callSetFlightMode(1)
                                            flightModePopup.close()
                                        }
                                    }

                                    QGCButton {
                                        text: qsTr("6 Button")
                                        onClicked: {
                                            controller.callSetFlightMode(2)
                                            flightModePopup.close()
                                        }
                                    }
                                }
                            }

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Flight Mode Channel:") }
                            QGCComboBox {
                                Layout.fillWidth:   true
                                centeredLabel:      true
                                model:              [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]
                                currentIndex:       flightChannel - 1

                                onActivated: controller.callSetFlightChannel(currentIndex + 1)
                            }
                        } // GridLayout
                    } // Rectangle - Flight Mode Settings
                } // Column - Flight Mode Settings
            }

            Loader { sourceComponent: flightModeComponent }

            //-----------------------------------------------------------------
            //-- Deadzone
            Component {
                id: deadzoneComponent

                Column {
                    spacing: _margins / 2

                    QGCLabel {
                        text:           qsTr("Deadzone")
                        font.family:    ScreenTools.demiboldFontFamily
                    }
                    QGCLabel {
                        text:       qsTr("Sets how far a stick can move from centre before its position is reported as changed.")
                        opacity:    0.7
                        width:      _cardWidth
                        wrapMode:   Text.WordWrap
                    }

                    Rectangle {
                        width:  _cardWidth
                        height: deadzoneGrid.y + deadzoneGrid.height + _margins
                        color:  qgcPal.windowShade

                        GridLayout {
                            id:                 deadzoneGrid
                            anchors.margins:    _margins
                            anchors.top:        parent.top
                            anchors.left:       parent.left
                            width:              parent.width - (_margins * 2)
                            columnSpacing:      _margins
                            rowSpacing:         _innerMargin
                            columns:            2

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Stick Deadzone:") }
                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            _innerMargin

                                QGCSlider {
                                    id:                         deadzoneSlider
                                    Layout.fillWidth:           true
                                    minimumValue:               10
                                    maximumValue:               80
                                    stepSize:                   1
                                    implicitHeight:             ScreenTools.defaultFontPixelHeight * 2
                                    updateValueWhileDragging:   false
                                    value:                      deadzone

                                    onValueChanged: controller.callSetDeadzone(value)
                                }

                                QGCTextField {
                                    id:                     deadzoneField
                                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 5
                                    numericValuesOnly:      true
                                    validator:              IntValidator { bottom: 10; top: 80 }
                                    text:                   activeFocus ? text : deadzone.toString()

                                    onEditingFinished: {
                                        var v = parseInt(text)
                                        if (!isNaN(v)) {
                                            controller.callSetDeadzone(v)
                                        }

                                        if (ScreenTools.isMobile) {
                                            focus = false
                                        }
                                    }
                                }
                            }
                        } // GridLayout
                    } // Rectangle - Deadzone Settings
                } // Column - Deadzone Settings
            }

            Loader { sourceComponent: deadzoneComponent }

            //-----------------------------------------------------------------
            //-- Binding
            Component {
                id: bindingComponent

                Column {
                    spacing: _margins / 2

                    QGCLabel {
                        text:           qsTr("Binding")
                        font.family:    ScreenTools.demiboldFontFamily
                    }
                    QGCLabel {
                        text:       qsTr("Pairs this controller with a powered air unit within range.")
                        opacity:    0.7
                        width:      _cardWidth
                        wrapMode:   Text.WordWrap
                    }

                    Rectangle {
                        width:  _cardWidth
                        height: bindingGrid.y + bindingGrid.height + _margins
                        color:  qgcPal.windowShade

                        GridLayout {
                            id:                 bindingGrid
                            anchors.margins:    _margins
                            anchors.top:        parent.top
                            anchors.left:       parent.left
                            width:              parent.width - (_margins * 2)
                            columnSpacing:      _margins
                            rowSpacing:         _innerMargin
                            columns:            2

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Ground Unit:") }
                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            _innerMargin

                                // Status dot, coloured to match the button's
                                // state rather than relying on the text alone.
                                Rectangle {
                                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 0.6
                                    Layout.preferredHeight: width
                                    radius:                 width / 2
                                    color:
                                        bindingStatus === 3 ? qgcPal.colorGreen :
                                        bindingStatus === 1 || bindingStatus === 2 || bindingStatus === 4 ? qgcPal.colorOrange :
                                                               qgcPal.colorGrey
                                }

                                QGCButton {
                                    Layout.fillWidth:   true
                                    text:
                                        bindingStatus === 0 ? qsTr("Bind") :
                                        bindingStatus === 1 || bindingStatus === 2 || bindingStatus === 4 ? qsTr("Binding") :
                                        bindingStatus === 3 ? qsTr("Bound") :
                                                               qsTr("Unknown")
                                    enabled:    bindingStatus === 0

                                    onClicked: controller.startBinding()
                                }
                            }
                        } // GridLayout
                    } // Rectangle - Binding Settings
                } // Column - Binding Settings
            }

            Loader { sourceComponent: bindingComponent }
        } // Column
    } // Component - systemPageComponent
} // SetupView
