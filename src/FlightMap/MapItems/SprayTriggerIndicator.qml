/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.3
import QtLocation       5.3
import QtQuick.Controls 1.2

import QGroundControl.ScreenTools   1.0
import QGroundControl.Controls      1.0
import QGroundControl.Vehicle       1.0
import QGroundControl.Palette       1.0

/// Marker for displaying a spray trigger on the map
MapQuickItem {
   anchorPoint.x:  sourceItem.width / 2
   anchorPoint.y:  sourceItem.height / 2

   sourceItem: Canvas {
      id: pinCanvas
      width: 40
      height: 60
      antialiasing: true

      onPaint: {
             var ctx = getContext("2d");
             ctx.reset();

             var cx = width / 2;      // Center X
             var cy = width / 2;      // Center Y of the top circle
             var r = width / 2 - 2;   // Radius of top circle to leave padding
             var tipY = height - 2;   // Bottom tip of the pin

             // 1. Draw Pin Drop Shadow
             ctx.beginPath();
             ctx.ellipse(cx, tipY, 8, 3);
             ctx.fillStyle = "rgba(0, 0, 0, 0.25)";
             ctx.fill();

             // 2. Build the main Map Pin path (teardrop)
             ctx.beginPath();
             // Arc around the top circle (from right-bottom angle back to left-bottom angle)
             ctx.arc(cx, cy, r, 0.15 * Math.PI, 0.85 * Math.PI, true);
             // Line down to the pinpoint bottom vertex
             ctx.lineTo(cx, tipY);
             ctx.closePath();

             // Fill pin body with green
             ctx.fillStyle = "#009431";
             ctx.fill();

             // Add a subtle darker green border for definition
             ctx.lineWidth = 1.5;
             ctx.strokeStyle = "#007025";
             ctx.stroke();

      }

      QGCColoredImage {
          height:             parent.height * 0.45
          width:              height
          anchors.top:        parent.top
          anchors.topMargin:  7
          anchors.horizontalCenter: parent.horizontalCenter
          source:             "/qmlimages/waterdrop.svg"
          fillMode:           Image.PreserveAspectFit
          sourceSize.height:  height
          color:              qgcPal.colorWhite
      }
   }
}
