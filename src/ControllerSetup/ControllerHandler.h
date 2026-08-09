/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#ifndef ControllerHandler_H
#define ControllerHandler_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QFileInfoList>
#include <QElapsedTimer>
#include <QDebug>
#include <QList>
#include <array>
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>

#include <Siyi/unirc_sdk.h>
#include <Siyi/rcu_session.h>

/// Controller for ControllerHandler.qml.
class ControllerHandler : public QObject
{
    Q_OBJECT
public:
    ControllerHandler();
    ~ControllerHandler();

    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(QVariantList channelMappings READ channelMappings NOTIFY channelMappingsChanged)
    Q_PROPERTY(QVariantList channelReverses READ channelReverses NOTIFY channelReversesChanged)

    void setAllChannelValues    (std::array<qint16,16> channels);
    void pullChannelMappings    ();
    void pullChannelReverse     ();

    QVariantList channels       ()  {return _channels;}
    QVariantList channelMappings()  {return _channelMappings;}
    QVariantList channelReverses()  {return _channelReverses;}

    Q_INVOKABLE void setChannelReverse(int channel, bool reverse);
    void setChannelReverseThread(int channel, bool reverse);

    Q_INVOKABLE void shutdown_sdk();

signals:
    void channelsChanged();
    void channelMappingsChanged();
    void channelReversesChanged();

private slots:

private:
    unircsdk::UniRcSdk  _siyiSdk;
    unircsdk::RcuSession _rcu;
    QVariantList _channels;
    QVariantList _channelMappings;
    QVariantList _channelReverses;

};

#endif
