// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/abstract_data_packet.h>
#include <nx/utils/lockable.h>
#include <nx/utils/thread/long_runnable.h>
#include <utils/common/from_this_to_shared.h>

class QnAbstractStreamDataProvider;
class QnResource;
class QnAbstractMediaDataReceptor;

#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10)

class NX_VMS_COMMON_API QnAbstractStreamDataProvider:
    public QnLongRunnable
{
    Q_OBJECT
public:
    explicit QnAbstractStreamDataProvider(const QnResourcePtr& resource);
    virtual ~QnAbstractStreamDataProvider() override;

    virtual bool dataCanBeAccepted() const;

    int processorsCount() const;
    void addDataProcessor(QnAbstractMediaDataReceptor* dp);
    void removeDataProcessor(QnAbstractMediaDataReceptor* dp);

    virtual void subscribe(QnAbstractMediaDataReceptor* dp) { addDataProcessor(dp); }
    virtual void unsubscribe(QnAbstractMediaDataReceptor* dp) { removeDataProcessor(dp); }
    const QnResourcePtr& getResource() const;

    virtual bool isReverseMode() const { return false;}

    void setNeedSleep(bool sleep);

    double getSpeed() const;

    /* One resource may have several providers used with different roles*/
    virtual void setRole(Qn::ConnectionRole role);
    Qn::ConnectionRole getRole() const;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const { return QnConstResourceVideoLayoutPtr(); }
    virtual bool hasVideo() const { return true; }
    virtual bool needConfigureProvider() const;
    virtual void startIfNotRunning() { start(); }

signals:
    void videoLayoutChanged();

protected:
    virtual void putData(const QnAbstractDataPacketPtr& data);

protected:
    nx::Lockable<QList<QnAbstractMediaDataReceptor*>> m_dataprocessors;
    QnResourcePtr m_resource;

private:
    std::atomic<Qn::ConnectionRole> m_role;
};

using QnAbstractStreamDataProviderPtr = QSharedPointer<QnAbstractStreamDataProvider>;
