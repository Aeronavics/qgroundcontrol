import QtQuick              2.15
import QtQuick.Controls     2.15

import QGroundControl.Palette       1.0
import QGroundControl.ScreenTools   1.0

// Small pill-shaped channel bar for the live calibration readout in
// ControllerCalibration.qml. Same clip-reveal fill technique as the channel
// bars in ChannelSetup.qml (kept in sync deliberately - factored out here so
// there's only one copy of it): a full-size green pill is clipped down to the
// progress fraction rather than drawn as an independently-rounded small
// rectangle, because rounding a narrow rectangle to its own half-height
// doesn't taper the same way the background's semicircular cap does, and
// renders taller than the cap at low values.
Item {
    id:     root
    width:  70
    height: 14

    property real value:  1500
    property real from:   1050
    property real to:     1950
    // Clamp to [0,1]: `value` can start at 0 (before real RC data arrives),
    // which is below `from`, so this would otherwise go negative/over 1.
    readonly property real _fraction: Math.max(0, Math.min(1, (value - from) / (to - from)))

    // On the calibration page (CalibrationDialIndicator), `value` is fed by
    // the RCU's raw analog stream, which only pushes new values at ~17Hz -
    // interpolate toward each new value instead of stepping, so the bar
    // reads as smooth motion rather than visibly jumping every ~59ms.
    // Harmless for other users of this component (e.g. one-shot values):
    // a Behavior only animates when the bound value actually changes.
    Behavior on value { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Rectangle {
        id:             background
        anchors.fill:   parent
        radius:         Math.min(width, height) / 2
        color:          qgcPal.window
        border.color:   qgcPal.text
        border.width:   1
    }

    Item {
        id:     fillClip
        clip:   true
        height: parent.height
        width:  root._fraction * parent.width

        Rectangle {
            color:  qgcPal.colorGreen
            radius: root.height / 2
            width:  fillClip.parent.width
            height: parent.height
        }
    }
}
