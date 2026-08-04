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
import QtQuick.Dialogs  1.2
import QtQuick.Layouts  1.2

import QGroundControl               1.0
import QGroundControl.Controllers   1.0
import QGroundControl.Controls      1.0
import QGroundControl.Palette       1.0
import QGroundControl.ScreenTools   1.0

AnalyzePage {
    id:                 droneOpsPage
    pageComponent:      pageComponent
    pageDescription:    qsTr("Sign in to DroneOps and push flight logs to an open job.")

    property real _margin:      ScreenTools.defaultFontPixelWidth
    property real _fieldWidth:  ScreenTools.defaultFontPixelWidth * 32
    property real _buttonWidth: ScreenTools.defaultFontPixelWidth * 16

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    Component.onCompleted: droneOpsController.refreshLogFiles()

    Component {
        id: pageComponent

        Item {
            width:  availableWidth
            height: availableHeight

            QGCFlickable {
                anchors.fill:       parent
                contentHeight:      mainColumn.height
                clip:               true

                Column {
                    id:         mainColumn
                    width:      parent.width
                    spacing:    _margin * 2

                    //-------------------------------------------------------------------
                    //-- Sign-in panel
                    Rectangle {
                        width:      Math.min(_fieldWidth + _margin * 4, parent.width)
                        height:     loginColumn.height + _margin * 4
                        color:      qgcPal.windowShade
                        radius:     _margin
                        visible:    !droneOpsController.loggedIn

                        Column {
                            id:                 loginColumn
                            anchors.centerIn:   parent
                            width:              parent.width - _margin * 4
                            spacing:            _margin * 1.5

                            QGCLabel {
                                text:           qsTr("Sign in to DroneOps")
                                font.pointSize: ScreenTools.mediumFontPointSize
                            }

                            QGCLabel { text: qsTr("Server address") }
                            QGCTextField {
                                id:                 serverField
                                width:              parent.width
                                text:               droneOpsController.baseUrl
                                placeholderText:    qsTr("https://droneops.example.com")
                                onEditingFinished:  droneOpsController.baseUrl = text
                            }

                            QGCLabel { text: qsTr("Email") }
                            QGCTextField {
                                id:                 emailField
                                width:              parent.width
                                text:               droneOpsController.email
                                placeholderText:    qsTr("pilot@org.com")
                                onEditingFinished:  droneOpsController.email = text
                            }

                            QGCLabel { text: qsTr("Password") }
                            QGCTextField {
                                id:             passwordField
                                width:          parent.width
                                echoMode:       TextInput.Password
                                onAccepted:     droneOpsController.login(passwordField.text, mfaField.text)
                            }

                            QGCLabel {
                                text:       qsTr("Authenticator code")
                                visible:    droneOpsController.mfaRequired
                            }
                            QGCTextField {
                                id:                 mfaField
                                width:              parent.width
                                visible:            droneOpsController.mfaRequired
                                placeholderText:    qsTr("6-digit code")
                                inputMethodHints:   Qt.ImhDigitsOnly
                                maximumLength:      8
                                onAccepted:         droneOpsController.login(passwordField.text, mfaField.text)
                            }

                            QGCLabel {
                                width:      parent.width
                                visible:    droneOpsController.mfaOtpauthUri !== ""
                                wrapMode:   Text.WrapAnywhere
                                font.pointSize: ScreenTools.smallFontPointSize
                                text:       qsTr("Enrolment URI: ") + droneOpsController.mfaOtpauthUri
                            }

                            QGCButton {
                                text:       droneOpsController.mfaRequired ? qsTr("Verify") : qsTr("Sign In")
                                width:      _buttonWidth
                                enabled:    !droneOpsController.busy
                                primary:    true
                                onClicked:  droneOpsController.login(passwordField.text, mfaField.text)
                            }
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Signed-in header
                    RowLayout {
                        width:      parent.width
                        spacing:    _margin
                        visible:    droneOpsController.loggedIn

                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               qsTr("Signed in as %1").arg(droneOpsController.email)
                        }
                        QGCButton {
                            text:       qsTr("Sign Out")
                            onClicked:  droneOpsController.logout()
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Job picker
                    Column {
                        width:      parent.width
                        spacing:    _margin
                        visible:    droneOpsController.loggedIn

                        QGCLabel {
                            text:           qsTr("Open job to sync against")
                            font.pointSize: ScreenTools.mediumFontPointSize
                        }

                        RowLayout {
                            width:      parent.width
                            spacing:    _margin

                            QGCComboBox {
                                id:                 jobCombo
                                Layout.fillWidth:   true
                                enabled:            droneOpsController.jobs.length > 0
                                model:              _jobLabels()

                                property var jobs:  droneOpsController.jobs

                                function _jobLabels() {
                                    var labels = []
                                    var list = droneOpsController.jobs
                                    for (var i = 0; i < list.length; i++) {
                                        var j = list[i]
                                        var label = j.job_number ? j.job_number : qsTr("(no number)")
                                        if (j.name)         label += " — " + j.name
                                        if (j.client_name)  label += " (" + j.client_name + ")"
                                        labels.push(label)
                                    }
                                    if (labels.length === 0) {
                                        labels.push(qsTr("No open jobs"))
                                    }
                                    return labels
                                }

                                onJobsChanged: {
                                    model = _jobLabels()
                                    currentIndex = jobs.length > 0 ? 0 : -1
                                }
                            }

                            QGCButton {
                                text:       qsTr("Refresh")
                                enabled:    !droneOpsController.busy
                                onClicked:  droneOpsController.refreshJobs()
                            }
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Local log list
                    Column {
                        width:      parent.width
                        spacing:    _margin
                        visible:    droneOpsController.loggedIn

                        RowLayout {
                            width:      parent.width
                            spacing:    _margin

                            QGCLabel {
                                Layout.fillWidth:   true
                                text:               qsTr("Flight log to upload")
                                font.pointSize:     ScreenTools.mediumFontPointSize
                            }
                            QGCButton {
                                text:       qsTr("Browse…")
                                onClicked:  fileDialog.open()
                            }
                            QGCButton {
                                text:       qsTr("Rescan")
                                onClicked:  droneOpsController.refreshLogFiles()
                            }
                        }

                        Rectangle {
                            width:      parent.width
                            height:     ScreenTools.defaultFontPixelHeight * 12
                            color:      qgcPal.windowShade
                            radius:     _margin / 2

                            ListView {
                                id:             logList
                                anchors.fill:   parent
                                anchors.margins: _margin / 2
                                clip:           true
                                model:          droneOpsController.logFiles

                                property string selectedPath: ""

                                delegate: Rectangle {
                                    width:      logList.width
                                    height:     ScreenTools.defaultFontPixelHeight * 2
                                    color:      modelData.path === logList.selectedPath ? qgcPal.buttonHighlight : "transparent"

                                    RowLayout {
                                        anchors.fill:           parent
                                        anchors.leftMargin:     _margin
                                        anchors.rightMargin:    _margin
                                        spacing:                _margin

                                        QGCLabel {
                                            Layout.fillWidth:   true
                                            elide:              Text.ElideMiddle
                                            text:               modelData.name
                                            color:              modelData.path === logList.selectedPath ? qgcPal.buttonHighlightText : qgcPal.text
                                        }
                                        QGCLabel {
                                            text:   modelData.modifiedStr
                                            color:  modelData.path === logList.selectedPath ? qgcPal.buttonHighlightText : qgcPal.text
                                        }
                                        QGCLabel {
                                            text:   modelData.sizeStr
                                            color:  modelData.path === logList.selectedPath ? qgcPal.buttonHighlightText : qgcPal.text
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill:   parent
                                        onClicked:      logList.selectedPath = modelData.path
                                    }
                                }
                            }

                            QGCLabel {
                                anchors.centerIn:   parent
                                visible:            droneOpsController.logFiles.length === 0
                                text:               qsTr("No logs found in the Logs folder.\nUse Browse… to pick a file.")
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        QGCLabel {
                            width:      parent.width
                            visible:    logList.selectedPath !== ""
                            elide:      Text.ElideMiddle
                            text:       qsTr("Selected: ") + logList.selectedPath
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Queue upload + active progress
                    Column {
                        width:      parent.width
                        spacing:    _margin
                        visible:    droneOpsController.loggedIn

                        RowLayout {
                            width:      parent.width
                            spacing:    _margin

                            QGCButton {
                                text:       qsTr("Queue upload")
                                primary:    true
                                enabled:    logList.selectedPath !== "" &&
                                            jobCombo.currentIndex >= 0 &&
                                            droneOpsController.jobs.length > 0
                                onClicked: {
                                    var job = droneOpsController.jobs[jobCombo.currentIndex]
                                    droneOpsController.queueUpload(job.id, jobCombo.currentText, logList.selectedPath)
                                }
                            }
                            QGCButton {
                                text:       qsTr("Cancel current")
                                visible:    droneOpsController.uploading
                                onClicked:  droneOpsController.cancelUpload()
                            }
                        }

                        ProgressBar {
                            width:          parent.width
                            minimumValue:   0
                            maximumValue:   1
                            value:          droneOpsController.uploadProgress
                            visible:        droneOpsController.uploading
                        }

                        QGCLabel {
                            visible:    droneOpsController.parseStatus !== ""
                            text:       qsTr("Server status: ") + droneOpsController.parseStatus
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Upload queue
                    Column {
                        width:      parent.width
                        spacing:    _margin
                        visible:    droneOpsController.loggedIn || droneOpsController.uploadQueue.length > 0

                        RowLayout {
                            width:      parent.width
                            spacing:    _margin

                            QGCLabel {
                                Layout.fillWidth:   true
                                font.pointSize:     ScreenTools.mediumFontPointSize
                                text:               droneOpsController.pendingCount > 0
                                                        ? qsTr("Upload queue — %1 pending").arg(droneOpsController.pendingCount)
                                                        : qsTr("Upload queue")
                            }
                            QGCButton {
                                text:       qsTr("Process")
                                enabled:    droneOpsController.loggedIn && droneOpsController.pendingCount > 0 && !droneOpsController.uploading
                                onClicked:  droneOpsController.processQueue()
                            }
                            QGCButton {
                                text:       qsTr("Clear done")
                                onClicked:  droneOpsController.clearCompleted()
                            }
                        }

                        Rectangle {
                            width:      parent.width
                            height:     ScreenTools.defaultFontPixelHeight * 12
                            color:      qgcPal.windowShade
                            radius:     _margin / 2
                            visible:    droneOpsController.uploadQueue.length > 0

                            ListView {
                                id:             queueList
                                anchors.fill:   parent
                                anchors.margins: _margin / 2
                                clip:           true
                                spacing:        _margin / 3
                                model:          droneOpsController.uploadQueue

                                function _statusColor(status) {
                                    switch (status) {
                                    case "done":
                                    case "duplicate":   return qgcPal.colorGreen
                                    case "error":       return qgcPal.warningText
                                    case "uploading":
                                    case "processing":  return qgcPal.colorOrange
                                    default:            return qgcPal.text
                                    }
                                }

                                function _statusLabel(item) {
                                    if (item.status === "error" && item.error)
                                        return qsTr("error: ") + item.error
                                    if (item.status === "uploaded")
                                        return qsTr("uploaded (unconfirmed)")
                                    return item.status
                                }

                                delegate: Rectangle {
                                    width:      queueList.width
                                    height:     ScreenTools.defaultFontPixelHeight * 2.4
                                    color:      "transparent"

                                    RowLayout {
                                        anchors.fill:           parent
                                        anchors.leftMargin:     _margin
                                        anchors.rightMargin:    _margin
                                        spacing:                _margin

                                        Column {
                                            Layout.fillWidth:   true
                                            QGCLabel {
                                                width:      parent.width
                                                elide:      Text.ElideMiddle
                                                text:       modelData.fileName + "  →  " + modelData.jobLabel
                                            }
                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          queueList._statusColor(modelData.status)
                                                text:           queueList._statusLabel(modelData) + "  ·  " + modelData.sizeStr
                                            }
                                        }
                                        QGCButton {
                                            text:       qsTr("Retry")
                                            visible:    modelData.status === "error" || modelData.status === "uploaded"
                                            onClicked:  droneOpsController.retryQueued(index)
                                        }
                                        QGCButton {
                                            text:       qsTr("Remove")
                                            visible:    modelData.status !== "uploading" && modelData.status !== "processing"
                                            onClicked:  droneOpsController.removeQueued(index)
                                        }
                                    }
                                }
                            }
                        }

                        QGCLabel {
                            visible:    droneOpsController.uploadQueue.length === 0
                            text:       qsTr("Queue is empty.")
                            color:      qgcPal.colorGrey
                        }
                    }

                    //-------------------------------------------------------------------
                    //-- Status line
                    QGCLabel {
                        width:      parent.width
                        wrapMode:   Text.WordWrap
                        text:       droneOpsController.statusText
                        color:      droneOpsController.errorStatus ? qgcPal.warningText : qgcPal.text
                    }
                }
            }

            FileDialog {
                id:             fileDialog
                title:          qsTr("Select a flight log")
                folder:         shortcuts.home
                selectMultiple: false
                nameFilters:    [ qsTr("Flight logs (*.bin *.BIN *.ulg *.log *.tlog)"), qsTr("All files (*)") ]
                onAccepted: {
                    var path = fileDialog.fileUrl.toString()
                    // Strip the file:// scheme for the local path the controller expects.
                    path = path.replace(/^(file:\/{2,3})/, "")
                    if (Qt.platform.os !== "windows" && !path.startsWith("/")) {
                        path = "/" + path
                    }
                    path = decodeURIComponent(path)
                    logList.selectedPath = path
                }
            }
        }
    }
}
