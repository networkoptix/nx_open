// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webengine_profile_manager.h"

#include <QtCore/QHash>
#include <QtNetwork/QNetworkProxy>
#include <QtQml/QtQml>
#include <QtWebEngineQuick/private/qquickwebenginedownloadrequest_p.h>

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_web_page_workarounds.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_proxy_server.h>

namespace nx::vms::client::desktop::utils {

struct WebEngineProfileManager::Private
{
    WebEngineProfileManager* const parent;
    QHash<QString, QPointer<QQuickWebEngineProfile>> profiles;

    Private(WebEngineProfileManager* parent): parent(parent) {}

    QQuickWebEngineProfile* getOrCreateProfile(
        const QString& name,
        bool offTheRecord,
        const QUrl& proxy = QUrl());
};

WebEngineProfileManager::WebEngineProfileManager():
    base_type(),
    d(new Private(this))
{
}

WebEngineProfileManager::~WebEngineProfileManager()
{}

QQuickWebEngineProfile* WebEngineProfileManager::Private::getOrCreateProfile(
    const QString& name,
    bool offTheRecord,
    const QUrl& proxy)
{
    // Suffix allows to distinguish between offTheRecord and non-offTheRecord profiles in cache.
    const auto suffix = offTheRecord ? "_off" : "_nx";

    QPointer<QQuickWebEngineProfile>& profile = profiles[name + suffix];
    if (!profile)
    {
        profile = new QQuickWebEngineProfile(parent);

        // Ensure the profile has a QML context needed for construction of script collection value.
        if (!qmlContext(profile))
            QQmlEngine::setContextForObject(profile, qmlContext(parent));

        profile->setOffTheRecord(offTheRecord);

        if (!proxy.isEmpty())
            profile->setProxy(proxy);

        profile->setStorageName(name);

        // Web pages are re-created even on layout tab switch,
        // so profile should save as much data as possible.
        profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);

        QObject::connect(profile, &QQuickWebEngineProfile::downloadRequested,
            parent, [this](QQuickWebEngineDownloadRequest* download)
            {
                parent->downloadRequested(download);
            });
    }
    return profile.data();
}

Q_INVOKABLE QQuickWebEngineProfile* WebEngineProfileManager::getProfile(
    const QString& name, bool offTheRecord, const QString& resourceId)
{
    nx::Uuid parentId;

    if (!resourceId.isEmpty())
    {
        // If we can find a parent of provided resource then we should setup a proxy.

        const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
        const auto resourceUuid = nx::Uuid::fromStringSafe(resourceId);

        if (const auto resource = resourcePool->getResourceById(resourceUuid))
            parentId = resource->getParentId();
    }

    if (parentId.isNull())
        return d->getOrCreateProfile(name, offTheRecord);

    const auto proxyAddress = LocalProxyServer::instance()->address();

    QUrl proxyUrl;

    proxyUrl.setScheme("socks5");
    proxyUrl.setHost(proxyAddress.address.toString().c_str());
    proxyUrl.setPort(proxyAddress.port);
    proxyUrl.setUserName(resourceId);
    proxyUrl.setPassword(QString::fromStdString(LocalProxyServer::instance()->password()));

    return d->getOrCreateProfile(name + "_" + resourceId, offTheRecord, proxyUrl);
}

WebEngineProfileManager* WebEngineProfileManager::instance()
{
    static WebEngineProfileManager manager;
    return &manager;
}

void WebEngineProfileManager::registerQmlType()
{
    qmlRegisterSingletonType<WebEngineProfileManager>(
        "nx.vms.client.desktop.utils", 1, 0, "WebEngineProfileManager",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            auto manager = instance();
            qmlEngine->setObjectOwnership(manager, QQmlEngine::CppOwnership);
            return manager;
        });
}

} // namespace nx::vms::client::desktop::utils
