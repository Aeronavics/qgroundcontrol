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
    id:             buttonPage
    pageComponent:  buttonPageComponent

    Component {
        id: buttonPageComponent

        Flow {
            id:         flowLayout
            width:      availableWidth
            spacing:    _margins

            FactPanelController { id: controller; }

            QGCPalette { id: ggcPal; colorGroupEnabled: true }

            property real _margins:         ScreenTools.defaultFontPixelHeight
            property real _innerMargin:     _margins / 2
            property bool _showIcon:        !ScreenTools.isTinyScreen

            Component {
                id: buttonComponent

                Column {
                    spacing: _margins

                    GridLayout {
                        id:             gridLayout
                        columnSpacing:  _margins
                        rowSpacing:     _margins
                        columns:        2
                        QGCLabel { text: qsTr("S1") }
                        Switch {

                        }
                        QGCLabel { text: qsTr("S2") }
                        Switch {

                        }
                        QGCLabel { text: qsTr("S3") }
                        Switch {

                        }
                    } // GridLayout
                } // Column
            } // Component - channelComponent

            Column {
                spacing: _margins / 2

                Rectangle {
                    width:  parent.width
                    height: buttonLoader.y + buttonLoader.height + _margins
                    color:  ggcPal.windowShade

                    Loader {
                        id:                 buttonLoader
                        anchors.margins:    _margins
                        anchors.top:        parent.top
                        anchors.left:       parent.left
                        sourceComponent:    buttonComponent
                    }
                } // Rectangle
            } // Column - channel Settings
        } // Flow
    } // Component - channelPageComponent
} // SetupView
