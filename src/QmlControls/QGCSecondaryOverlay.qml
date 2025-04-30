/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick                  2.12
import QtQuick.Controls         2.4
import QtQuick.Dialogs          1.3
import QtQuick.Layouts          1.12

import QtLocation               5.3
import QtPositioning            5.3
import QtQuick.Window           2.2
import QtQml.Models             2.1

import QGroundControl               1.0
import QGroundControl.Controllers   1.0
import QGroundControl.Controls      1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FlightDisplay 1.0
import QGroundControl.FlightMap     1.0
import QGroundControl.Palette       1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Vehicle       1.0

Item {
    id:         _overlayRoot
    width:      _overlaySize
    height:     _overlaySize * (9/16)
    visible:    show

    property bool   show:                   true

    property bool   _isOverlayExpanded:        true
    property real   _overlaySize:       parent.width * 0.2
    property real   _maxSize:           0.75                // Percentage of parent control size
    property real   _minSize:           0.10

    function _setOverlayExpanded(isOverlayExpanded) {
        _isOverlayExpanded = isOverlayExpanded
    }
    Rectangle {
        anchors.fill:   parent
        visible:        _isOverlayExpanded

        Image {
            id:             noVideo
            anchors.fill:   parent
            source:         "/res/NoVideoBackground.jpg"
            fillMode:       Image.PreserveAspectCrop
            visible:        !(QGroundControl.videoManager.decoding)

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
                font.pointSize:     ScreenTools.smallFontPointSize
                anchors.centerIn:   parent
            }
        }

        Component {
            id: smallVideoBackgroundComponent
            QGCVideoBackground {
                id:             smallVideoContent
                objectName:     "smallVideoContent"
                visible:        false
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
            sourceComponent:    smallVideoBackgroundComponent
        }

        Component {
            id: secondSmallVideoBackgroundComponent
            QGCVideoBackground {
                id:             secondSmallVideoContent
                objectName:     "secondSmallVideoContent"
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
            sourceComponent:    secondSmallVideoBackgroundComponent
        }

        MouseArea {
            id:             overlayMouseArea
            anchors.fill:   parent
            enabled:        _isOverlayExpanded
            hoverEnabled:   true
        }

        // MouseArea to drag in order to resize the PiP area
        MouseArea {
            id:             overlayResize
            anchors.top:    parent.top
            anchors.left:   parent.left
            height:         ScreenTools.minTouchPixels
            width:          height

            property real initialX:     0
            property real initialWidth: 0

            // When we push the mouse button down, we un-anchor the mouse area to prevent a resizing loop
            onPressed: {
                overlayResize.anchors.top = undefined // Top doesn't seem to 'detach'
                overlayResize.anchors.left = undefined // This one works right, which is what we really need
                overlayResize.initialX = mouse.x
                overlayResize.initialWidth = _overlayRoot.width
            }

            // When we let go of the mouse button, we re-anchor the mouse area in the correct position
            onReleased: {
                overlayResize.anchors.top = _overlayRoot.top
                overlayResize.anchors.left = _overlayRoot.left
            }

            // Drag
            onPositionChanged: {
                if (overlayResize.pressed) {
                    var parentWidth = _overlayRoot.parent.width
                    var newWidth = overlayResize.initialWidth + overlayResize.initialX - mouse.x
                    if (newWidth < parentWidth * _maxSize && newWidth > parentWidth * _minSize) {
                        _overlaySize = newWidth
                    }
                }
            }
        }

        // Resize icon
        Image {
            source:         "/qmlimages/pipResize.svg"
            fillMode:       Image.PreserveAspectFit
            mipmap: true
            mirror: true
            anchors.left:  parent.left
            anchors.top:    parent.top
            visible:        ScreenTools.isMobile || overlayMouseArea.containsMouse
            height:         ScreenTools.defaultFontPixelHeight * 2.5
            width:          ScreenTools.defaultFontPixelHeight * 2.5
            sourceSize.height:  height
        }

        Image {
            id:             hideOverlay
            source:         "/qmlimages/pipHide.svg"
            mipmap:         true
            mirror:         true
            fillMode:       Image.PreserveAspectFit
            anchors.right:   parent.right
            anchors.bottom: parent.bottom
            visible:        ScreenTools.isMobile || overlayMouseArea.containsMouse
            height:         ScreenTools.defaultFontPixelHeight * 2.5
            width:          ScreenTools.defaultFontPixelHeight * 2.5
            sourceSize.height:  height
            MouseArea {
                anchors.fill:   parent
                onClicked:      _overlayRoot._setOverlayExpanded(false)
            }
        }
    }

    Rectangle {
        id:                     showOverlay
        anchors.right :         parent.right
        anchors.bottom:         parent.bottom
        height:                 ScreenTools.defaultFontPixelHeight * 2
        width:                  ScreenTools.defaultFontPixelHeight * 2
        radius:                 ScreenTools.defaultFontPixelHeight / 3
        visible:                !_isOverlayExpanded
        color:                  Qt.rgba(0,0,0,0.75)
        Image {
            width:              parent.width  * 0.75
            height:             parent.height * 0.75
            sourceSize.height:  height
            source:             "/res/buttonRight.svg"
            mipmap:             true
            mirror:             true
            fillMode:           Image.PreserveAspectFit
            anchors.verticalCenter:     parent.verticalCenter
            anchors.horizontalCenter:   parent.horizontalCenter
        }
        MouseArea {
            anchors.fill:   parent
            onClicked:      _overlayRoot._setOverlayExpanded(true)
        }
    }
}
