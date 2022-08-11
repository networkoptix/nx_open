// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

namespace nx::vms::client::core { class DesktopDataProviderBase; }

namespace nx::vms::client::desktop {

/*
* This class used for sharing desktop media stream.
*/
class DesktopDataProviderWrapper:
    public QnAbstractMediaStreamDataProvider,
    public QnAbstractMediaDataReceptor
{
public:
    DesktopDataProviderWrapper(QnResourcePtr res, core::DesktopDataProviderBase* owner);
    virtual ~DesktopDataProviderWrapper();

    virtual void start(Priority priority = InheritPriority) override;
    bool isInitialized() const;
    QString lastErrorStr() const;
    virtual bool hasThread() const override { return false; }
    virtual bool canAcceptData() const override;
    virtual bool needConfigureProvider() const override;
    const nx::vms::client::core::DesktopDataProviderBase* owner() const { return m_owner;  }

protected:
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    QSet<void*> m_needKeyData;
    core::DesktopDataProviderBase* m_owner;
};

} // namespace nx::vms::client::desktop
