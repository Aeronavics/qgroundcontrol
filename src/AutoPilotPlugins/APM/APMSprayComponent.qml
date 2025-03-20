/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick              2.3
import QtQuick.Controls     1.2
import QtGraphicalEffects   1.0
import QtQuick.Layouts      1.2

import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0

SetupPage {
    id:             sprayPage
    pageComponent:  sprayPageComponent

    Component {
        id: sprayPageComponent

        Flow {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            FactPanelController { id: controller; }

            QGCPalette { id: ggcPal; colorGroupEnabled: true }

            property Fact _sprayer:                         controller.getParameterFact(-1, "SPOT_ENABLE")
            property bool _SprayEnabled:                    _sprayer.rawValue !== 0

            property Fact _sprayMode:                       controller.getParameterFact(-1, "SPOT_MODE", false /* reportMissing */)
            property Fact _sprayFlowrateHigh:               controller.getParameterFact(-1, "SPOT_RATE_HIGH", false /* reportMissing */)
            property Fact _sprayFlowrateMid:                controller.getParameterFact(-1, "SPOT_RATE_MID", false /* reportMissing */)
            property Fact _sprayFlowrateLow:                controller.getParameterFact(-1, "SPOT_RATE_LOW", false /* reportMissing */)
            property Fact _sprayVolumeHigh:                 controller.getParameterFact(-1, "SPOT_VOL_HIGH", false /* reportMissing */)
            property Fact _sprayVolumeMid:                  controller.getParameterFact(-1, "SPOT_VOL_MID", false /* reportMissing */)
            property Fact _sprayVolumeLow:                  controller.getParameterFact(-1, "SPOT_VOL_LOW", false /* reportMissing */)
            property Fact _sprayPulse:                      controller.getParameterFact(-1, "SPOT_PULSE", false /* reportMissing */)

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            Component {
                id: spotSprayComponent

                Column {
                    spacing: _margins

                    GridLayout {
                        id:             gridLayout
                        columnSpacing:  _margins
                        rowSpacing:     _margins
                        columns:        2
                        QGCLabel { text: qsTr("Sprayer mode:") }
                        FactComboBox {
                            fact:               sprayMode
                            indexModel:         false
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("High Flowrate:") }
                        FactTextField {
                            fact:               sprayFlowrateHigh
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Medium Flowrate:") }
                        FactTextField {
                            fact:               sprayFlowrateMid
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Low Flowrate:") }
                        FactTextField {
                            fact:               sprayFlowrateLow
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("High Volume:") }
                        FactTextField {
                            fact:               sprayVolumeHigh
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Medium Volume:") }
                        FactTextField {
                            fact:               sprayVolumeMid
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Low Volume:") }
                        FactTextField {
                            fact:               sprayVolumeLow
                            showUnits:          true
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Pulse Volume:") }
                        FactTextField {
                            fact:               sprayPulse
                            showUnits:          true
                            Layout.fillWidth:   true
                        }
                    } // GridLayout
                } // Column
            }

            Column {
                spacing: _margins / 2
                visible: _batt1MonitorEnabled

                QGCLabel {
                    text:       qsTr("Sprayer Settings")
                    font.family: ScreenTools.demiboldFontFamily
                }

                Rectangle {
                    width:  sprayerLoader.x + sprayerLoader.width + _margins
                    height: sprayerLoader.y + sprayerLoader.height + _margins
                    color:  ggcPal.windowShade

                    Loader {
                        id:                 sprayerLoader
                        anchors.margins:    _margins
                        anchors.top:        parent.top
                        anchors.left:       parent.left
                        sourceComponent:    spotSprayComponent

                        property Fact sprayMode:                _sprayMode
                        property Fact sprayFlowrateHigh:        _sprayFlowrateHigh
                        property Fact sprayFlowrateMid:         _sprayFlowrateMid
                        property Fact sprayFlowrateLow:         _sprayFlowrateLow
                        property Fact sprayVolumeHigh:          _sprayVolumeHigh
                        property Fact sprayVolumeMid:           _sprayVolumeMid
                        property Fact sprayVolumeLow:           _sprayVolumeLow
                        property Fact sprayPulse:               _sprayPulse
                    }
                } // Rectangle
            } // Column - Spray Settings
        } // Flow
    } // Component - sprayPageComponent
} // SetupView
