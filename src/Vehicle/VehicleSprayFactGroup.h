#pragma once

#include "FactGroup.h"
#include "QGCMAVLink.h"

class VehicleSprayFactGroup : public FactGroup
{
    Q_OBJECT

public:
    VehicleSprayFactGroup(QObject* parent = nullptr);

    Q_PROPERTY(Fact* mesFlowrate        READ mesFlowrate        CONSTANT)
    Q_PROPERTY(Fact* desFlowrate        READ desFlowrate        CONSTANT)
    Q_PROPERTY(Fact* totalSprayedVolume READ totalSprayedVolume CONSTANT)
    Q_PROPERTY(Fact* armedSprayedVolume READ armedSprayedVolume CONSTANT)
    Q_PROPERTY(Fact* lastTreeVolume     READ lastTreeVolume     CONSTANT)
    Q_PROPERTY(Fact* sprayRemaining     READ sprayRemaining     CONSTANT)
    Q_PROPERTY(Fact* mesPressure        READ mesPressure        CONSTANT)
    Q_PROPERTY(Fact* error              READ error              CONSTANT)

    Fact* mesFlowrate       () { return &_mesFlowrateFact; }
    Fact* desFlowrate       () { return &_desFlowrateFact; }
    Fact* totalSprayedVolume() { return &_totalSprayedVolumeFact; }
    Fact* armedSprayedVolume() { return &_armedSprayedVolumeFact; }
    Fact* lastTreeVolume    () { return &_lastTreeVolumeFact; }
    Fact* sprayRemaining    () { return &_sprayRemainingFact; }
    Fact* mesPressure       () { return &_mesPressureFact; }
    Fact* error             () { return &_errorFact; }

    // Overrides from FactGroup
    virtual void handleMessage(Vehicle* vehicle, mavlink_message_t& message) override;

    static const char* _mesFlowrateFactName;
    static const char* _desFlowrateFactName;
    static const char* _totalSprayedVolumeFactName;
    static const char* _armedSprayedVolumeFactName;
    static const char* _lastTreeVolumeFactName;
    static const char* _sprayRemainingFactName;
    static const char* _mesPressureFactName;
    static const char* _errorFactName;

protected:
    void _handleSprayStatus(mavlink_message_t& message);

    Fact _mesFlowrateFact;
    Fact _desFlowrateFact;
    Fact _totalSprayedVolumeFact;
    Fact _armedSprayedVolumeFact;
    Fact _lastTreeVolumeFact;
    Fact _sprayRemainingFact;
    Fact _mesPressureFact;
    Fact _errorFact;
};
