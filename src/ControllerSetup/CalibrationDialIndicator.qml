import QtQuick              2.15
import QtQuick.Layouts      1.15

import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0

// Rotate-left / label / value bar / rotate-right row for one dial wheel's
// live position, matching UniGCS's own RC Calibration screen.
RowLayout {
    id:      root
    spacing: ScreenTools.defaultFontPixelWidth * 2

    property string label:  "LD"
    property real   value:  0
    property real   rawMax: 150   // symmetric range: -rawMax..rawMax

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    // Custom icons mimicking the Unicode "↺"/"↻" glyphs (RotateClockwise.svg /
    // RotateCounterClockwise.svg) - those glyphs aren't in every font QGC
    // ships/uses and render as a missing-glyph box. QGC's existing
    // ArrowCW.svg/ArrowCCW.svg were tried first but turned out to just be
    // plain filled triangles, not curved arrows - a poor visual match.
    QGCColoredImage {
        width:      ScreenTools.defaultFontPixelHeight
        height:     width
        source:     "/qmlimages/RotateCounterClockwise.svg"
        color:      qgcPal.colorBlue
    }

    QGCLabel {
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 3
        text:                   root.label
        font.family:            ScreenTools.demiboldFontFamily
    }

    CalibrationChannelBar {
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 24
        from:  -root.rawMax
        to:    root.rawMax
        value: root.value
    }

    QGCLabel {
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 4
        text:                   Math.round(root.value)
    }

    QGCColoredImage {
        width:      ScreenTools.defaultFontPixelHeight
        height:     width
        source:     "/qmlimages/RotateClockwise.svg"
        color:      qgcPal.colorBlue
    }
}
