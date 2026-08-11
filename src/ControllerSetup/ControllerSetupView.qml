/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts  1.2

import QGroundControl                       1.0
import QGroundControl.AutoPilotPlugin       1.0
import QGroundControl.Palette               1.0
import QGroundControl.Controls              1.0
import QGroundControl.ScreenTools           1.0
import QGroundControl.MultiVehicleManager   1.0
import QGroundControl.Controllers           1.0

Rectangle {
    id:     setupView
    color:  qgcPal.window
    z:      QGroundControl.zOrderTopMost

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    ExclusiveGroup { id: setupButtonGroup }

    readonly property real      _defaultTextHeight: ScreenTools.defaultFontPixelHeight
    readonly property real      _defaultTextWidth:  ScreenTools.defaultFontPixelWidth
    readonly property real      _horizontalMargin:  _defaultTextWidth / 2
    readonly property real      _verticalMargin:    _defaultTextHeight / 2
    readonly property real      _buttonWidth:       _defaultTextWidth * 18
    readonly property string    _armedVehicleText:  qsTr("This operation cannot be performed while the vehicle is armed.")

    property bool   _vehicleArmed:                  QGroundControl.multiVehicleManager.activeVehicle ? QGroundControl.multiVehicleManager.activeVehicle.armed : false
    property string _messagePanelText:              qsTr("missing message panel text")
    property bool   _fullParameterVehicleAvailable: QGroundControl.multiVehicleManager.parameterReadyVehicleAvailable && !QGroundControl.multiVehicleManager.activeVehicle.parameterManager.missingParameters
    property bool   _vehicleConnected:              QGroundControl.multiVehicleManager.activeVehicle ? !QGroundControl.multiVehicleManager.activeVehicle.vehicleLinkManager.communicationLost : false
    property var    _corePlugin:                    QGroundControl.corePlugin

    function showSummaryPanel() {
        if (mainWindow.preventViewSwitch()) {
            return
        }
        _showSummaryPanel()
    }

    function _showSummaryPanel() {
        // if (!_vehicleConnected) {
        //     panelLoader.setSourceComponent(disconnectedVehicleSummaryComponent)
        // } else {
        //     panelLoader.setSource("ChannelSetup.qml")
        // }
        panelLoader.setSource("ChannelSetup.qml")
        channelsButton.checked = true
    }

    function showPanel(button, qmlSource) {
        if (mainWindow.preventViewSwitch()) {
            return
        }
        button.checked = true
        panelLoader.setSource(qmlSource)
    }

    ControllerHandler {
        id: controller
    }


    Component.onCompleted: _showSummaryPanel()

    Component {
        id: disconnectedVehicleSummaryComponent
        Rectangle {
            color: qgcPal.windowShade
            QGCLabel {
                anchors.margins:        _defaultTextWidth * 2
                anchors.fill:           parent
                verticalAlignment:      Text.AlignVCenter
                horizontalAlignment:    Text.AlignHCenter
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.largeFontPointSize
                text:                   qsTr("Controller settings and info will display after connecting your vehicle.")
            }
        }
    }

    QGCFlickable {
        id:                 buttonScroll
        width:              buttonColumn.width
        anchors.topMargin:  _defaultTextHeight / 2
        anchors.top:        parent.top
        anchors.bottom:     parent.bottom
        anchors.leftMargin: _horizontalMargin
        anchors.left:       parent.left
        contentHeight:      buttonColumn.height
        flickableDirection: Flickable.VerticalFlick
        clip:               true

        ColumnLayout {
            id:         buttonColumn
            spacing:    _defaultTextHeight / 2

            SubMenuButton {
                id:                 channelsButton
                imageResource:      "/qmlimages/VehicleSummaryIcon.png"
                setupIndicator:     false
                checked:            true
                exclusiveGroup:     setupButtonGroup
                text:               qsTr("Channels")
                Layout.fillWidth:   true

                onClicked: showSummaryPanel()
            }

            SubMenuButton {
                id:                 calibrationButton
                imageResource:      "/qmlimages/FirmwareUpgradeIcon.png"
                setupIndicator:     false
                exclusiveGroup:     setupButtonGroup
                text:               qsTr("Calibration")
                Layout.fillWidth:   true

                onClicked: showPanel(this, "ControllerCalibration.qml")
            }
            SubMenuButton {
                id:                 systemButton
                imageResource:      "/qmlimages/FirmwareUpgradeIcon.png"
                setupIndicator:     false
                exclusiveGroup:     setupButtonGroup
                text:               qsTr("System")
                Layout.fillWidth:   true

                onClicked: showPanel(this, "ControllerSystem.qml")
            }
            // SubMenuButton {
            //     id:                 calibrationButton
            //     imageResource:      "/qmlimages/FirmwareUpgradeIcon.png"
            //     setupIndicator:     false
            //     exclusiveGroup:     setupButtonGroup
            //     text:               qsTr("RC Calibration")
            //     Layout.fillWidth:   true

            //     onClicked: showPanel(this, "RCCalibration.qml")
            // }
            // SubMenuButton {
            //     id:                 systemButton
            //     imageResource:      "/qmlimages/FirmwareUpgradeIcon.png"
            //     setupIndicator:     false
            //     exclusiveGroup:     setupButtonGroup
            //     text:               qsTr("System")
            //     Layout.fillWidth:   true

            //     onClicked: showPanel(this, "SystemSetup.qml")
            // }
        }
    }

    Rectangle {
        id:                     divider
        anchors.topMargin:      _verticalMargin
        anchors.bottomMargin:   _verticalMargin
        anchors.leftMargin:     _horizontalMargin
        anchors.left:           buttonScroll.right
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom
        width:                  1
        color:                  qgcPal.windowShade
    }

    Loader {
        id:                     panelLoader
        anchors.topMargin:      _verticalMargin
        anchors.bottomMargin:   _verticalMargin
        anchors.leftMargin:     _horizontalMargin
        anchors.rightMargin:    _horizontalMargin
        anchors.left:           divider.right
        anchors.right:          parent.right
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom

        function setSource(source, vehicleComponent) {
            panelLoader.source = ""
            panelLoader.vehicleComponent = vehicleComponent
            panelLoader.source = source
        }

        function setSourceComponent(sourceComponent, vehicleComponent) {
            panelLoader.sourceComponent = undefined
            panelLoader.vehicleComponent = vehicleComponent
            panelLoader.sourceComponent = sourceComponent
        }

        property var vehicleComponent
    }
}
