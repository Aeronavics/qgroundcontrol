#include "VehicleSummedBatteryFactGroup.h"
#include "Vehicle.h"
#include <bitset>

const char* VehicleSummedBatteryFactGroup::_voltageFactName               = "voltage";
const char* VehicleSummedBatteryFactGroup::_currentFactName               = "current";
const char* VehicleSummedBatteryFactGroup::_chargeStateFactName           = "chargeState";

VehicleSummedBatteryFactGroup::VehicleSummedBatteryFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/SummedBatteryFact.json", parent)
    , _voltageFact          (0, _voltageFactName,                   FactMetaData::valueTypeDouble)
    , _currentFact          (0, _currentFactName,                   FactMetaData::valueTypeDouble)
    , _chargeStateFact      (0, _chargeStateFactName,               FactMetaData::valueTypeUint8)

{
    _addFact(&_voltageFact,                 _voltageFactName);
    _addFact(&_currentFact,                 _currentFactName);
    _addFact(&_chargeStateFact,             _chargeStateFactName);

    _voltageFact.setRawValue                (qQNaN());
    _currentFact.setRawValue                (qQNaN());
    _chargeStateFact.setRawValue            (MAV_BATTERY_CHARGE_STATE_UNDEFINED);

}

void VehicleSummedBatteryFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_BATTERY_STATUS:
        _handleBatteryStatus(message);
        break;
    default:
        break;
    }
}

void VehicleSummedBatteryFactGroup::_handleBatteryStatus(mavlink_message_t& message)
{
    mavlink_battery_status_t batteryStatus;
    mavlink_msg_battery_status_decode(&message, &batteryStatus);

    double totalVoltage = qQNaN();
    for (int i=0; i<10; i++) {
        double cellVoltage = batteryStatus.voltages[i] == UINT16_MAX ? qQNaN() : static_cast<double>(batteryStatus.voltages[i]) / 1000.0;
        if (qIsNaN(cellVoltage)) {
            break;
        }
        if (i == 0) {
            totalVoltage = cellVoltage;
        } else {
            totalVoltage += cellVoltage;
        }
    }
    for (int i=0; i<4; i++) {
        double cellVoltage = batteryStatus.voltages_ext[i] == UINT16_MAX ? qQNaN() : static_cast<double>(batteryStatus.voltages_ext[i]) / 1000.0;
        if (qIsNaN(cellVoltage)) {
            break;
        }
        totalVoltage += cellVoltage;
    }

    voltage()->setRawValue          (totalVoltage);

    // check if battery has been seen before and if not add it to the list
    if (batteryStatus.id >= _currentList.length())
    {
        while(batteryStatus.id >= _currentList.length())
        {
            _currentList.append(0);
            _chargeStateList.append(0);
        }
    }
    _currentList[batteryStatus.id] = static_cast<double>(batteryStatus.current_battery) / 100.0;
    _chargeStateList[batteryStatus.id] = batteryStatus.charge_state;

    // Sum currents
    double total_current = 0;
    for (int i = 0; i < _currentList.length(); i++)
    {
        total_current += _currentList[i];
    }

    current()->setRawValue(total_current);


    // check for worst charge status
    uint8_t worst_charge_state = 0;
    for (int i = 0; i < _chargeStateList.length(); i++)
    {
        if (_chargeStateList[i] > worst_charge_state)
        {
            worst_charge_state = _chargeStateList[i];
        }
    }

    chargeState()->setRawValue(worst_charge_state);

}
