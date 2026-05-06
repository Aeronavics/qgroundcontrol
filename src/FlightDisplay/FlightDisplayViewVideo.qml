/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


import QtQuick                          2.11
import QtQuick.Controls                 2.4

import QGroundControl                   1.0
import QGroundControl.FlightDisplay     1.0
import QGroundControl.FlightMap         1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.Controls          1.0
import QGroundControl.Palette           1.0
import QGroundControl.Vehicle           1.0
import QGroundControl.Controllers       1.0

Item {
    id:     root
    clip:   true

    property bool useSmallFont: true

    property double _ar:                QGroundControl.videoManager.aspectRatio
    // property bool   _showGrid:          QGroundControl.settingsManager.videoSettings.gridLines.rawValue > 0
    property var    _dynamicCameras:    globals.activeVehicle ? globals.activeVehicle.cameraManager : null
    property bool   _connected:         globals.activeVehicle ? !globals.activeVehicle.communicationLost : false
    property int    _curCameraIndex:    _dynamicCameras ? _dynamicCameras.currentCamera : 0
    property bool   _isCamera:          _dynamicCameras ? _dynamicCameras.cameras.count > 0 : false
    property var    _camera:            _isCamera ? _dynamicCameras.cameras.get(_curCameraIndex) : null
    property int    _fitMode:           QGroundControl.settingsManager.videoSettings.videoFit.rawValue

    function getWidth() {
        return videoBackground.getWidth()
    }
    function getHeight() {
        return videoBackground.getHeight()
    }

    Image {
        id:             noVideo
        anchors.fill:   parent
        source:         "/res/NoVideoBackground.jpg"
        fillMode:       Image.PreserveAspectCrop
        visible:        (QGroundControl.videoManager.primaryStream && !(QGroundControl.videoManager.decoding)) || (QGroundControl.videoManager.secondaryStream && !(QGroundControl.videoManager.secondaryDecoding)) || (QGroundControl.videoManager.tertiaryStream && !(QGroundControl.videoManager.tertiaryDecoding))

        Rectangle {
            anchors.centerIn:   parent
            width:              noVideoLabel.contentWidth + ScreenTools.defaultFontPixelHeight
            height:             noVideoLabel.contentHeight + ScreenTools.defaultFontPixelHeight
            radius:             ScreenTools.defaultFontPixelWidth / 2
            color:              "black"
            opacity:            0.5
        }

        QGCLabel {
            id:                 noVideoLabel
            text:               QGroundControl.settingsManager.videoSettings.streamEnabled.rawValue ? qsTr("WAITING FOR VIDEO") : qsTr("VIDEO DISABLED")
            font.family:        ScreenTools.demiboldFontFamily
            color:              "white"
            font.pointSize:     useSmallFont ? ScreenTools.smallFontPointSize : ScreenTools.largeFontPointSize
            anchors.centerIn:   parent
        }
    }

    Rectangle {
        id:             videoBackground
        anchors.fill:   parent
        color:          "black"
        visible:        QGroundControl.videoManager.decoding
        function getWidth() {
            //-- Fit Width or Stretch
            if(_fitMode === 0 || _fitMode === 2) {
                return parent.width
            }
            //-- Fit Height
            return _ar != 0.0 ? parent.height * _ar : parent.width
        }
        function getHeight() {
            //-- Fit Height or Stretch
            if(_fitMode === 1 || _fitMode === 2) {
                return parent.height
            }
            //-- Fit Width
            return _ar != 0.0 ? parent.width * (1 / _ar) : parent.height
        }
        Component {
            id: videoBackgroundComponent
            QGCVideoBackground {
                id:             videoContent
                objectName:     "videoContent"

                Connections {
                    target: QGroundControl.videoManager
                    function onImageFileChanged() {
                        videoContent.grabToImage(function(result) {
                            if (QGroundControl.videoManager.primaryStream) {
                                if (!result.saveToFile(QGroundControl.videoManager.imageFile)) {
                                    console.error('Error capturing video frame');
                                }
                                QGroundControl.videoManager.writeEXIFDataToFile(QGroundControl.videoManager.imageFile);
                            }
                        });
                    }
                }
            }
        }
        Loader {
            // GStreamer is causing crashes on Lenovo laptop OpenGL Intel drivers. In order to workaround this
            // we don't load a QGCVideoBackground object when video is disabled. This prevents any video rendering
            // code from running. Setting QGCVideoBackground.receiver = null does not work to prevent any
            // video OpenGL from being generated. Hence the Loader to completely remove it.
            height:             parent.getHeight()
            width:              parent.getWidth()
            anchors.centerIn:   parent
            visible:            QGroundControl.videoManager.decoding
            sourceComponent:    videoBackgroundComponent
        }

        Component {
            id: secondVideoBackgroundComponent
            QGCVideoBackground {
                id:             secondVideoContent
                objectName:     "secondVideoContent"
                visible:        false

                Connections {
                    target: QGroundControl.videoManager
                    function onSecondaryImageFileChanged() {
                        secondVideoContent.grabToImage(function(result) {
                            if (QGroundControl.videoManager.secondaryStream) {
                                if (!result.saveToFile(QGroundControl.videoManager.secondaryImageFile)) {
                                    console.error('Error capturing video frame');
                                }
                                QGroundControl.videoManager.writeEXIFDataToFile(QGroundControl.videoManager.secondaryImageFile);
                            }
                        });
                    }
                }
            }
        }
        Loader {
            // GStreamer is causing crashes on Lenovo laptop OpenGL Intel drivers. In order to workaround this
            // we don't load a QGCVideoBackground object when video is disabled. This prevents any video rendering
            // code from running. Setting QGCVideoBackground.receiver = null does not work to prevent any
            // video OpenGL from being generated. Hence the Loader to completely remove it.
            height:             parent.getHeight()
            width:              parent.getWidth()
            anchors.centerIn:   parent
            visible:            QGroundControl.videoManager.secondaryDecoding
            sourceComponent:    secondVideoBackgroundComponent
        }

        Component {
            id: thirdVideoBackgroundComponent
            QGCVideoBackground {
                id:             thirdVideoContent
                objectName:     "thirdVideoContent"
                visible:        false

                Connections {
                    target: QGroundControl.videoManager
                    function onTertiaryImageFileChanged() {
                        thirdVideoContent.grabToImage(function(result) {
                            if (QGroundControl.videoManager.tertiaryStream) {
                                if (!result.saveToFile(QGroundControl.videoManager.tertiaryImageFile)) {
                                    console.error('Error capturing video frame');
                                }
                                QGroundControl.videoManager.writeEXIFDataToFile(QGroundControl.videoManager.tertiaryImageFile);
                            }
                        });
                    }
                }
            }
        }
        Loader {
            // GStreamer is causing crashes on Lenovo laptop OpenGL Intel drivers. In order to workaround this
            // we don't load a QGCVideoBackground object when video is disabled. This prevents any video rendering
            // code from running. Setting QGCVideoBackground.receiver = null does not work to prevent any
            // video OpenGL from being generated. Hence the Loader to completely remove it.
            height:             parent.getHeight()
            width:              parent.getWidth()
            anchors.centerIn:   parent
            visible:            QGroundControl.videoManager.tertiaryDecoding
            sourceComponent:    thirdVideoBackgroundComponent

            property bool videoDisabled: QGroundControl.settingsManager.videoSettings.videoSource.rawValue === QGroundControl.settingsManager.videoSettings.disabledVideoSource
        }
    }
}
