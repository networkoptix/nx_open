#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/resource/resource_fwd.h>

#include <nx/streaming/abstract_data_consumer.h>

class QnResourceCommandProcessor : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    QnResourceCommandProcessor();
    ~QnResourceCommandProcessor();

    bool hasCommandsForResource(const QnResourcePtr& resource) const;

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

};

#endif // ENABLE_DATA_PROVIDERS
