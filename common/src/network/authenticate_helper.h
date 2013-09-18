#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <QtCore/QMap>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"
#include "utils/network/http/httptypes.h"


class QnAuthHelper: public QObject
{
    Q_OBJECT

public:
    QnAuthHelper();
    virtual ~QnAuthHelper();

    static void initStaticInstance(QnAuthHelper* instance);
    static QnAuthHelper* instance();
    
    bool authenticate(const nx_http::HttpRequest& request, nx_http::HttpResponse& response);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);

private:
    void addAuthHeader(nx_http::HttpResponse& responseHeaders);
    QByteArray getNonce();
    bool isNonceValid(const QByteArray& nonce) const;
    bool doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::HttpResponse& responseHeaders);
    bool doBasicAuth(const QByteArray& authData, nx_http::HttpResponse& responseHeaders);
    bool doCustomAuthorization(const QByteArray& authData, nx_http::HttpResponse& response, const QByteArray& sesionKey);

private:
    QMutex m_mutex;
    static QnAuthHelper* m_instance;
    //QMap<QByteArray, QElapsedTimer> m_nonces;
    QMap<QnId, QnUserResourcePtr> m_users;
};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
