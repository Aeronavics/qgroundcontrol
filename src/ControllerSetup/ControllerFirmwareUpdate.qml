/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick              2.15
import QtQuick.Controls     2.15
import QtQuick.Layouts      1.15
import QtQuick.Dialogs      1.2

import QGroundControl               1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controllers   1.0

SetupPage {
    id:             firmwarePage
    pageComponent:  firmwarePageComponent

    // Keep these in sync with ControllerHandler::FpvUpdateState.
    readonly property int stIdle:       0
    readonly property int stAnnouncing: 1
    readonly property int stUploading:  2
    readonly property int stVerifying:  3
    readonly property int stCommitting: 4
    readonly property int stRebooting:  5
    readonly property int stSuccess:    6
    readonly property int stFailed:     7

    property int    updateState:    controller.fpvUpdateState
    property int    updateProgress: controller.fpvUpdateProgress
    property string updateStatus:   controller.fpvUpdateStatus
    property bool   linkUp:         controller.fpvLinkUp
    property string fpvVersion:     controller.fpvVersion

    // True from the moment the module starts being written until it is back.
    // Nothing destructive may be interrupted in this window.
    property bool   _inProgress:    updateState !== stIdle &&
                                    updateState !== stSuccess &&
                                    updateState !== stFailed
    property bool   _writing:       updateState === stCommitting ||
                                    updateState === stRebooting

    // The chosen file is resolved and held in C++: on Android the browser hands
    // back a content:// URI that has no filesystem path, so QML must not try to
    // parse it into one.
    // false = ground unit (192.168.144.12), true = air unit (192.168.144.11).
    property bool   targetAir:      false
    property bool   airPresent:     controller.airUnitPresent
    property string airVersion:     controller.airVersion

    property bool   fileChosen:     controller.firmwareSelected
    property string firmwareName:   controller.firmwareName
    property real   firmwareSize:   controller.firmwareSize

    Component.onCompleted: controller.callRefreshFpvVersion()

    Component {
        id: firmwarePageComponent

        Column {
            id:         layout
            width:      availableWidth
            spacing:    _margins

            QGCPalette { id: qgcPal; colorGroupEnabled: true }

            property real _margins:     ScreenTools.defaultFontPixelHeight
            property real _innerMargin: _margins / 2
            property real _cardWidth:   ScreenTools.defaultFontPixelWidth * 65
            property real _labelWidth:  ScreenTools.defaultFontPixelWidth * 21

            //-----------------------------------------------------------------
            //-- Firmware status
            Column {
                spacing: _margins / 2

                QGCLabel {
                    text:           qsTr("FPV Firmware")
                    font.family:    ScreenTools.demiboldFontFamily
                }
                QGCLabel {
                    text:       qsTr("Firmware for the video/telemetry link.")
                    opacity:    0.7
                    width:      _cardWidth
                    wrapMode:   Text.WordWrap
                }

                Rectangle {
                    width:  _cardWidth
                    height: statusGrid.y + statusGrid.height + _margins
                    color:  qgcPal.windowShade

                    GridLayout {
                        id:                 statusGrid
                        anchors.margins:    _margins
                        anchors.top:        parent.top
                        anchors.left:       parent.left
                        width:              parent.width - (_margins * 2)
                        columnSpacing:      _margins
                        rowSpacing:         _innerMargin
                        columns:            2

                        QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Status:") }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: _innerMargin

                            Rectangle {
                                width:  ScreenTools.defaultFontPixelHeight * 0.6
                                height: width
                                radius: width / 2
                                color:  linkUp ? qgcPal.colorGreen : qgcPal.colorRed
                            }
                            QGCLabel {
                                Layout.fillWidth:   true
                                text:               linkUp ? qsTr("Connected") : qsTr("Not detected")
                            }
                        }

                        QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Ground unit:") }
                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               fpvVersion !== "" ? fpvVersion : qsTr("Unknown")
                        }

                        QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Air unit:") }
                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               airPresent
                                                    ? (airVersion !== "" ? airVersion : qsTr("Connected"))
                                                    : qsTr("Not connected")
                            opacity:            airPresent ? 1 : 0.6
                        }

                        Item { width: 1; height: 1 }
                        QGCButton {
                            text:       qsTr("Refresh")
                            enabled:    !_inProgress
                            onClicked:  controller.callRefreshFpvVersion()
                        }
                    }
                }
            }

            //-----------------------------------------------------------------
            //-- The USB trap. This is the single most likely reason an update
            //-- fails, and it is invisible without being told, so say it plainly
            //-- rather than letting the user hit a string of timeouts.
            Rectangle {
                width:      _cardWidth
                height:     usbWarnColumn.height + _margins
                color:      qgcPal.windowShade
                border.color: qgcPal.colorOrange
                border.width: 1
                visible:    !linkUp && !_inProgress

                Column {
                    id:                 usbWarnColumn
                    anchors.centerIn:   parent
                    width:              parent.width - (_margins * 2)
                    spacing:            _innerMargin / 2

                    QGCLabel {
                        text:           qsTr("Ground unit not detected")
                        color:          qgcPal.colorOrange
                        font.family:    ScreenTools.demiboldFontFamily
                    }
                    QGCLabel {
                        width:      parent.width
                        wrapMode:   Text.WordWrap
                        text:       qsTr("Unplug any USB cable from the controller. Connecting USB switches the " +
                                         "controller's internal link away from the radio module, which makes the " +
                                         "ground unit unreachable and any update fail.")
                    }
                }
            }

            //-----------------------------------------------------------------
            //-- Firmware file + update
            Column {
                spacing: _margins / 2

                QGCLabel {
                    text:           qsTr("Update FPV Firmware")
                    font.family:    ScreenTools.demiboldFontFamily
                }
                QGCLabel {
                    text:       qsTr("Select the firmware supplied by SIYI for this unit. Both the ground and air " +
                                     "units take the SAME file (for example CX6653C-N…) — the unit rejects a file " +
                                     "whose name does not match its hardware.")
                    opacity:    0.7
                    width:      _cardWidth
                    wrapMode:   Text.WordWrap
                }

                Rectangle {
                    width:  _cardWidth
                    height: updateColumn.y + updateColumn.height + _margins
                    color:  qgcPal.windowShade

                    Column {
                        id:                 updateColumn
                        anchors.margins:    _margins
                        anchors.top:        parent.top
                        anchors.left:       parent.left
                        width:              parent.width - (_margins * 2)
                        spacing:            _innerMargin

                        RowLayout {
                            width:      parent.width
                            spacing:    _innerMargin

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Update:") }
                            QGCComboBox {
                                Layout.fillWidth:   true
                                centeredLabel:      true
                                enabled:            !_inProgress
                                model:              [qsTr("Ground unit"), qsTr("Air unit")]
                                currentIndex:       targetAir ? 1 : 0
                                onActivated:        targetAir = (currentIndex === 1)
                            }
                        }

                        RowLayout {
                            width:      parent.width
                            spacing:    _innerMargin

                            QGCLabel { Layout.preferredWidth: _labelWidth; text: qsTr("Firmware file:") }
                            QGCLabel {
                                Layout.fillWidth:   true
                                elide:              Text.ElideMiddle
                                text:               fileChosen
                                                        ? (firmwareSize > 0
                                                            ? qsTr("%1  (%2 MB)").arg(firmwareName).arg((firmwareSize / 1048576).toFixed(1))
                                                            : firmwareName)
                                                        : qsTr("None selected")
                                opacity:            fileChosen ? 1 : 0.6
                            }
                        }

                        RowLayout {
                            width:      parent.width
                            spacing:    _innerMargin

                            QGCButton {
                                text:       qsTr("Browse…")
                                enabled:    !_inProgress
                                onClicked:  fileDialog.open()
                            }

                            QGCButton {
                                primary:    true
                                enabled:    !_inProgress && fileChosen &&
                                            (targetAir ? airPresent : linkUp)
                                text:       targetAir ? qsTr("Update Air Unit") : qsTr("Update Ground Unit")
                                onClicked:  confirmDialog.open()
                            }
                        }

                        //-- Progress
                        Column {
                            width:      parent.width
                            spacing:    _innerMargin / 2
                            visible:    updateState !== stIdle

                            QGCLabel {
                                width:      parent.width
                                wrapMode:   Text.WordWrap
                                text:       updateStatus
                                color:      updateState === stFailed  ? qgcPal.colorRed   :
                                            updateState === stSuccess ? qgcPal.colorGreen :
                                                                        qgcPal.text
                            }

                            ProgressBar {
                                width:          parent.width
                                from:           0
                                to:             100
                                value:          updateProgress
                                visible:        _inProgress
                                // Only the upload has a meaningful percentage;
                                // the later phases are indeterminate.
                                indeterminate:  _writing || updateState === stVerifying
                            }

                            QGCLabel {
                                width:      parent.width
                                wrapMode:   Text.WordWrap
                                visible:    _writing
                                color:      qgcPal.colorOrange
                                text:       targetAir
                                                ? qsTr("Do not power off the aircraft or the controller, and do not " +
                                                       "move out of range. The air unit restarts twice and can take " +
                                                       "several minutes to come back.")
                                                : qsTr("Do not power off the controller or unplug anything. The ground " +
                                                       "unit restarts twice and can take several minutes to come back.")
                            }
                        }
                    }
                }
            }

            //-----------------------------------------------------------------
            // A plain FileDialog, NOT QGCFileDialog. QGCFileDialog switches to a
            // flat list of one preset directory on mobile with no way to browse,
            // and its urlToLocalFile() returns "" for the content:// URIs that
            // Android's system file browser produces — which silently drops the
            // selection. Here the URL is passed through untouched and resolved
            // in C++ (QFile understands content:// on Android).
            FileDialog {
                id:             fileDialog
                title:          qsTr("Select FPV firmware")
                selectExisting: true
                selectMultiple: false
                // SIYI ships these as .zip1 — that really is the extension.
                // NB: "All files" deliberately first — Android maps these to MIME
                // types and ".zip1" is unknown, so a narrower default filter can
                // hide the firmware entirely.
                nameFilters:    [qsTr("All files (*)"), qsTr("Firmware files (*.zip1 *.zip)")]

                onAccepted: controller.callSelectFirmware(fileUrl.toString())
            }

            QGCPopupDialog {
                id:             confirmDialog
                title:          qsTr("Update FPV Firmware")
                buttons:        StandardButton.Cancel | StandardButton.Ok
                destroyOnClose: false

                onAccepted: controller.callUpgradeFpvFirmware(targetAir)

                Column {
                    spacing:    _innerMargin
                    width:      ScreenTools.defaultFontPixelWidth * 50

                    QGCLabel {
                        width:      parent.width
                        wrapMode:   Text.WordWrap
                        text:       targetAir
                                        ? qsTr("This will write new firmware to the FPV AIR UNIT over the radio link " +
                                               "and restart it. The aircraft's video and telemetry link is down for " +
                                               "the duration.")
                                        : qsTr("This will write new firmware to the FPV ground unit and restart it. " +
                                               "The unit is unusable while it updates.")
                    }
                    QGCLabel {
                        width:          parent.width
                        wrapMode:       Text.WordWrap
                        visible:        targetAir
                        color:          qgcPal.colorOrange
                        font.family:    ScreenTools.demiboldFontFamily
                        text:           qsTr("This transfers over the radio link and can take several minutes. " +
                                             "Keep the aircraft powered, stationary and close by. If it fails " +
                                             "part way through, the air unit can be left unusable and cannot be " +
                                             "recovered over the air — do this on the bench, not in the field.")
                    }
                    QGCLabel {
                        width:      parent.width
                        wrapMode:   Text.WordWrap
                        text:       qsTr("Make sure the controller has plenty of battery and will not be powered " +
                                         "off. Do not fly until the update has finished.")
                    }
                    QGCLabel {
                        width:          parent.width
                        wrapMode:       Text.WordWrap
                        font.family:    ScreenTools.demiboldFontFamily
                        text:           firmwarePage.firmwareName
                    }
                }
            }
        }
    }
}
