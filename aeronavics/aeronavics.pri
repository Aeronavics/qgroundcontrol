message("Adding Custom Plugin")

#-- Version control
#   Major and minor versions are defined here (manually)

CUSTOM_QGC_VER_MAJOR = 1
CUSTOM_QGC_VER_MINOR = 1
CUSTOM_QGC_VER_FIRST_BUILD = 1

# Build number is automatic
# Uses the current branch. This way it works on any branch including build-server's PR branches
CUSTOM_QGC_VER_BUILD = $$system(git --git-dir ../.git rev-list $$GIT_BRANCH --first-parent --count)
win32 {
    CUSTOM_QGC_VER_BUILD = $$system("set /a $$CUSTOM_QGC_VER_BUILD - $$CUSTOM_QGC_VER_FIRST_BUILD")
} else {
    CUSTOM_QGC_VER_BUILD = $$system("echo $(($$CUSTOM_QGC_VER_BUILD - $$CUSTOM_QGC_VER_FIRST_BUILD))")
}
CUSTOM_QGC_VERSION = $${CUSTOM_QGC_VER_MAJOR}.$${CUSTOM_QGC_VER_MINOR}.$${CUSTOM_QGC_VER_BUILD}

DEFINES -= APP_VERSION_STR=\"\\\"$$APP_VERSION_STR\\\"\"
DEFINES += APP_VERSION_STR=\"\\\"$$CUSTOM_QGC_VERSION\\\"\"

message(Custom QGC Version: $${CUSTOM_QGC_VERSION})

# Build a single flight stack by disabling PX4 support
CONFIG  += QGC_DISABLE_PX4_MAVLINK
CONFIG  += QGC_DISABLE_PX4_PLUGIN QGC_DISABLE_PX4_PLUGIN_FACTORY

# We implement our own APM plugin factory
CONFIG  += QGC_DISABLE_APM_PLUGIN_FACTORY

# Branding

DEFINES += CUSTOMHEADER=\"\\\"AeronavicsPlugin.h\\\"\"
DEFINES += CUSTOMCLASS=AeronavicsPlugin

TARGET   = CustomQGroundControl
DEFINES += QGC_APPLICATION_NAME='"\\\"Aeronavics QGroundControl\\\""'

DEFINES += QGC_ORG_NAME=\"\\\"qgroundcontrol.org\\\"\"
DEFINES += QGC_ORG_DOMAIN=\"\\\"org.qgroundcontrol\\\"\"

QGC_APP_NAME        = "Aeronavics QGroundControl"
QGC_BINARY_NAME     = "AeronavicsQGroundControl"
QGC_ORG_NAME        = "Aeronavics"
QGC_ORG_DOMAIN      = "org.aeronavics"
QGC_ANDROID_PACKAGE = "org.aeronavics.qgroundcontrol"
QGC_APP_DESCRIPTION = "Aeronavics QGroundControl"
QGC_APP_COPYRIGHT   = "Copyright (C) 2020 QGroundControl Development Team. All rights reserved."

# Our own, custom resources
RESOURCES += \
    $$PWD/aeronavics.qrc

QML_IMPORT_PATH += \
   $$PWD/res

# Our own, custom sources
SOURCES += \
    $$PWD/src/AeronavicsPlugin.cc \
    $$PWD/src/FirmwarePlugin/AeronavicsCopterFirmwarePlugin.cc

HEADERS += \
    $$PWD/src/AeronavicsPlugin.h \
    $$PWD/src/FirmwarePlugin/AeronavicsCopterFirmwarePlugin.h

INCLUDEPATH += \
    $$PWD/src \

#-------------------------------------------------------------------------------------
# Custom Firmware/AutoPilot Plugin

INCLUDEPATH += \
    $$PWD/src/FirmwarePlugin \
    $$PWD/src/AutoPilotPlugin

HEADERS+= \
    $$PWD/src/AutoPilotPlugin/AeronavicsAutoPilotPlugin.h \
    $$PWD/src/FirmwarePlugin/AeronavicsFirmwarePlugin.h \
    $$PWD/src/FirmwarePlugin/AeronavicsFirmwarePluginFactory.h \

SOURCES += \
    $$PWD/src/AutoPilotPlugin/AeronavicsAutoPilotPlugin.cc \
    $$PWD/src/FirmwarePlugin/AeronavicsFirmwarePlugin.cc \
    $$PWD/src/FirmwarePlugin/AeronavicsFirmwarePluginFactory.cc \

