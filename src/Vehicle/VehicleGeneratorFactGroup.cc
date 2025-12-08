#include "VehicleGeneratorFactGroup.h"
#include "Vehicle.h"
#include <bitset>

const char* VehicleGeneratorFactGroup::_statusFactName =                "status";
const char* VehicleGeneratorFactGroup::_rpmFactName =                   "RPM";
const char* VehicleGeneratorFactGroup::_currentFactName =               "current";
const char* VehicleGeneratorFactGroup::_powerFactName =                 "power";
const char* VehicleGeneratorFactGroup::_voltageFactName =               "voltage";
const char* VehicleGeneratorFactGroup::_coilTempFactName =              "coilTemp";
const char* VehicleGeneratorFactGroup::_genTempFactName =               "genTemp";
const char* VehicleGeneratorFactGroup::_runtimeFactName =               "runtime";
const char* VehicleGeneratorFactGroup::_timeToServiceFactName =         "timeToService";
const char* VehicleGeneratorFactGroup::_fuelRemainingFactName =         "fuelRemaining";
const char* VehicleGeneratorFactGroup::_generatorSeenFactName =         "genEnabled";

VehicleGeneratorFactGroup::VehicleGeneratorFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/GeneratorFact.json", parent)
    , _statusFact           (0, _statusFactName,            FactMetaData::valueTypeUint64)
    , _rpmFact              (0, _rpmFactName,               FactMetaData::valueTypeUint16)
    , _currentFact          (0, _currentFactName,           FactMetaData::valueTypeFloat)
    , _powerFact            (0, _powerFactName,             FactMetaData::valueTypeFloat)
    , _voltageFact          (0, _voltageFactName,           FactMetaData::valueTypeFloat)
    , _coilTempFact         (0, _coilTempFactName,          FactMetaData::valueTypeInt16)
    , _genTempFact          (0, _genTempFactName,           FactMetaData::valueTypeInt16)
    , _runtimeFact          (0, _runtimeFactName,           FactMetaData::valueTypeString)
    , _timeToServiceFact    (0, _timeToServiceFactName,     FactMetaData::valueTypeString)
    , _fuelRemainingFact    (0, _fuelRemainingFactName,     FactMetaData::valueTypeUint8)
    , _generatorSeenFact    (0, _generatorSeenFactName,     FactMetaData::valueTypeBool)
{
    _addFact(&_statusFact,          _statusFactName);
    _addFact(&_rpmFact,             _rpmFactName);
    _addFact(&_currentFact,         _currentFactName);
    _addFact(&_powerFact,           _powerFactName);
    _addFact(&_voltageFact,         _voltageFactName);
    _addFact(&_coilTempFact,        _coilTempFactName);
    _addFact(&_genTempFact,         _genTempFactName);
    _addFact(&_runtimeFact,         _runtimeFactName);
    _addFact(&_timeToServiceFact,   _timeToServiceFactName);
    _addFact(&_fuelRemainingFact,   _fuelRemainingFactName);

    // Start out as not available "--.--"
    _statusFact.setRawValue(qQNaN());
    _rpmFact.setRawValue(qQNaN());
    _currentFact.setRawValue(qQNaN());
    _powerFact.setRawValue(qQNaN());
    _voltageFact.setRawValue(qQNaN());
    _coilTempFact.setRawValue(qQNaN());
    _genTempFact.setRawValue(qQNaN());
    _runtimeFact.setRawValue(qQNaN());
    _timeToServiceFact.setRawValue("--:--");
    _fuelRemainingFact.setRawValue("--:--");
    _generatorSeenFact.setRawValue(false);
}

void VehicleGeneratorFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_GENERATOR_STATUS:
        _handleGeneratorStatus(message);
        break;
    case MAVLINK_MSG_ID_FUEL_STATUS:
        _handleFuelStatus(message);
        break;
    default:
        break;
    }
}

void VehicleGeneratorFactGroup::_handleGeneratorStatus(mavlink_message_t& message)
{
    mavlink_generator_status_t generator;
    mavlink_msg_generator_status_decode(&message, &generator);

    status()->setRawValue       (generator.status == UINT16_MAX ? qQNaN() : generator.status);
    rpm()->setRawValue          (generator.generator_speed == UINT16_MAX ? qQNaN() : generator.generator_speed);
    current()->setRawValue      (generator.load_current);
    power()->setRawValue        (generator.power_generated);
    voltage()->setRawValue      (generator.bus_voltage);
    coilTemp()->setRawValue     (generator.rectifier_temperature == INT16_MAX ? qQNaN() : generator.rectifier_temperature);
    genTemp()->setRawValue      (generator.generator_temperature == INT16_MAX ? qQNaN() : generator.generator_temperature);

    _updateGeneratorFlags();

    uint32_t run_time_hours = generator.runtime / 3600;
    uint32_t run_time_minutes = (generator.runtime % 3600) / 60;
    std::string run_time_string = std::to_string(run_time_hours) + "h " + std::to_string(run_time_minutes) + "m";
    runtime()->setRawValue(run_time_string.c_str());

    uint32_t maintenance_time_hours = 0;
    uint32_t maintenance_time_minutes = 0;
    if (generator.time_until_maintenance < 0)
    {
        maintenance_time_hours = generator.time_until_maintenance / 3600;
        maintenance_time_minutes = (generator.time_until_maintenance % 3600) / 60;
    }

    std::string maintenance_time_string = std::to_string(maintenance_time_hours) + "h " + std::to_string(maintenance_time_minutes) + "m";
    timeToService()->setRawValue(maintenance_time_string.c_str());

    generatorSeen()->setRawValue(true);
}

void VehicleGeneratorFactGroup::_handleFuelStatus(mavlink_message_t& message) {
    mavlink_fuel_status_t fuel_status;
    mavlink_msg_fuel_status_decode(&message, &fuel_status);
    fuelRemaining()->setRawValue(fuel_status.percent_remaining);
}

void VehicleGeneratorFactGroup::_updateGeneratorFlags() {

    // Check the status received, and convert it to a List with the state of each flag
    int statusFlag = _statusFact.rawValue().toInt();

    // No need to update the list if we have the same flags
    if ( statusFlag == _prevFlag) {
        return;
    }

    _prevFlag = statusFlag;
    _flagsListGenerator.clear();

    std::bitset<23> bitsetFlags(statusFlag);

    for (size_t i=0; i<bitsetFlags.size(); i++) {
        if (bitsetFlags.test(i)) {
            _flagsListGenerator.append(1);
        } else {
            _flagsListGenerator.append(0);
        }
    }
    emit flagsListGeneratorChanged();
}
