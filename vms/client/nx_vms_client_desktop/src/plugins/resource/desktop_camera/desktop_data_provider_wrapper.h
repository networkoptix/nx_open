#pragma once

#include <QtCore/QtGlobal>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

class QnDesktopDataProviderBase;

/*
* This class used for sharing desktop media stream
*/

class QnDesktopDataProviderWrapper: public QnAbstractMediaStreamDataProvider, public QnAbstractDataConsumer
{
public:
    QnDesktopDataProviderWrapper(QnResourcePtr res, QnDesktopDataProviderBase* owner);
    virtual ~QnDesktopDataProviderWrapper();

    virtual void start(Priority priority = InheritPriority) override;
    bool isInitialized() const;
    QString lastErrorStr() const;
    virtual bool hasThread() const override { return false; }
    virtual bool needConfigureProvider() const override;
    const QnDesktopDataProviderBase* owner() const { return m_owner;  }
protected:
    virtual bool processData(const QnAbstractDataPacketPtr& /*data*/) override { return true; }

protected:
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    QSet<void*> m_needKeyData;
    QnDesktopDataProviderBase* m_owner;
};
