#ifndef resource_consumer_h_1921
#define resource_consumer_h_1921

#include "resource.h"

class QN_EXPORT QnResourceConsumer
{
public:
    QnResourceConsumer(QnResourcePtr resource);
    virtual ~QnResourceConsumer();

    const QnResourcePtr &getResource() const;

    bool isConnectedToTheResource() const;

    virtual void beforeDisconnectFromResource() = 0;
    virtual void disconnectFromResource();

protected:
    QnResourcePtr m_resource;
};

#endif // resource_consumer_h_1921
