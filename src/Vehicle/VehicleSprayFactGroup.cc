#include "VehicleSprayFactGroup.h"
#include "Vehicle.h"
#include <bitset>

const char* VehicleSprayFactGroup::_mesFlowrateFactName =       "mesFlowrate";
const char* VehicleSprayFactGroup::_desFlowrateFactName =       "desFlowrate";
const char* VehicleSprayFactGroup::_sprayeVolumeFactName =      "sprayeVolume";
const char* VehicleSprayFactGroup::_sprayRemainingFactName =    "sprayRemaining";
const char* VehicleSprayFactGroup::_mesPressureFactName =       "mesPressure";
const char* VehicleSprayFactGroup::_errorFactName =             "error";

VehicleSprayFactGroup::VehicleSprayFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/SprayFact.json", parent)
    , _mesFlowrateFact      (0, _mesFlowrateFactName,       FactMetaData::valueTypeUint16)
    , _desFlowrateFact      (0, _desFlowrateFactName,       FactMetaData::valueTypeUint16)
    , _sprayeVolumeFact     (0, _sprayeVolumeFactName,      FactMetaData::valueTypeFloat)
    , _sprayRemainingFact   (0, _sprayRemainingFactName,    FactMetaData::valueTypeFloat)
    , _mesPressureFact      (0, _mesPressureFactName,       FactMetaData::valueTypeUint16)
    , _errorFact            (0, _errorFactName,             FactMetaData::valueTypeUint8)
{
    _addFact(&_mesFlowrateFact,     _mesFlowrateFactName);
    _addFact(&_desFlowrateFact,     _desFlowrateFactName);
    _addFact(&_sprayeVolumeFact,    _sprayeVolumeFactName);
    _addFact(&_sprayRemainingFact,  _sprayRemainingFactName);
    _addFact(&_mesPressureFact,     _mesPressureFactName);
    _addFact(&_errorFact,           _errorFactName);

    // Start out as not available "--.--"
    _mesFlowrateFact.setRawValue(qQNaN());
    _desFlowrateFact.setRawValue(qQNaN());
    _sprayeVolumeFact.setRawValue(qQNaN());
    _sprayRemainingFact.setRawValue(qQNaN());
    _mesPressureFact.setRawValue(qQNaN());
    _errorFact.setRawValue(qQNaN());
}

void VehicleSprayFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_ANV_MSG_SPRAY_STATUS:
        _handleSprayStatus(message);
        break;
    default:
        break;
    }
}

void VehicleSprayFactGroup::_handleSprayStatus(mavlink_message_t& message)
{
    mavlink_anv_msg_spray_status_t spray;
    mavlink_msg_anv_msg_spray_status_decode(&message, &spray);

    mesFlowrate()->setRawValue      (spray.measured_flowrate);
    desFlowrate()->setRawValue      (spray.desired_flowrate);
    sprayeVolume()->setRawValue     (spray.sprayed_volume);
    sprayRemaining()->setRawValue   (spray.spray_remaining);
    mesPressure()->setRawValue      (spray.pressure);
    error()->setRawValue            (spray.error);
}
