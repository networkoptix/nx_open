#ifndef droid_device_server_h_1755
#define droid_device_server_h_1755

#include <QList>

#include "core/resourcemanagment/resource_searcher.h"
#include "utils/network/nettools.h"
#include "droid_controlport_listener.h"

class QnPlDroidResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDroidResourceSearcher();

public:
    static QnPlDroidResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QnResourcePtr checkHostAddr(QHostAddress addr, QAuthenticator auth);
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
private:
    QList<QSharedPointer<QUdpSocket> > m_socketList;
    qint64 m_lastReadSocketTime;
    QnDroidControlPortListener m_controlPortListener;
};

#endif // droid_device_server_h_1755
