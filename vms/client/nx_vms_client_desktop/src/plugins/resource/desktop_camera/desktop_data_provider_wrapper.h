// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

class QnDesktopDataProviderBase;

/*
* This class used for sharing desktop media stream
*/

class QnDesktopDataProviderWrapper: public QnAbstractMediaStreamDataProvider, public QnAbstractMediaDataReceptor
{
public:
    QnDesktopDataProviderWrapper(QnResourcePtr res, QnDesktopDataProviderBase* owner);
    virtual ~QnDesktopDataProviderWrapper();

    virtual void start(Priority priority = InheritPriority) override;
    bool isInitialized() const;
    QString lastErrorStr() const;
    virtual bool hasThread() const override { return false; }
    virtual bool canAcceptData() const override;
    virtual bool needConfigureProvider() const override;
    void clearUnprocessedData() override {};
    const QnDesktopDataProviderBase* owner() const { return m_owner;  }

protected:
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    QSet<void*> m_needKeyData;
    QnDesktopDataProviderBase* m_owner;
};
