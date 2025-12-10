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

import QtLocation                   5.3
import QtPositioning                5.3

import QGroundControl               1.0
import QGroundControl.Controllers   1.0
import QGroundControl.Controls      1.0
import QGroundControl.FlightDisplay 1.0
import QGroundControl.FlightMap     1.0
import QGroundControl.Palette       1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Vehicle       1.0

AnalyzePage {
    id:                 sprayLogDownloadPage
    pageComponent:      pageComponent
    pageDescription:    qsTr("Download and manage spray logs from your vehicle.")

    property real _margin:          ScreenTools.defaultFontPixelWidth
    property real _butttonWidth:    ScreenTools.defaultFontPixelWidth * 10

    QGCPalette { id: palette; colorGroupEnabled: enabled }

    // Component.completed: spraylogController.refresh()

    Component {
        id: pageComponent

        Item {
            RowLayout {
                id: sprayLogLayout
                width:  availableWidth
                height: availableHeight

                Connections {
                    target: spraylogController
                    onSelectionChanged: {
                        tableView.selection.clear()
                        for(var i = 0; i < spraylogController.model.count; i++) {
                            var o = spraylogController.model.get(i)
                            if (o && o.selected) {
                                tableView.selection.select(i, i)
                            }
                        }
                    }
                }

                TableView {
                    id: tableView
                    Layout.fillHeight:  true
                    model:              spraylogController.model
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
                                var o = spraylogController.model.get(styleData.row)
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
                                var o = spraylogController.model.get(styleData.row)
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
                                var o = spraylogController.model.get(styleData.row)
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
                                var o = spraylogController.model.get(styleData.row)
                                return o ? o.status : ""
                            }
                        }
                    }
                }

                Column {
                    spacing:            _margin
                    Layout.alignment:   Qt.AlignTop | Qt.AlignLeft
                    QGCButton {
                        enabled:    !spraylogController.requestingList && !spraylogController.downloadingLogs
                        text:       qsTr("Refresh")
                        width:      _butttonWidth
                        onClicked: {
                            // if (!QGroundControl.multiVehicleManager.activeVehicle || QGroundControl.multiVehicleManager.activeVehicle.isOfflineEditingVehicle) {
                            //     mainWindow.showMessageDialog(qsTr("Log Refresh"), qsTr("You must be connected to a vehicle in order to download logs."))
                            // } else {
                                spraylogController.refresh()
                            // }
                        }
                    }
                    QGCButton {
                        enabled:    !spraylogController.requestingList && tableView.selection.count > 0
                        text:       qsTr("Download")
                        width:      _butttonWidth
                        onClicked: {
                            //-- Clear selection
                            for(var i = 0; i < spraylogController.model.count; i++) {
                                var o = spraylogController.model.get(i)
                                if (o) o.selected = false
                            }
                            //-- Flag selected log files
                            tableView.selection.forEach(function(rowIndex){
                                var o = spraylogController.model.get(rowIndex)
                                if (o) o.selected = true
                            })
                            mainWindow.showMessageDialog(
                                qsTr("Download Selected Log Files"),
                                qsTr("All selected log files will downloaded. Are you sure?"),
                                StandardButton.Yes | StandardButton.No,
                                function() { spraylogController.queueForDownload() }
                            )
                        }
                    }
                    QGCButton {
                        enabled:    !spraylogController.requestingList && !spraylogController.downloadingLogs && tableView.selection.count > 0
                        text:       qsTr("Erase")
                        width:      _butttonWidth
                        onClicked: {
                            //-- Clear selection
                            for(var i = 0; i < spraylogController.model.count; i++) {
                                var o = spraylogController.model.get(i)
                                if (o) o.selected = false
                            }
                            //-- Flag selected log files
                            tableView.selection.forEach(function(rowIndex){
                                var o = spraylogController.model.get(rowIndex)
                                if (o) o.selected = true
                            })
                            mainWindow.showMessageDialog(
                                qsTr("Delete Selected Log Files"),
                                qsTr("All selected log files will be erased permanently. Are you sure?"),
                                StandardButton.Yes | StandardButton.No,
                                function() { spraylogController.erase() }
                            )
                        }
                    }
                    QGCButton {
                        enabled:    !spraylogController.requestingList && !spraylogController.downloadingLogs && spraylogController.model.count > 0
                        text:       qsTr("Erase All")
                        width:      _butttonWidth
                        onClicked:  mainWindow.showMessageDialog(qsTr("Delete All Log Files"),
                                                                 qsTr("All log files will be erased permanently. Is this really what you want?"),
                                                                 StandardButton.Yes | StandardButton.No,
                                                                 function() { spraylogController.eraseAll() })
                    }
                    QGCButton {
                        enabled:    tableView.selection.count > 0
                        text:       qsTr("View")
                        width:      _butttonWidth
                        onClicked: {
                            for(var i = 0; i < spraylogController.model.count; i++) {
                                var o = spraylogController.model.get(i)
                                if (o) o.selected = false
                            }
                            var all_downloaded = true;
                            tableView.selection.forEach(function(rowIndex){
                                var o = spraylogController.model.get(rowIndex)
                                if (o) o.selected = true
                                if (o.status !== "Downloaded") {
                                    all_downloaded = false;
                                }
                            })
                            if (!all_downloaded) {
                                mainWindow.showMessageDialog(
                                    qsTr("View Error"),
                                    qsTr("Logs must be downloaded to before viewing"),
                                    StandardButton.Yes,
                                    function() { }
                                )
                            }
                            else {
                                spraylogController.addLogsToSprayMap(_flightMap.width, _flightMap.height)
                                sprayMapView.visible = true
                                sprayLogLayout.visible = false
                            }
                        }
                    }
                }
            }
            RowLayout {
                id: sprayMapView
                width:  availableWidth
                height: availableHeight
                visible: false

                FlightMap {
                    Layout.fillHeight:  true
                    Layout.fillWidth:   true

                    id:                         _flightMap
                    allowGCSLocationCenter:     true
                    planView:                   false
                    zoomLevel:                  spraylogController.mapZoomLevel
                    center:                     spraylogController.mapCenter

                    // Spray trigger points
                    MapItemView {
                        model: spraylogController.sprayTrailPoints

                        delegate: SprayingIndicator {
                            coordinate:     object.coordinate
                            z:              QGroundControl.zOrderTopMost
                        }
                    }
                    MapItemView {
                        model: spraylogController.sprayStartPoints

                        delegate: SprayTriggerIndicator {
                            coordinate:     object.coordinate
                            z:              QGroundControl.zOrderTopMost
                        }
                    }

                }
                Column {
                    spacing:            _margin
                    Layout.alignment:   Qt.AlignTop | Qt.AlignLeft

                    QGCButton {
                        enabled: QGroundControl.multiVehicleManager.activeVehicleAvailable
                        text:       qsTr("Add to map")
                        width:      _butttonWidth
                        onClicked: {
                            mainWindow.showMessageDialog(
                                qsTr("Add to Flight Map"),
                                qsTr("Do you want to add these marker to the flight map?"),
                                StandardButton.Yes | StandardButton.No,
                                function() { spraylogController.addLogsToFlightMap() }
                            )
                        }
                    }
                    QGCButton {
                        text:       qsTr("Download")
                        width:      _butttonWidth
                        onClicked: {
                            _flightMap.grabToImage(function(result) {
                                var filename = new Date().toLocaleString()
                                if (result.saveToFile("/storage/emulated/0/QGroundControl Daily/Logs/spray maps/" + filename + ".png"))
                                {
                                    mainWindow.showMessageDialog(
                                        qsTr("Success"),
                                        qsTr("Map saved to Logs/spray maps/" + filename + ".png"),
                                        StandardButton.Yes,
                                        function() {}
                                    )
                                }
                                else
                                {
                                    mainWindow.showMessageDialog(
                                        qsTr("Failure"),
                                        qsTr("Failed to save map"),
                                        StandardButton.Yes,
                                        function() {}
                                    )
                                }
                            })
                        }
                    }
                    QGCButton {
                        text:       qsTr("Close")
                        width:      _butttonWidth
                        onClicked: {
                            sprayMapView.visible = false
                            sprayLogLayout.visible = true
                        }
                    }
                }
            }
        }
    }
}
