// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_auth_debug_info_watcher.h"

#include <chrono>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <nx/cloud/db/api/oauth_data.h>
#include <nx/cloud/db/api/oauth_manager.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/jose/jws.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/html/html.h>
#include <ui/workbench/workbench_context.h>

namespace {

static const QString kDigestDebugInfoKey = "userDigest";
static const QString kTokenDebugInfoKey = "userToken";
static const QString kElideString = "...";
static constexpr int kMaxTokenLength = 50;

} // namespace

namespace nx::vms::client::desktop {

QStringList getFormatJWTInfo(const nx::network::jws::Token<nx::cloud::db::api::ClaimSet>& token)
{
    using namespace std::chrono_literals;
    static const QString kTemplate("&nbsp;&nbsp;%1: %2");
    QStringList result;
    const auto filter = DebugInfoStorage::instance()->filter();
    if (filter.contains(DebugInfoStorage::Field::all))
        return result;

    if (!filter.contains(DebugInfoStorage::Field::typ_header))
        result << kTemplate.arg("typHeader", QString::fromStdString(token.header.typ));
    if (!filter.contains(DebugInfoStorage::Field::alg))
        result << kTemplate.arg("alg", QString::fromStdString(token.header.alg));
    if (!filter.contains(DebugInfoStorage::Field::kid))
        result << kTemplate.arg("kid", QString::fromStdString(token.header.kid.value_or("")));
    if (!filter.contains(DebugInfoStorage::Field::exp))
    {
        result << kTemplate.arg("exp",
            QDateTime::fromSecsSinceEpoch(token.payload.exp().value_or(0s).count())
                .toString("dd/MM/yyyy hh:mm:ss"));
    }
    if (!filter.contains(DebugInfoStorage::Field::pwd_time))
    {
        result << kTemplate.arg("pwdTime",
            QDateTime::fromSecsSinceEpoch(token.payload.passwordTime().value_or(0s).count())
                .toString("dd/MM/yyyy hh:mm:ss"));
    }
    if (!filter.contains(DebugInfoStorage::Field::sid))
        result << kTemplate.arg("sid", QString::fromStdString(token.payload.sid().value_or("")));
    if (!filter.contains(DebugInfoStorage::Field::typ_payload))
    {
        result << kTemplate.arg(
            "typPayload", QString::fromStdString(token.payload.typ().value_or("")));
    }
    if (!filter.contains(DebugInfoStorage::Field::aud))
        result << kTemplate.arg("aud", QString::fromStdString(token.payload.aud().value_or("")));
    if (!filter.contains(DebugInfoStorage::Field::iat))
    {
        result << kTemplate.arg("iat",
            QDateTime::fromSecsSinceEpoch(token.payload.iat().value_or(0s).count())
                .toString("dd/MM/yyyy hh:mm:ss"));
    }
    if (!filter.contains(DebugInfoStorage::Field::sub))
        result << kTemplate.arg("sub", QString::fromStdString(token.payload.sub().value_or("")));
    if (!filter.contains(DebugInfoStorage::Field::client_id))
        result << kTemplate.arg(
            "client_id", QString::fromStdString(token.payload.clientId().value_or("")));
    if (!filter.contains(DebugInfoStorage::Field::iss))
        result << kTemplate.arg("iss", QString::fromStdString(token.payload.iss().value_or("")));

    return result;
}

void elideIfNeeded(QString& str)
{
    if (str.length() < kMaxTokenLength)
        return;

    const int partLength = (kMaxTokenLength - kElideString.length()) / 2;
    str = str.first(partLength) + kElideString + str.last(partLength);
}
class UserAuthDebugInfoWatcher::Private
{
    UserAuthDebugInfoWatcher* q;

public:
    Private(UserAuthDebugInfoWatcher* parent): q(parent) {}

    void updateSessionDebugInfo()
    {
        using namespace nx::vms::client::core;

        const auto filter = DebugInfoStorage::instance()->filter();
        if (filter.contains(DebugInfoStorage::Field::token))
            return;

        const auto updateTokenInfoForDebugInfo =
            [this]()
            {
                const auto session = qnClientCoreModule->networkModule()->session();
                const auto token = session->connection()->credentials().authToken;
                if (token.isBearerToken())
                {
                    QString displayToken;
                    const auto encoded =
                        std::string_view(token.value)
                            .substr(nx::cloud::db::api::OauthManager::kTokenPrefix.size());
                    if (const auto jwt =
                            nx::network::jws::decodeToken<nx::cloud::db::api::ClaimSet>(encoded))
                    {
                        const auto tokenInfo = getFormatJWTInfo(jwt.value());
                        displayToken = nx::vms::common::html::kLineBreak
                            + tokenInfo.join(nx::vms::common::html::kLineBreak);
                    }
                    else
                    {
                        displayToken = QString::fromStdString(token.value);
                        elideIfNeeded(displayToken);
                    }

                    DebugInfoStorage::instance()->setValue(
                        kTokenDebugInfoKey, "Token: " + displayToken);
                }
                else
                {
                    DebugInfoStorage::instance()->removeValue(kTokenDebugInfoKey);
                }
            };

        // When a new session appears, this old connection will be destroyed.
        connect(qnClientCoreModule->networkModule()->session().get(),
            &RemoteSession::credentialsChanged,
            q,
            updateTokenInfoForDebugInfo);

        updateTokenInfoForDebugInfo();
    }

public:
    QnUserResourcePtr currentUser;
};

UserAuthDebugInfoWatcher::UserAuthDebugInfoWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    connect(context(),
        &QnWorkbenchContext::userChanged,
        this,
        [this](const QnUserResourcePtr& user)
        {
            if (d->currentUser)
                d->currentUser->disconnect(this);

            d->currentUser = user;

            if (!user)
            {
                DebugInfoStorage::instance()->removeValue(kDigestDebugInfoKey);
                DebugInfoStorage::instance()->removeValue(kTokenDebugInfoKey);
                return;
            }

            const auto filter = DebugInfoStorage::instance()->filter();
            if (filter.contains(DebugInfoStorage::Field::digest))
                return;

            const auto updateDigestInfoForDebugInfo =
                [user]()
                {
                    const QString digestStatus = user->shouldDigestAuthBeUsed() ? "on" : "off";
                    DebugInfoStorage::instance()->setValue(
                        kDigestDebugInfoKey, "Digest: " + digestStatus);
                };

            connect(
                user.get(), &QnUserResource::digestChanged, this, updateDigestInfoForDebugInfo);

            updateDigestInfoForDebugInfo();

            d->updateSessionDebugInfo();
        });
}

UserAuthDebugInfoWatcher::~UserAuthDebugInfoWatcher()
{
}

} // namespace nx::vms::client::desktop
