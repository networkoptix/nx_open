#ifndef droid_device_server_h_1755
#define droid_device_server_h_1755

#ifdef ENABLE_DROID

#include <QtCore/QList>

#include "core/resource_management/resource_searcher.h"
#include <nx/network/nettools.h>
#include <nx/network/socket.h>


class QnPlDroidResourceSearcher: public QnAbstractNetworkResourceSearcher
{

public:
    QnPlDroidResourceSearcher(QnCommonModule* commonModule);

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
private:
    QList<QSharedPointer<AbstractDatagramSocket> > m_socketList;
    qint64 m_lastReadSocketTime;
};

#endif // #ifdef ENABLE_DROID
#endif // droid_device_server_h_1755
