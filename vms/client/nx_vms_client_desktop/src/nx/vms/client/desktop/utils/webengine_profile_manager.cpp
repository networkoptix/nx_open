#include "webengine_profile_manager.h"

#include <QtCore/QHash>
#include <QtQml/QtQml>
#include <QtWebEngine/private/qquickwebenginedownloaditem_p.h>

namespace nx::vms::client::desktop::utils {

struct WebEngineProfileManager::Private
{
    QHash<QString, QPointer<QQuickWebEngineProfile>> profiles;
};

WebEngineProfileManager::WebEngineProfileManager():
    base_type(),
    d(new Private)
{}

WebEngineProfileManager::~WebEngineProfileManager()
{}

QQuickWebEngineProfile* WebEngineProfileManager::getProfile(const QString& name)
{
    QPointer<QQuickWebEngineProfile>& profile = d->profiles[name];
    if (!profile)
    {
        profile = new QQuickWebEngineProfile(this);
        if (!name.isEmpty())
        {
            profile->setStorageName(name);
            profile->setOffTheRecord(false);
            // Web pages are re-created even on layout tab switch,
            // so profile shoud save as much data as possible.
            profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);

            connect(profile, &QQuickWebEngineProfile::downloadRequested,
                this, &WebEngineProfileManager::downloadRequested);
        }
        else
        {
            profile->setOffTheRecord(true);
        }
    }
    return profile.data();
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
