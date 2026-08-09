#include "ControllerHandler.h"

ControllerHandler::ControllerHandler()
{
    _siyiSdk.setChannelListener([this](const std::array<qint16,16>& ch){
        setAllChannelValues(ch);
    });

    _siyiSdk.start();

    _channels = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    _channelMappings = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
    _channelReverses = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

    pullChannelMappings();
    pullChannelReverse();

    // _rcu.setChannelListener([this](const std::array<qint16,16>& ch){
    //     setChannels(ch);
    // });

    // _rcu.open();
}


ControllerHandler::~ControllerHandler()
{

}

void ControllerHandler::setAllChannelValues(std::array<qint16,16> channels)
{
    // qDebug() << "Channel 1: " << channels[0];
    QVariantList tempList;
    tempList.reserve(channels.size());
    for (qint16 val : channels){
        tempList.append(val);
    }
    // _channels = QList(channels.begin(), channels.end());
    _channels = tempList;
    emit channelsChanged();
}


void ControllerHandler::pullChannelMappings()
{
    std::vector<unircsdk::UniRcSdk::Mapping> _tempChannelMappings;
    _siyiSdk.getAllChannelMappings(_tempChannelMappings);

    QVariantList tempList;
    tempList.reserve(_tempChannelMappings.size());
    for (unircsdk::UniRcSdk::Mapping val : _tempChannelMappings){
        tempList.append(val.name().c_str());
    }

    _channelMappings = tempList;
    emit channelMappingsChanged();
}

void ControllerHandler::pullChannelReverse()
{
    std::vector<int> _tempChannelReverses;
    _siyiSdk.getAllReverse(_tempChannelReverses);

    QVariantList tempList;
    tempList.reserve(_tempChannelReverses.size());
    for (int val : _tempChannelReverses)
    {
        tempList.append(val == 1 ? false : true);
    }
    _channelReverses = tempList;
    emit channelReversesChanged();
}

void ControllerHandler::setChannelReverse(int channel, bool reverse)
{
    qDebug() << "channel: " << channel << " reverse: " << reverse;
    QtConcurrent::run(this, &ControllerHandler::setChannelReverseThread, channel, reverse);
}

void ControllerHandler::setChannelReverseThread(int channel, bool reverse)
{
    _siyiSdk.setChannelReverse(channel, reverse);
    pullChannelReverse();
}

void ControllerHandler::shutdown_sdk()
{
    _siyiSdk.close();
}
