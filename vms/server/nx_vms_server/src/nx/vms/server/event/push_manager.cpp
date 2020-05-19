#include "push_manager.h"

#include <api/global_settings.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>
#include <nx_vms_server_ini.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/switch.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/level.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/utils/app_info.h>
#include <translation/translation_manager.h>

namespace nx::vms::server::event {

using namespace nx::network;

static const QString kPushApiPath = "api/notifications/push_notification";
static const size_t kPushThumbnailHeight = 480;

const RetryPolicy PushManager::kDefaultRetryPolicy(
    /*maxRetryCount*/ 5,
    /*initialDelay*/ std::chrono::seconds(1),
    /*delayMultiplier*/ 7,
    /*maxDelay*/ std::chrono::hours(1),
    /*randomRatio*/ 0);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PushPayload)(PushNotification)(PushRequest), (json), _Fields);

static const QString utf8Icon(const vms::event::Level level)
{
    static const auto commonIcon = ini().pushNotifyCommonUtfIcon;
    return nx::utils::switch_(level,
        vms::event::Level::critical, [] { return QString(QChar(0x274C)); }, //< UTF for "X".
        vms::event::Level::important, [] { return QString(QChar(0x26A0)); }, //< UTF for "(!)".
        vms::event::Level::success, [] { return QString(QChar(0x2705)); }, //< UTF for "[v]".
        nx::utils::default_, [] { return commonIcon ? QString(QChar(commonIcon)) : QString(); }
    );
}

// -----------------------------------------------------------------------------------------------

class PushManager::Pipeline: public aio::BasicPollable
{
public:
    Pipeline(QnGlobalSettings* settings, bool useEncryption):
        m_settings(settings), m_useEncryption(useEncryption)
    {
    }

    void send(const nx::Buffer& request, nx::utils::MoveOnlyFunc<void(bool)> handler)
    {
        dispatch(
            [this, request = &request, handler = std::move(handler)]() mutable
            {
                m_queue.emplace(request, std::move(handler));
                if (!m_client)
                    sendNextInQueue();
            });
    }

    void stopWhileInAioThread() override
    {
        m_client.reset();
    }

private:
    void sendNextInQueue()
    {
        if (m_queue.empty())
            return m_client.reset();

        auto request = std::move(m_queue.front().first);
        auto handler = std::move(m_queue.front().second);
        m_queue.pop();

        const nx::utils::Url url = url::Builder()
            .setScheme(http::urlSheme(m_useEncryption))
            .setEndpoint(m_settings->cloudHost())
            .setPath(kPushApiPath);

        if (!m_client)
        {
            m_client = std::make_unique<http::AsyncClient>();
            m_client->bindToAioThread(getAioThread());
            m_client->setUserName(m_settings->cloudSystemId());
            m_client->setUserPassword(m_settings->cloudAuthKey());
            m_client->setAuthType(nx::network::http::AuthType::authBasic);
        }

        m_client->setRequestBody(std::make_unique<http::BufferSource>(
            "application/json", *request));
        m_client->doPost(url,
            [this, url, request, handler = std::move(handler)]() mutable
            {
                NX_DEBUG(
                    this, "Send to %1: %2 -- %3", m_client->url(), *request, m_client->response()
                        ? m_client->response()->statusLine.toString().trimmed()
                        : SystemError::toString(m_client->lastSysErrorCode()));

                handler(m_client->hasRequestSucceeded());
                if (!m_queue.empty())
                    sendNextInQueue();
                else
                    m_client.reset();
            });
    }

private:
    QnGlobalSettings* const m_settings = nullptr;
    const bool m_useEncryption = true;
    std::queue<std::pair<const nx::Buffer*, nx::utils::MoveOnlyFunc<void(bool)>>> m_queue;
    std::unique_ptr<nx::network::http::AsyncClient> m_client;
};

// -----------------------------------------------------------------------------------------------

class PushManager::Dispatcher
{
public:
    Dispatcher(PushManager::Pipeline& pipeline, RetryPolicy retryPolicy):
        m_pipeline(pipeline),
        m_timer(retryPolicy, pipeline.getAioThread())
    {
    }

    void start(nx::Buffer request, nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_request = std::move(request);
        m_handler = std::move(handler);
        dispatch();
    }

private:
    void dispatch()
    {
        m_pipeline.send(
            m_request,
            [this](bool isOk)
            {
                if (isOk)
                    return nx::utils::swapAndCall(m_handler);

                if (!m_timer.scheduleNextTry([this](){ dispatch(); }))
                {
                    NX_WARNING(this, "Request is dropped after several failures: %1", m_request);
                    return nx::utils::swapAndCall(m_handler);
                }

                NX_VERBOSE(this, "Retry is scheduled in %1: %2", m_timer.timeToEvent(), m_request);
            });
    }

private:
    PushManager::Pipeline& m_pipeline;
    RetryTimer m_timer;
    nx::Buffer m_request;
    nx::utils::MoveOnlyFunc<void()> m_handler;
};

// -----------------------------------------------------------------------------------------------

PushManager::PushManager(
    QnMediaServerModule* serverModule, RetryPolicy retryPolicy, bool useEncryption)
:
    nx::vms::server::ServerModuleAware(serverModule),
    m_retryPolicy(retryPolicy),
    m_pipeline(
        std::make_unique<Pipeline>(serverModule->commonModule()->globalSettings(), useEncryption))
{
}

PushManager::~PushManager()
{
    std::promise<void> promise;
    m_pipeline->pleaseStop(
        [this, &promise]
        {
            m_dispatchers.clear();
            promise.set_value();
        });

    promise.get_future().wait();
}

bool PushManager::send(const vms::event::AbstractActionPtr& action)
{
    const auto settings = serverModule()->commonModule()->globalSettings();
    auto systemId = settings->cloudSystemId();
    if (systemId.isEmpty())
    {
        NX_DEBUG(this, "Not sending notification, system is not connected to cloud");
        return false;
    }

    auto targets = cloudUsers(action);
    if (targets.empty())
    {
        NX_DEBUG(this, "Not sending notification, no targets found");
        return false;
    }

    PushRequest request{std::move(systemId), std::move(targets), makeNotification(action)};
    m_pipeline->post(
        [this, request = std::move(request)]() mutable
        {
            m_dispatchers.push_front(std::make_unique<Dispatcher>(*m_pipeline, m_retryPolicy));
            const auto it = m_dispatchers.begin();
            (*it)->start(QJson::serialized(request), [this, it] { m_dispatchers.erase(it); });
        });

    return true; //< TODO: Find out the way to report actual result async.
}

PushPayload PushManager::makePayload(const vms::event::EventParameters& event, bool isCamera) const
{
    const auto settings = serverModule()->commonModule()->globalSettings();
    url::Builder url = url::Builder()
        .setScheme(nx::vms::utils::AppInfo::nativeUriProtocol())
        .setEndpoint(settings->cloudHost())
        .setPathParts("client", settings->cloudSystemId(), "view")
        .addQueryItem("timestamp", event.eventTimestampUsec / 1000);

    url::Builder imageUrl;
    if (isCamera)
    {
        url.addQueryItem("resources", event.eventResourceId.toSimpleString());
        imageUrl = url::Builder()
            .setScheme(http::kSecureUrlSchemeName)
            .setEndpoint(settings->cloudSystemId())
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

    const auto language = common->globalSettings()->pushNotificationsLanguage();
    NX_VERBOSE(this, "Translate notification to %1", language.isEmpty() ? "NONE" : language);
    QnTranslationManager::LocaleRollback localeGuard(
        serverModule()->findInstance<QnTranslationManager>(), language);

    vms::event::StringsHelper strings(common);
    const auto caption = params.sayText.isEmpty() //< Used to store custom caption text.
        ? strings.notificationCaption(event, camera, /*useHtml*/ false)
        : params.sayText;
    const auto description = params.text.isEmpty()
        ? strings.notificationDescription(event)
        : params.text;

    const auto icon = utf8Icon(vms::event::levelOf(action));

    const auto resourceName = camera
        ? camera->getUserDefinedName()
        : (resource ? resource->getName() : QString());
    const auto resourceText = params.useSource
        ? (resourceName.isEmpty() ? QString() : ("[" + resourceName + "]"))
        : QString();
    const bool addNewLine = !description.isEmpty() && !resourceText.isEmpty();

    QJsonObject options;
    NX_ASSERT(QJson::deserialize(QByteArray(ini().pushNotifyOptions), &options));

    return {
        (icon.isEmpty() ? QString() : (icon + " ")) + caption,
        description + (addNewLine ? "\n" : "") + resourceText,
        makePayload(event, camera),
        options,
    };
}

std::set<QString> PushManager::cloudUsers(const vms::event::AbstractActionPtr& action) const
{
    const auto actionParams = action->getParams();
    QnUserResourceList userList;
    if (actionParams.allUsers)
    {
        userList = serverModule()->resourcePool()->getResources<QnUserResource>();
        NX_VERBOSE(this, "Looking for all cloud users in list of %1", userList.size());
    }
    else
    {
        QList<QnUuid> userRoles;
        userRolesManager()->usersAndRoles(actionParams.additionalResources, userList, userRoles);
        NX_VERBOSE(this, "Looking for cloud users %1 in list of %2 and roles of %3",
            containerString(actionParams.additionalResources), userList.size(), userRoles.size());

        for (const auto& role: userRoles)
        {
            for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(role))
                userList.push_back(subject.user());
        }
    }

    const auto eventParams = action->getRuntimeParams();
    std::set<QString> names;
    for (const auto& user: userList)
    {
        if (user->isEnabled() && user->isCloud() && vms::event::hasAccessToSource(eventParams, user))
            names.insert(user->getName());
    }

    return names;
}

} // namespace nx::vms::server::event
