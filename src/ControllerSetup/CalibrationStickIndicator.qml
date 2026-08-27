import QtQuick              2.15
import QtQuick.Shapes       1.12

import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0

// Circular crosshair + dot indicator for one physical stick's live X/Y
// position, matching UniGCS's own RC Calibration screen (two of these side
// by side: left stick, right stick).
Column {
    id:      root
    spacing: ScreenTools.defaultFontPixelHeight / 2

    property real xValue: 0    // raw analog value, horizontal axis
    property real yValue: 0    // raw analog value, vertical axis
    property real rawMax: 150  // symmetric range: -rawMax..rawMax on each axis
    property real diameter: ScreenTools.defaultFontPixelHeight * 14

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Item {
        id:                     face
        width:                  root.diameter
        height:                 root.diameter
        anchors.horizontalCenter: parent.horizontalCenter

        // Outer and inner reference circles (stroke only, no fill).
        Rectangle {
            anchors.centerIn:   parent
            width:              parent.width
            height:             parent.height
            radius:             width / 2
            color:              "transparent"
            border.color:       qgcPal.text
            border.width:       1
            opacity:            0.6
        }
        Rectangle {
            anchors.centerIn:   parent
            width:              parent.width * 0.58
            height:             parent.height * 0.58
            radius:             width / 2
            color:              "transparent"
            border.color:       qgcPal.text
            border.width:       1
            opacity:            0.6
        }

        // Crosshair tick marks at N/E/S/W: an Item the same size as the face,
        // rotated 0/90/180/270 about its centre, holding one tick fixed at
        // its own top edge - simpler and less error-prone than rotating each
        // tick individually about the face's centre with matching offsets.
        Repeater {
            model: 4

            Item {
                anchors.fill: face
                rotation:     index * 90

                Rectangle {
                    width:              2
                    height:             ScreenTools.defaultFontPixelHeight / 2
                    color:              qgcPal.text
                    opacity:            0.6
                    anchors.horizontalCenter: parent.horizontalCenter
                    y:                  -height / 2
                }
            }
        }

        // Axis lines through the centre, marking x=0/y=0 (not just the N/E/
        // S/W edge ticks above).
        Rectangle {
            anchors.verticalCenter:   parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width:      parent.width
            height:     1
            color:      qgcPal.text
            opacity:    0.6
        }
        Rectangle {
            anchors.verticalCenter:   parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width:      1
            height:     parent.height
            color:      qgcPal.text
            opacity:    0.6
        }

        // Live position dot, clamped to stay within the outer circle.
        Rectangle {
            id:     dot
            width:  ScreenTools.defaultFontPixelHeight * 0.9
            height: width
            radius: width / 2
            color:  qgcPal.text

            readonly property real _fx: Math.max(-1, Math.min(1, root.xValue / root.rawMax))
            // Screen Y grows downward; stick "up" (positive Y) should move
            // the dot toward the top, so invert.
            readonly property real _fy: Math.max(-1, Math.min(1, -root.yValue / root.rawMax))
            readonly property real _radius: (face.width - width) / 2

            x: face.width  / 2 - width  / 2 + _fx * _radius
            y: face.height / 2 - height / 2 + _fy * _radius
        }
    }

    QGCLabel {
        anchors.horizontalCenter: parent.horizontalCenter
        text:           Math.round(root.xValue) + "," + Math.round(root.yValue)
        font.pointSize: ScreenTools.mediumFontPointSize
    }
}
