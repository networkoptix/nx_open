#include "generic_user_data_provider.h"

#include <QtCore/QCryptographicHash>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/app_info.h>

#include <nx/network/app_info.h>
#include <nx/utils/log/log.h>

GenericUserDataProvider::GenericUserDataProvider(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    Qn::directConnect(
        commonModule->resourcePool(), &QnResourcePool::resourceAdded,
        this, &GenericUserDataProvider::at_resourcePool_resourceAdded);
    Qn::directConnect(
        commonModule->resourcePool(), &QnResourcePool::resourceChanged,
        this, &GenericUserDataProvider::at_resourcePool_resourceAdded);
    Qn::directConnect(
        commonModule->resourcePool(), &QnResourcePool::resourceRemoved,
        this, &GenericUserDataProvider::at_resourcePool_resourceRemoved);
}

GenericUserDataProvider::~GenericUserDataProvider()
{
    directDisconnectAll();
}

QnResourcePtr GenericUserDataProvider::findResByName(const QByteArray& nxUserName) const
{
    QnMutexLocker lock(&m_mutex);
    const auto& lowerName = nxUserName.toLower();
    for (const QnUserResourcePtr& user: m_users)
    {
        if (user->getName().toUtf8().toLower() == lowerName)
            return user;
    }

    for (const QnMediaServerResourcePtr& server : m_servers)
    {
        if (server->getId() == QnUuid::fromStringSafe(nxUserName))
            return server;
    }

    NX_VERBOSE(this, lm("Unable to get user by name: %1").arg(nxUserName));
    return QnResourcePtr();
}

Qn::AuthResult GenericUserDataProvider::authorize(
    const QnResourcePtr& res,
    const nx::network::http::Method::ValueType& method,
    const nx::network::http::header::Authorization& authorizationHeader,
    nx::network::http::HttpHeaders* const /*responseHeaders*/)
{
    if (const auto user = res.dynamicCast<QnUserResource>(); user && !user->isEnabled())
    {
        NX_VERBOSE(this, "%1 is disabled", user);
        return Qn::Auth_DisabledUser;
    }

    if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::digest)
    {
        QByteArray ha1;
        auto user = res.dynamicCast<QnUserResource>();
        if (user)
        {
            ha1 = user->getDigest();
        }
        else if (auto server = res.dynamicCast<QnMediaServerResource>())
        {
            const QString ha1Data = lit("%1:%2:%3").arg(server->getId().toString()).arg(nx::network::AppInfo::realm()).arg(server->getAuthKey());
            ha1 = QCryptographicHash::hash(ha1Data.toUtf8(), QCryptographicHash::Md5).toHex();
        }

        QCryptographicHash ha2Hash(QCryptographicHash::Md5);
        ha2Hash.addData(method);
        ha2Hash.addData(":");
        ha2Hash.addData(authorizationHeader.digest->params["uri"]);
        const QByteArray ha2 = ha2Hash.result().toHex();

        QCryptographicHash responseHash(QCryptographicHash::Md5);
        responseHash.addData(ha1);
        responseHash.addData(":");
        responseHash.addData(authorizationHeader.digest->params["nonce"]);
        responseHash.addData(":");
        responseHash.addData(ha2);
        const QByteArray calcResponse = responseHash.result().toHex();

        if (calcResponse != authorizationHeader.digest->params["response"])
        {
            NX_VERBOSE(this, lm("Wrong digest for %1").args(user));
            return Qn::Auth_WrongPassword;
        }

        return Qn::Auth_OK;
    }
    else if (authorizationHeader.authScheme == nx::network::http::header::AuthScheme::basic)
    {
        if (auto user = res.dynamicCast<QnUserResource>())
        {
            if (user->checkLocalUserPassword(QString::fromUtf8(authorizationHeader.basic->password)))
                return Qn::Auth_OK;
        }
        else if (auto server = res.dynamicCast<QnMediaServerResource>())
        {
            if (server->getAuthKey() == QString::fromUtf8(authorizationHeader.basic->password))
            {
                NX_VERBOSE(this, lm("Authorized %1 by server key").args(server));
                return Qn::Auth_OK;
            }
        }

        NX_VERBOSE(this, lm("Wrong basic password for %1").args(res));
        return Qn::Auth_WrongPassword;
    }

    return Qn::Auth_Forbidden;
}

std::tuple<Qn::AuthResult, QnResourcePtr> GenericUserDataProvider::authorize(
    const nx::network::http::Method::ValueType& method,
    const nx::network::http::header::Authorization& authorizationHeader,
    nx::network::http::HttpHeaders* const responseHeaders)
{
    auto res = findResByName(authorizationHeader.userid());
    if (!res)
        return std::make_tuple(Qn::Auth_WrongLogin, QnResourcePtr());
    return std::make_tuple(
        authorize(res, method, authorizationHeader, responseHeaders),
        res);
}

void GenericUserDataProvider::at_resourcePool_resourceAdded(const QnResourcePtr& res)
{
    QnMutexLocker lock(&m_mutex);

    if (auto user = res.dynamicCast<QnUserResource>())
        m_users.insert(user->getId(), user);
    else if (auto server = res.dynamicCast<QnMediaServerResource>())
        m_servers.insert(server->getId(), server);
}

void GenericUserDataProvider::at_resourcePool_resourceRemoved(const QnResourcePtr& res)
{
    QnMutexLocker lock(&m_mutex);

    m_users.remove(res->getId());
    m_servers.remove(res->getId());
}
