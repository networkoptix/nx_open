/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_AUTH_GENERIC_USER_DATA_PROVIDER_H
#define NX_AUTH_GENERIC_USER_DATA_PROVIDER_H

#include <QtCore/QObject>

#include "abstract_user_data_provider.h"
#include <nx/utils/safe_direct_connection.h>
#include <common/common_module_aware.h>


class GenericUserDataProvider
:
    public QObject,
    public QnCommonModuleAware,
    public AbstractUserDataProvider,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    GenericUserDataProvider(QnCommonModule* commonModule);
    ~GenericUserDataProvider();

    virtual QnResourcePtr findResByName(const QByteArray& nxUserName) const override;
    virtual Qn::AuthResult authorize(
        const QnResourcePtr& res,
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) override;
    virtual std::tuple<Qn::AuthResult, QnResourcePtr> authorize(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authorizationHeader,
        nx_http::HttpHeaders* const responseHeaders) override;

private:
    mutable QnMutex m_mutex;
    QMap<QnUuid, QnUserResourcePtr> m_users;
    QMap<QnUuid, QnMediaServerResourcePtr> m_servers;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr& res);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& res);
};

#endif  //NX_AUTH_GENERIC_USER_DATA_PROVIDER_H
