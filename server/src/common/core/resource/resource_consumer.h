#ifndef resource_consumer_h_1921
#define resource_consumer_h_1921

class QnResource;
typedef QSharedPointer<QnResource> QnResourcePtr;
typedef QList<QnResourcePtr> QnResourceList;

class QnResourceConsumer
{
public:
    QnResourceConsumer(QnResourcePtr resource);
    virtual ~QnResourceConsumer();

    QnResourcePtr getResource() const;

    bool isConnectedToTheResource() const;

    virtual void beforeDisconnectFromResource();
    virtual void disconnectFromResource();

protected:
    QnResourcePtr m_resource;
};

#endif //resource_consumer_h_1921