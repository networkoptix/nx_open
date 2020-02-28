#include "push_manager.h"

#include <QtCore/QJsonObject>

#include <api/global_settings.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>
#include <nx_vms_server_ini.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/switch.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/level.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/utils/app_info.h>
#include <translation/translation_manager.h>

namespace nx::vms::server::event {

static const QString kPushApiPath = "api/notifications/push_notification";
static const size_t kPushThumbnailHeight = 480;

struct PushPayload
{
    QString url;
    QString imageUrl;
};
#define PushPayload_Fields (url)(imageUrl)

struct PushNotification
{
    QString title;
    QString body;
    PushPayload payload;
    QJsonObject options;
};
#define PushNotification_Fields (title)(body)(payload)(options)

struct PushRequest
{
    QString systemId;
    std::set<QString> targets;
    PushNotification notification;
};
#define PushRequest_Fields (systemId)(targets)(notification)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PushPayload)(PushNotification)(PushRequest), (json), _Fields);

static const QString utf8Icon(const vms::event::Level level)
{
    static const auto defaultIcon = ini().pushNotifyCommonUtfIcon
        ? QString(QChar(0x2615)) //< UTF for ":)".
        : QString();
    return nx::utils::switch_(level,
        vms::event::Level::critical, [] { return QString(QChar(0x203C)); }, //< UTF for "!!".
        vms::event::Level::important, [] { return QString(QChar(0x26A0)); }, //< UTF for "(!)".
        vms::event::Level::success, [] { return QString(QChar(0x2705)); }, //< UTF for "[v]".
        nx::utils::default_, [] { return defaultIcon; }
    );
}

// -----------------------------------------------------------------------------------------------

PushManager::PushManager(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

bool PushManager::send(const vms::event::AbstractActionPtr& action)
{
    PushRequest request;
    const auto settings = serverModule()->commonModule()->globalSettings();
    request.systemId = settings->cloudSystemId();
    if (request.systemId.isEmpty())
    {
        NX_DEBUG(this, "Not sending notification, system is not connected to cloud");
        return true;
    }

    request.targets = cloudUsers(action->getParams().additionalResources);
    if (request.targets.empty())
    {
        NX_DEBUG(this, "Not sending notification, no targets found");
        return true;
    }

    request.notification = makeNotification(action);
    NX_VERBOSE(this, "Push Notification: %1\n%2",
        request.notification.title, request.notification.body);

    const auto url = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setHost(settings->cloudHost())
        .setPath(kPushApiPath);

    nx::network::http::HttpClient client;
    client.setUserName(settings->cloudSystemId());
    client.setUserPassword(settings->cloudAuthKey());
    client.setAuthType(nx::network::http::AuthType::authBasic);

    const auto serialized = QJson::serialized(request);
    const auto result = client.doPost(url, "application/json", serialized)
        && client.hasRequestSucceeded();

    NX_UTILS_LOG(
        result ? nx::utils::log::Level::debug : nx::utils::log::Level::warning,
        this,
        "Send notification to %1: %2 -- %3", url, serialized, client.response()
            ? client.response()->toString()
            : SystemError::toString(client.lastSysErrorCode()));

    return result;
}

PushPayload PushManager::makePayload(const vms::event::EventParameters& event, bool isCamera) const
{
    const auto settings = serverModule()->commonModule()->globalSettings();
    nx::network::url::Builder url = nx::network::url::Builder()
        .setScheme(nx::vms::utils::AppInfo::nativeUriProtocol())
        .setHost(settings->cloudHost())
        .setPathParts("client", settings->cloudSystemId(), "view")
        .addQueryItem("timestamp", event.eventTimestampUsec / 1000);

    nx::network::url::Builder imageUrl;
    if (isCamera)
    {
        url.addQueryItem("resources", event.eventResourceId.toSimpleString());
        imageUrl = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setHost(settings->cloudSystemId())
            .setPath("ec2/cameraThumbnail")
            .addQueryItem("cameraId", event.eventResourceId.toSimpleString())
            .addQueryItem("time", event.eventTimestampUsec / 1000)
            .addQueryItem("height", kPushThumbnailHeight);
    }

    const QString imageUrlOverride(ini().pushNotifyImageUrl);
    return {url.toString(), imageUrlOverride.isEmpty() ? imageUrl.toString() : imageUrlOverride};
}

PushNotification PushManager::makeNotification(const vms::event::AbstractActionPtr& action) const
{
    const auto params = action->getParams();
    const auto event = action->getRuntimeParams();
    const auto common = serverModule()->commonModule();
    const auto resource = resourcePool()->getResourceById(event.eventResourceId);
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    QJsonObject options;
    NX_ASSERT(QJson::deserialize(QByteArray(ini().pushNotifyOptions), &options));

    const auto language = common->globalSettings()->pushNotificationsLanguage();
    NX_VERBOSE(this, "Translate notification to %1", language);
    QnTranslationManager::LocaleRollback localeGuard(
        serverModule()->findInstance<QnTranslationManager>(), language);

    const auto join =
        [](const QString& delimiter, QStringList items)
        {
            items.removeAll("");
            return items.join(delimiter);
        };

    vms::event::StringsHelper strings(common);
    return {
        join(" ", {
            utf8Icon(vms::event::levelOf(action)),
            strings.notificationCaption(event, camera, /*useHtml*/ false),
        }),
        join("\n", {
            camera ? camera->getUserDefinedName() : (resource ? resource->getName() : QString()),
            params.text,
            strings.notificationDescription(event),
        }),
        makePayload(event, camera),
        options,
    };
}

std::set<QString> PushManager::cloudUsers(std::vector<QnUuid> filter) const
{
    QnUserResourceList userList;
    if (filter.empty())
    {
        userList = serverModule()->resourcePool()->getResources<QnUserResource>();
    }
    else
    {
        QList<QnUuid> userRoles;
        userRolesManager()->usersAndRoles(filter, userList, userRoles);
        for (const auto& role: userRoles)
        {
            for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(role))
                userList.push_back(subject.user());
        }
    }

    std::set<QString> names;
    for (const auto& user: userList)
    {
        if (user->isEnabled() && user->isCloud())
            names.insert(user->getName());
    }

    return names;
}

} // namespace nx::vms::server::event
