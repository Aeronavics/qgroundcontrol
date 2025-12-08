/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.11
import QtQuick.Layouts  1.11

import QGroundControl                       1.0
import QGroundControl.Controls              1.0
import QGroundControl.MultiVehicleManager   1.0
import QGroundControl.ScreenTools           1.0
import QGroundControl.Palette               1.0
import MAVLink                              1.0

//-------------------------------------------------------------------------
//-- Fuel Indicator
Item {
    id:             _root
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width: fuelRow.width * 1.1

    property var generator: _activeVehicle.generator

    property bool showIndicator: generator.generatorSeen.value

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    function getFuelColor() {
        if (generator.fuelRemaining.rawValue >= 15)
        {
            return qgcPal.text
        }
        else if (generator.fuelRemaining.rawValue >= 5)
        {
            return qgcPal.colorOrange
        }
        else
        {
            return qgcPal.colorRed
        }
    }

    function getFuelPercentageText() {
        if (!isNaN(generator.fuelRemaining.rawValue)) {
            return generator.fuelRemaining.valueString + generator.fuelRemaining.units
        }
        return ""
    }

    Row {
        id:             fuelRow
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth

        QGCColoredImage {
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            width:              height
            sourceSize.width:   width
            source:             "/qmlimages/fuel.svg"
            fillMode:           Image.PreserveAspectFit
            color:              getFuelColor()
        }

        QGCLabel {
            text:                   getFuelPercentageText()
            font.pointSize:         ScreenTools.mediumFontPointSize
            color:                  getFuelColor()
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
