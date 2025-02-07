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
    Q_PROPERTY(Fact* sprayeVolume       READ sprayeVolume       CONSTANT)
    Q_PROPERTY(Fact* sprayRemaining     READ sprayRemaining     CONSTANT)
    Q_PROPERTY(Fact* mesPressure        READ mesPressure        CONSTANT)
    Q_PROPERTY(Fact* error              READ error              CONSTANT)

    Fact* mesFlowrate       () { return &_mesFlowrateFact; }
    Fact* desFlowrate       () { return &_desFlowrateFact; }
    Fact* sprayeVolume      () { return &_sprayeVolumeFact; }
    Fact* sprayRemaining    () { return &_sprayRemainingFact; }
    Fact* mesPressure       () { return &_mesPressureFact; }
    Fact* error             () { return &_errorFact; }

    // Overrides from FactGroup
    virtual void handleMessage(Vehicle* vehicle, mavlink_message_t& message) override;

    static const char* _mesFlowrateFactName;
    static const char* _desFlowrateFactName;
    static const char* _sprayeVolumeFactName;
    static const char* _sprayRemainingFactName;
    static const char* _mesPressureFactName;
    static const char* _errorFactName;

protected:
    void _handleSprayStatus(mavlink_message_t& message);

    Fact _mesFlowrateFact;
    Fact _desFlowrateFact;
    Fact _sprayeVolumeFact;
    Fact _sprayRemainingFact;
    Fact _mesPressureFact;
    Fact _errorFact;
};
