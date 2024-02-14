// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_auth_debug_info_watcher.h"

#include <QtCore/QString>

#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <ui/workbench/workbench_context.h>

namespace {

static const QString kDigestDebugInfoKey = "userDigest";
static const QString kTokenDebugInfoKey = "userToken";
static const QString kElideString = "...";
static constexpr int kMaxTokenLength = 50;

} // namespace

namespace nx::vms::client::desktop {

class UserAuthDebugInfoWatcher::Private
{
    UserAuthDebugInfoWatcher* q;

public:
    Private(UserAuthDebugInfoWatcher* parent): q(parent) {}

    void updateSessionDebugInfo()
    {
        using namespace nx::vms::client::core;

        const auto updateTokenInfoForDebugInfo =
            []()
            {
                const auto session = qnClientCoreModule->networkModule()->session();
                const auto token = session->connection()->credentials().authToken;
                if (token.isBearerToken())
                {
                    QString displayToken = QString::fromStdString(token.value);
                    if (displayToken.length() > kMaxTokenLength)
                    {
                        const int partLength = (kMaxTokenLength - kElideString.length()) / 2;
                        displayToken = displayToken.first(partLength)
                            + kElideString
                            + displayToken.last(partLength);
                    }
                    DebugInfoStorage::instance()->setValue(kTokenDebugInfoKey,
                        "Token: " + displayToken);
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
