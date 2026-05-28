/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick                      2.11
import QtQuick.Controls             2.4
import QtQml.Models                 2.1

import QGroundControl               1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controls      1.0
import QGroundControl.FlightDisplay 1.0
import QGroundControl.Vehicle       1.0

Item {
    property var model: listModel
    PreFlightCheckModel {
        id:     listModel

        PreFlightCheckGroup {
            name: qsTr("Hand Controller Checks")

            PreFlightCheckButton {
                name:           qsTr("Battery")
                manualText:     qsTr("Charged?")
            }

            PreFlightCheckButton {
                name:           qsTr("Antenna")
                manualText:     qsTr("Damage Free? Extended?")
            }

            PreFlightCheckButton {
                name:           qsTr("Controls")
                manualText:     qsTr("Damage Free?")
            }

            PreFlightCheckButton {
                name:           qsTr("Flight mode")
                manualText:     qsTr("Set Correcty?")
            }
        }

        PreFlightCheckGroup {
            name: qsTr("Aircraft Checks")

            PreFlightCheckButton {
                name:           qsTr("Booms Connectors")
                manualText:     qsTr("Clean and Damage Free?")
            }

            PreFlightCheckButton {
                name:           qsTr("Booms")
                manualText:     qsTr("Installed and latched?")
            }

            PreFlightCheckButton {
                name:           qsTr("Motors")
                manualText:     qsTr("Free Rotation?")
            }

            PreFlightCheckButton {
                name:           qsTr("Propellers")
                manualText:     qsTr("Mounted and secured?")
            }

            PreFlightCheckButton {
                name:           qsTr("Tanks")
                manualText:     qsTr("Connected and Latched? Lids Tight?")
            }

            PreFlightCheckButton {
                name:           qsTr("Antenna")
                manualText:     qsTr("Damage Free?")
            }

            PreFlightCheckButton {
                name:           qsTr("Spray System")
                manualText:     qsTr("No Leaks?")
            }

            PreFlightCheckButton {
                name:           qsTr("Fuel System")
                manualText:     qsTr("No Leaks?")
            }

            PreFlightBatteryCheck {
                failureVoltage:                 56
                allowFailureVoltageOverride:    false
            }

            PreFlightSensorsHealthCheck {
            }

            PreFlightGPSCheck {
                failureSatCount:        9
                allowOverrideSatCount:  true
            }

            PreFlightCheckButton {
                name:           qsTr("Flight area")
                manualText:     qsTr("Launch area and path free of obstacles/people?")
            }

            // PreFlightRCCheck {
            // }
        }

        // PreFlightCheckGroup {
        //     name: qsTr("Please arm the vehicle here")

        //     PreFlightCheckButton {
        //         name:            qsTr("Motors")
        //         manualText:      qsTr("Propellers free? Then throttle up gently. Working properly?")
        //     }

        //     PreFlightCheckButton {
        //         name:           qsTr("Mission")
        //         manualText:     qsTr("Please confirm mission is valid (waypoints valid, no terrain collision).")
        //     }

        //     PreFlightSoundCheck {
        //     }
        // }

        // PreFlightCheckGroup {
        //     name: qsTr("Last preparations before launch")

        //     // Check list item group 2 - Final checks before launch
        //     PreFlightCheckButton {
        //         name:           qsTr("Payload")
        //         manualText:     qsTr("Configured and started? Payload lid closed?")
        //     }

        //     PreFlightCheckButton {
        //         name:           qsTr("Wind & weather")
        //         manualText:     qsTr("OK for your platform?")
        //     }
        // }
    }
}

