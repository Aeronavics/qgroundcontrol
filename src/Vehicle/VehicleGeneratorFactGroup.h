#pragma once

#include "FactGroup.h"
#include "QGCMAVLink.h"

class VehicleGeneratorFactGroup : public FactGroup
{
    Q_OBJECT

public:
    VehicleGeneratorFactGroup(QObject* parent = nullptr);

    Q_PROPERTY(Fact* status                     READ status             CONSTANT)
    Q_PROPERTY(Fact* rpm                        READ rpm                CONSTANT)
    Q_PROPERTY(Fact* current                    READ current            CONSTANT)
    Q_PROPERTY(Fact* power                      READ power              CONSTANT)
    Q_PROPERTY(Fact* voltage                    READ voltage            CONSTANT)
    Q_PROPERTY(Fact* coilTemp                   READ coilTemp           CONSTANT)
    Q_PROPERTY(Fact* genTemp                    READ genTemp            CONSTANT)
    Q_PROPERTY(Fact* runtime                    READ runtime            CONSTANT)
    Q_PROPERTY(Fact* timeToService              READ timeToService      CONSTANT)
    Q_PROPERTY(Fact* fuelRemaining              READ fuelRemaining      CONSTANT)
    Q_PROPERTY(QVariantList flagsListGenerator  READ flagsListGenerator  NOTIFY flagsListGeneratorChanged)
    Q_PROPERTY(Fact* generatorSeen              READ generatorSeen      CONSTANT)

    Fact* status                    () { return &_statusFact; }
    Fact* rpm                       () { return &_rpmFact; }
    Fact* current                   () { return &_currentFact; }
    Fact* power                     () { return &_powerFact; }
    Fact* voltage                   () { return &_voltageFact; }
    Fact* coilTemp                  () { return &_coilTempFact; }
    Fact* genTemp                   () { return &_genTempFact; }
    Fact* runtime                   () { return &_runtimeFact; }
    Fact* timeToService             () { return &_timeToServiceFact; }
    Fact* fuelRemaining             () { return &_fuelRemainingFact; }
    QVariantList& flagsListGenerator() { return _flagsListGenerator; }
    Fact* generatorSeen             () { return &_generatorSeenFact; }

    // Overrides from FactGroup
    virtual void handleMessage(Vehicle* vehicle, mavlink_message_t& message) override;

    static const char* _statusFactName;
    static const char* _rpmFactName;
    static const char* _currentFactName;
    static const char* _powerFactName;
    static const char* _voltageFactName;
    static const char* _coilTempFactName;
    static const char* _genTempFactName;
    static const char* _runtimeFactName;
    static const char* _timeToServiceFactName;
    static const char* _fuelRemainingFactName;
    static const char* _generatorSeenFactName;

signals:
    void flagsListGeneratorChanged();

protected:
    void _handleGeneratorStatus(mavlink_message_t& message);
    void _updateGeneratorFlags();

    void _handleFuelStatus(mavlink_message_t& message);

    Fact _statusFact;
    Fact _rpmFact;
    Fact _currentFact;
    Fact _powerFact;
    Fact _voltageFact;
    Fact _coilTempFact;
    Fact _genTempFact;
    Fact _runtimeFact;
    Fact _timeToServiceFact;
    Fact _fuelRemainingFact;
    Fact _generatorSeenFact;

    QVariantList _flagsListGenerator;
    int _prevFlag;
};
