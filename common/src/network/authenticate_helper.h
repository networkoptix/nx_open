#ifndef __QN_AUTH_HELPER__
#define __QN_AUTH_HELPER__

#include <QMap>
#include <core/resource/resource_fwd.h>
#include "utils/common/id.h"

class QnAuthHelper: public QObject
{
    Q_OBJECT

public:
    QnAuthHelper();
    virtual ~QnAuthHelper();

    static void initStaticInstance(QnAuthHelper* instance);
    static QnAuthHelper* instance();
    
    bool authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders);
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);
private:
    void removeOldNonces();
    void addAuthHeader(QHttpResponseHeader& responseHeaders);
    QByteArray getNonce();
    bool doDigestAuth(const QByteArray& method, const QByteArray& authData, QHttpResponseHeader& responseHeaders);
    bool doBasicAuth(const QByteArray& authData, QHttpResponseHeader& responseHeaders);
private:
    QMutex m_mutex;
    static QnAuthHelper* m_instance;
    QMap<QByteArray, QElapsedTimer> m_nonces;
    QMap<QnId, QnUserResourcePtr> m_users;
};

#define qnAuthHelper QnAuthHelper::instance()

#endif // __QN_AUTH_HELPER__
