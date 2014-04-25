#ifndef droid_device_server_h_1755
#define droid_device_server_h_1755

#ifdef ENABLE_DROID

#include <QtCore/QList>

#include "core/resource_management/resource_searcher.h"
#include "utils/network/nettools.h"
#include "droid_controlport_listener.h"

class QnPlDroidResourceSearcher : public QnAbstractNetworkResourceSearcher
{

public:
    QnPlDroidResourceSearcher();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
private:
    QList<QSharedPointer<AbstractDatagramSocket> > m_socketList;
    qint64 m_lastReadSocketTime;
};

#endif // #ifdef ENABLE_DROID
#endif // droid_device_server_h_1755
