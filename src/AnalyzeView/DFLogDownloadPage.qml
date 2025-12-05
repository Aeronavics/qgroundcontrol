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
import QtQuick.Dialogs      1.2
import QtQuick.Layouts      1.2

import QGroundControl               1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.Controllers   1.0
import QGroundControl.ScreenTools   1.0

AnalyzePage {
    id:                 dfLogDownloadPage
    pageComponent:      pageComponent
    pageDescription:    qsTr("Download and manage log files from your vehicle.")

    property real _margin:          ScreenTools.defaultFontPixelWidth
    property real _butttonWidth:    ScreenTools.defaultFontPixelWidth * 10

    QGCPalette { id: palette; colorGroupEnabled: enabled }

    Component.completed: dflogController.refresh()

    Component {
        id: pageComponent

        RowLayout {
            width:  availableWidth
            height: availableHeight

            Connections {
                target: dflogController
                onSelectionChanged: {
                    tableView.selection.clear()
                    for(var i = 0; i < dflogController.model.count; i++) {
                        var o = dflogController.model.get(i)
                        if (o && o.selected) {
                            tableView.selection.select(i, i)
                        }
                    }
                }
            }

            TableView {
                id: tableView
                Layout.fillHeight:  true
                model:              dflogController.model
                selectionMode:      SelectionMode.MultiSelection
                Layout.fillWidth:   true

                TableViewColumn {
                    title: qsTr("ID")
                    width: ScreenTools.defaultFontPixelWidth * 6
                    horizontalAlignment: Text.AlignHCenter
                    delegate : Text  {
                        color: styleData.textColor
                        horizontalAlignment: Text.AlignHCenter
                        text: {
                            var o = dflogController.model.get(styleData.row)
                            return o ? o.id : ""
                        }
                    }
                }

                TableViewColumn {
                    title: qsTr("Date")
                    width: ScreenTools.defaultFontPixelWidth * 34
                    horizontalAlignment: Text.AlignHCenter
                    delegate: Text  {
                        color: styleData.textColor
                        horizontalAlignment: Text.AlignHCenter
                        text: {
                            var o = dflogController.model.get(styleData.row)
                            if (o) {
                                var d = o.time
                                return d.toLocaleString(undefined, "short")
                            }
                            return ""
                        }
                    }
                }

                TableViewColumn {
                    title: qsTr("Size")
                    width: ScreenTools.defaultFontPixelWidth * 18
                    horizontalAlignment: Text.AlignHCenter
                    delegate : Text  {
                        color: styleData.textColor
                        horizontalAlignment: Text.AlignHCenter
                        text: {
                            var o = dflogController.model.get(styleData.row)
                            return o ? o.sizeStr : ""
                        }
                    }
                }

                TableViewColumn {
                    title: qsTr("Status")
                    width: ScreenTools.defaultFontPixelWidth * 22
                    horizontalAlignment: Text.AlignHCenter
                    delegate : Text  {
                        color: styleData.textColor
                        horizontalAlignment: Text.AlignHCenter
                        text: {
                            var o = dflogController.model.get(styleData.row)
                            return o ? o.status : ""
                        }
                    }
                }
            }
            Column {
                spacing:            _margin
                Layout.alignment:   Qt.AlignTop | Qt.AlignLeft
                QGCButton {
                    enabled:    !dflogController.requestingList && !dflogController.downloadingLogs
                    text:       qsTr("Refresh")
                    width:      _butttonWidth
                    onClicked: {
                        // if (!QGroundControl.multiVehicleManager.activeVehicle || QGroundControl.multiVehicleManager.activeVehicle.isOfflineEditingVehicle) {
                        //     mainWindow.showMessageDialog(qsTr("Log Refresh"), qsTr("You must be connected to a vehicle in order to download logs."))
                        // } else {
                            dflogController.refresh()
                        // }
                    }
                }
                QGCButton {
                    enabled:    !dflogController.requestingList && tableView.selection.count > 0
                    text:       qsTr("Download")
                    width:      _butttonWidth
                    onClicked: {
                        //-- Clear selection
                        for(var i = 0; i < dflogController.model.count; i++) {
                            var o = dflogController.model.get(i)
                            if (o) o.selected = false
                        }
                        //-- Flag selected log files
                        tableView.selection.forEach(function(rowIndex){
                            var o = dflogController.model.get(rowIndex)
                            if (o) o.selected = true
                        })
                        mainWindow.showMessageDialog(
                            qsTr("Download Selected Log Files"),
                            qsTr("All selected log files will downloaded. Are you sure?"),
                            StandardButton.Yes | StandardButton.No,
                            function() { dflogController.queueForDownload() }
                        )
                    }
                }
                QGCButton {
                    enabled:    !dflogController.requestingList && !dflogController.downloadingLogs && tableView.selection.count > 0
                    text:       qsTr("Erase")
                    width:      _butttonWidth
                    onClicked: {
                        //-- Clear selection
                        for(var i = 0; i < dflogController.model.count; i++) {
                            var o = dflogController.model.get(i)
                            if (o) o.selected = false
                        }
                        //-- Flag selected log files
                        tableView.selection.forEach(function(rowIndex){
                            var o = dflogController.model.get(rowIndex)
                            if (o) o.selected = true
                        })
                        mainWindow.showMessageDialog(
                            qsTr("Delete Selected Log Files"),
                            qsTr("All selected log files will be erased permanently. Are you sure?"),
                            StandardButton.Yes | StandardButton.No,
                            function() { dflogController.erase() }
                        )
                    }
                }
                QGCButton {
                    enabled:    !dflogController.requestingList && !dflogController.downloadingLogs && dflogController.model.count > 0
                    text:       qsTr("Erase All")
                    width:      _butttonWidth
                    onClicked:  mainWindow.showMessageDialog(qsTr("Delete All Log Files"),
                                                             qsTr("All log files will be erased permanently. Is this really what you want?"),
                                                             StandardButton.Yes | StandardButton.No,
                                                             function() { dflogController.eraseAll() })
                }
            }
        }
    }
}
