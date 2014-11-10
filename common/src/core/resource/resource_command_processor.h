#ifndef QN_RESOURCE_COMMAND_PROCESSOR
#define QN_RESOURCE_COMMAND_PROCESSOR

#ifdef ENABLE_DATA_PROVIDERS

#include "resource_fwd.h"

#include <core/dataconsumer/abstract_data_consumer.h>

class QN_EXPORT QnResourceCommandProcessor : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    QnResourceCommandProcessor();
    ~QnResourceCommandProcessor();

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

};

#endif // ENABLE_DATA_PROVIDERS

#endif //QN_RESOURCE_COMMAND_PROCESSOR
