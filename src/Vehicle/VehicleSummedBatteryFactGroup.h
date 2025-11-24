#pragma once

#include "FactGroup.h"
#include "QGCMAVLink.h"

class VehicleSummedBatteryFactGroup : public FactGroup
{
    Q_OBJECT

public:
    VehicleSummedBatteryFactGroup(QObject* parent = nullptr);

    Q_PROPERTY(Fact* voltage            READ voltage            CONSTANT)
    Q_PROPERTY(Fact* current            READ current            CONSTANT)
    Q_PROPERTY(Fact* chargeState        READ chargeState        CONSTANT)

    Fact* voltage                   () { return &_voltageFact; }
    Fact* current                   () { return &_currentFact; }
    Fact* chargeState               () { return &_chargeStateFact; }

    // Overrides from FactGroup
    virtual void handleMessage(Vehicle* vehicle, mavlink_message_t& message) override;

    static const char* _voltageFactName;
    static const char* _currentFactName;
    static const char* _chargeStateFactName;

protected:
    void _handleBatteryStatus(mavlink_message_t& message);

    Fact            _voltageFact;
    Fact            _currentFact;
    Fact            _chargeStateFact;

    QList<double>   _currentList;
    QList<uint8_t>  _chargeStateList;
};
