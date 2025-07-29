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
    id:             generatorPage
    pageComponent:  generatorPageComponent

    Component {
        id: generatorPageComponent

        Flow {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            FactPanelController { id: controller; }

            QGCPalette { id: ggcPal; colorGroupEnabled: true }

            property Fact _generator:                       controller.getParameterFact(-1, "GEN_TYPE")
            property bool _generatorEnabled:                _generator.rawValue === 4

            property Fact _generatorLowFS:                  controller.getParameterFact(-1, "GEN_LOW_FS", false /* reportMissing */)
            property Fact _generatorCritFS:                 controller.getParameterFact(-1, "GEN_CRIT_FS", false /* reportMissing */)
            property Fact _generatorOffFS:                  controller.getParameterFact(-1, "GEN_OFF_FS", false /* reportMissing */)
            property Fact _generatorErrorFS:                controller.getParameterFact(-1, "GEN_ERROR_FS", false /* reportMissing */)

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            Component {
                id: generatorComponent

                Column {
                    spacing: _margins

                    GridLayout {
                        id:             gridLayout
                        columnSpacing:  _margins
                        rowSpacing:     _margins
                        columns:        2
                        QGCLabel { text: qsTr("Low Fuel Failsafe:") }
                        FactComboBox {
                            fact:               generatorLowFS
                            indexModel:         false
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Critical Fuel Failsafe:") }
                        FactComboBox {
                            fact:               generatorCritFS
                            indexModel:         false
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Power Loss Failsafe:") }
                        FactComboBox {
                            fact:               generatorOffFS
                            indexModel:         false
                            Layout.fillWidth:   true
                        }

                        QGCLabel { text: qsTr("Error Failsafe:") }
                        FactComboBox {
                            fact:               generatorErrorFS
                            indexModel:         false
                            Layout.fillWidth:   true
                        }
                    } // GridLayout
                } // Column
            }

            Column {
                spacing: _margins / 2
                visible: _generatorEnabled

                QGCLabel {
                    text:       qsTr("Generator Failsafe Settings")
                    font.family: ScreenTools.demiboldFontFamily
                }

                Rectangle {
                    width:  generatorLoader.x + generatorLoader.width + _margins
                    height: generatorLoader.y + generatorLoader.height + _margins
                    color:  ggcPal.windowShade

                    Loader {
                        id:                 generatorLoader
                        anchors.margins:    _margins
                        anchors.top:        parent.top
                        anchors.left:       parent.left
                        sourceComponent:    generatorComponent

                        property Fact generatorLowFS:   _generatorLowFS
                        property Fact generatorCritFS:  _generatorCritFS
                        property Fact generatorOffFS:   _generatorOffFS
                        property Fact generatorErrorFS: _generatorErrorFS
                    }
                } // Rectangle
            } // Column - generator Settings
        } // Flow
    } // Component - generatorPageComponent
} // SetupView
