#ifndef resource_consumer_h_1921
#define resource_consumer_h_1921
#include "qnresource.h"

class QN_EXPORT QnResourceConsumer
{
public:
    QnResourceConsumer(QnResource* resource);
    virtual ~QnResourceConsumer();

    QnResource* getResource() const;

    bool isConnectedToTheResource() const;

    virtual void beforeDisconnectFromResource() = 0;
    virtual void disconnectFromResource();

protected:
    QnResource* m_resource;
};

#endif //resource_consumer_h_1921