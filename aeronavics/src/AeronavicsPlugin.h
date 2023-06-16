/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Custom QGCCorePlugin Declaration
 *   @author Gus Grubba <gus@auterion.com>
 */

#pragma once

#include "QGCCorePlugin.h"
#include "QGCOptions.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"

#include <QTranslator>

class AeronavicsOptions;
class AeronavicsPlugin;
class AeronavicsSettings;

Q_DECLARE_LOGGING_CATEGORY(CustomLog)

class AeronavicsFlyViewOptions : public QGCFlyViewOptions
{
public:
    AeronavicsFlyViewOptions(AeronavicsOptions* options, QObject* parent = nullptr);

    // Overrides from CustomFlyViewOptions
    bool                    showInstrumentPanel         (void) const final;
    bool                    showMultiVehicleList        (void) const final;
};

class AeronavicsOptions : public QGCOptions
{
public:
    AeronavicsOptions(AeronavicsPlugin*, QObject* parent = nullptr);

    // Overrides from QGCOptions
    bool                    wifiReliableForCalibration  (void) const final;
    bool                    showFirmwareUpgrade         (void) const final;
    bool                    checkFirmwareVersion        (void) const final;
    QGCFlyViewOptions*      flyViewOptions(void) final;

private:
    AeronavicsFlyViewOptions* _flyViewOptions = nullptr;
};

class AeronavicsPlugin : public QGCCorePlugin
{
    Q_OBJECT
public:
    AeronavicsPlugin(QGCApplication* app, QGCToolbox *toolbox);
    ~AeronavicsPlugin();

    // Overrides from QGCCorePlugin
    QVariantList&           settingsPages                   (void) final;
    QGCOptions*             options                         (void) final;
    QString                 brandImageIndoor                (void) const final;
    QString                 brandImageOutdoor               (void) const final;
    bool                    overrideSettingsGroupVisibility (QString name) final;
    bool                    adjustSettingMetaData           (const QString& settingsGroup, FactMetaData& metaData) final;
    void                    paletteOverride                 (QString colorName, QGCPalette::PaletteColorInfo_t& colorInfo) final;
    QQmlApplicationEngine*  createQmlApplicationEngine      (QObject* parent) final;

    // Overrides from QGCTool
    void                    setToolbox                      (QGCToolbox* toolbox);

private slots:
    void _advancedChanged(bool advanced);

private:
    void _addSettingsEntry(const QString& title, const char* qmlFile, const char* iconFile = nullptr);

private:
    AeronavicsOptions*  _options = nullptr;
    QVariantList    _customSettingsList; // Not to be mixed up with QGCCorePlugin implementation
};
