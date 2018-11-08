#ifndef resource_consumer_h_1921
#define resource_consumer_h_1921

#include <core/resource/resource_fwd.h>

class QnResourceConsumer
{
public:
    explicit QnResourceConsumer(const QnResourcePtr& resource);
    virtual ~QnResourceConsumer();

    const QnResourcePtr &getResource() const;

    bool isConnectedToTheResource() const;

    virtual void beforeDisconnectFromResource() {}
    virtual void disconnectFromResource();

protected:
    QnResourcePtr m_resource;
};

#endif // resource_consumer_h_1921
