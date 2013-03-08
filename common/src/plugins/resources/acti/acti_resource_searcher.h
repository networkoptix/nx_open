#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#include "plugins/resources/bonjour/bonjour_resource_searcher.h"


class QnActiResourceSearcher : public QnBonjourResourceSearcher
{
    QnActiResourceSearcher();

public:
    static QnActiResourceSearcher& instance();
    
    virtual ~QnActiResourceSearcher();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    virtual void processPacket(const QHostAddress& discoveryAddr, const QString& host, const BonjurDeviceInfo& devInfo, QnResourceList& result) override;
};

#endif // _ACTI_RESOURCE_SEARCHER_H__
