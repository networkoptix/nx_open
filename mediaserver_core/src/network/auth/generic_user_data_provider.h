#pragma once

#include <QtCore/QObject>

#include <nx/utils/safe_direct_connection.h>

#include <nx/vms/auth/abstract_user_data_provider.h>

#include <common/common_module_aware.h>

class GenericUserDataProvider:
    public QObject,
    public QnCommonModuleAware,
    public nx::vms::auth::AbstractUserDataProvider,
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
