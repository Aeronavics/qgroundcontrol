#include "VehicleSprayFactGroup.h"
#include "Vehicle.h"
#include <bitset>

const char* VehicleSprayFactGroup::_mesFlowrateFactName =       "mesFlowrate";
const char* VehicleSprayFactGroup::_desFlowrateFactName =       "desFlowrate";
const char* VehicleSprayFactGroup::_totalSprayedVolumeFactName =    "totalVolume";
const char* VehicleSprayFactGroup::_armedSprayedVolumeFactName =    "armedVolume";
const char* VehicleSprayFactGroup::_lastTreeVolumeFactName =    "lastTreeVolume";
const char* VehicleSprayFactGroup::_sprayRemainingFactName =    "sprayRemaining";
const char* VehicleSprayFactGroup::_mesPressureFactName =       "mesPressure";
const char* VehicleSprayFactGroup::_errorFactName =             "error";
const char* VehicleSprayFactGroup::_sprayerSeenFactName =             "sprayerEnabled";

VehicleSprayFactGroup::VehicleSprayFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/SprayFact.json", parent)
    , _mesFlowrateFact          (0, _mesFlowrateFactName,           FactMetaData::valueTypeUint16)
    , _desFlowrateFact          (0, _desFlowrateFactName,           FactMetaData::valueTypeUint16)
    , _totalSprayedVolumeFact   (0, _totalSprayedVolumeFactName,    FactMetaData::valueTypeFloat)
    , _armedSprayedVolumeFact   (0, _armedSprayedVolumeFactName,    FactMetaData::valueTypeFloat)
    , _lastTreeVolumeFact       (0, _lastTreeVolumeFactName,        FactMetaData::valueTypeFloat)
    , _sprayRemainingFact       (0, _sprayRemainingFactName,        FactMetaData::valueTypeFloat)
    , _mesPressureFact          (0, _mesPressureFactName,           FactMetaData::valueTypeUint16)
    , _errorFact                (0, _errorFactName,                 FactMetaData::valueTypeUint8)
    , _sprayerSeenFact          (0, _sprayerSeenFactName,           FactMetaData::valueTypeBool)
{
    _addFact(&_mesFlowrateFact,         _mesFlowrateFactName);
    _addFact(&_desFlowrateFact,         _desFlowrateFactName);
    _addFact(&_totalSprayedVolumeFact,  _totalSprayedVolumeFactName);
    _addFact(&_armedSprayedVolumeFact,  _armedSprayedVolumeFactName);
    _addFact(&_lastTreeVolumeFact,      _lastTreeVolumeFactName);
    _addFact(&_sprayRemainingFact,      _sprayRemainingFactName);
    _addFact(&_mesPressureFact,         _mesPressureFactName);
    _addFact(&_errorFact,               _errorFactName);
    _addFact(&_sprayerSeenFact,         _sprayerSeenFactName);

    // Start out as not available "--.--"
    _mesFlowrateFact.setRawValue(qQNaN());
    _desFlowrateFact.setRawValue(qQNaN());
    _totalSprayedVolumeFact.setRawValue(qQNaN());
    _armedSprayedVolumeFact.setRawValue(qQNaN());
    _lastTreeVolumeFact.setRawValue(qQNaN());
    _sprayRemainingFact.setRawValue(qQNaN());
    _mesPressureFact.setRawValue(qQNaN());
    _errorFact.setRawValue(qQNaN());
    _sprayerSeenFact.setRawValue(false);
}

void VehicleSprayFactGroup::handleMessage(Vehicle* /* vehicle */, mavlink_message_t& message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_ANV_SPRAY_STATUS:
        _handleSprayStatus(message);
        break;
    default:
        break;
    }
}

void VehicleSprayFactGroup::_handleSprayStatus(mavlink_message_t& message)
{
    mavlink_anv_spray_status_t spray;
    mavlink_msg_anv_spray_status_decode(&message, &spray);

    mesFlowrate()->setRawValue          (spray.measured_flowrate);
    desFlowrate()->setRawValue          (spray.desired_flowrate);
    totalSprayedVolume()->setRawValue   (spray.total_sprayed_volume);
    armedSprayedVolume()->setRawValue   (spray.armed_sprayed_volume);
    lastTreeVolume()->setRawValue       (spray.last_tree_volume);
    sprayRemaining()->setRawValue       (spray.spray_remaining);
    mesPressure()->setRawValue          (spray.pressure);
    error()->setRawValue                (spray.error);
    sprayerSeen()->setRawValue          (true);
}
