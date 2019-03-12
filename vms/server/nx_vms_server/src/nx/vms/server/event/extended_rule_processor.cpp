#include "extended_rule_processor.h"

#include <map>

#include <QtCore/QList>

#include <api/app_server_connection.h>

#include <nx/vms/event/actions/actions.h>
#include <nx/vms/event/events/ip_conflict_event.h>
#include <nx/vms/event/events/server_conflict_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/server/event/event_connector.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_history.h>

#include <core/resource/avi/avi_resource.h>

#include <database/server_db.h>

#include <camera/camera_pool.h>

#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <media_server/serverutil.h>

#include <nx/utils/log/log.h>
#include <camera/get_image_helper.h>

#include <QtConcurrent/QtConcurrent>
#include <utils/email/email.h>
#include <nx/utils/timer_manager.h>
#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <nx/email/mustache/mustache_helper.h>

#include <common/common_module.h>
#include <common/common_globals.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <utils/common/delayed.h>

#if !defined(EDGE_SERVER)
#include <providers/speech_synthesis_data_provider.h>
#endif

#include <providers/stored_file_data_provider.h>
#include <streaming/audio_streamer_pool.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include <nx/streaming/abstract_archive_stream_reader.h>

#include <utils/common/app_info.h>
#include <nx/utils/system_error.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>
#include <rest/handlers/multiserver_chunks_rest_handler.h>
#include <common/common_module.h>
#include <rest/handlers/multiserver_thumbnail_rest_handler.h>
#include <api/helpers/thumbnail_request_data.h>
#include <utils/common/util.h>
#include <nx/utils/concurrent.h>
#include <utils/camera/bookmark_helpers.h>
#include "nx/vms/server/resource/camera.h"
#include <media_server/media_server_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <api/helpers/camera_id_helper.h>

namespace {

static const QString tpProductLogo(lit("logo"));
static const QString tpSystemIcon(lit("systemIcon"));
static const QString tpOwnerIcon(lit("ownerIcon"));
static const QString tpSourceIcon(lit("sourceIcon"));
static const QString tpCloudOwner(lit("cloudOwner"));
static const QString tpCloudOwnerEmail(lit("cloudOwnerEmail"));
static const QString tpCompanyName(lit("companyName"));
static const QString tpCompanyUrl(lit("companyUrl"));
static const QString tpSupportLink(lit("supportLink"));
static const QString tpSupportLinkText(lit("supportLinkText"));
static const QString tpSystemName(lit("systemName"));
static const QString tpSystemSignature(lit("systemSignature"));
static const QString tpImageMimeType(lit("image/png"));
static const QString tpScreenshotFilename(lit("screenshot"));
static const QString tpScreenshotAttachmentName(lit("screenshot_%1.jpeg"));
static const QString tpScreenshotWithNumAttachmentName(lit("screenshot_%1_%2.jpeg"));

static const QString tpProductName(lit("productName"));
static const QString tpEvent(lit("event"));
static const QString tpSource(lit("source"));
static const QString tpSourceIP(lit("sourceIp"));

static const QString tpCameraName(lit("name"));
static const QString tpCameraIP(lit("cameraIP"));

static const QString tpUrlInt(lit("urlint"));
static const QString tpUrlExt(lit("urlext"));
static const QString tpCount(lit("count"));
static const QString tpIndex(lit("index"));
static const QString tpTimestamp(lit("timestamp"));
static const QString tpTimestampDate(lit("timestampDate"));
static const QString tpTimestampTime(lit("timestampTime"));
static const QString tpReason(lit("reason"));
static const QString tpReasonContext(lit("reasonContext"));
static const QString tpAggregated(lit("aggregated"));
static const QString tpInputPort(lit("inputPort"));
static const QString tpTriggerName(lit("triggerName"));
static const QString tpAnalyticsSdkEventType(lit("analyticsSdkEventType"));
static const QString tpHasCameras(lit("hasCameras"));
static const QString tpCameras(lit("cameras"));
static const QString tpUser(lit("user"));

static const QString tpCaption(lit("caption"));
static const QString tpDescription(lit("description"));

static const QSize SCREENSHOT_SIZE(640, 480);
static const unsigned int MS_PER_SEC = 1000;
static const std::chrono::seconds kDefaultMailAggregationPeriod(30);

static const int kEmailSendDelay = 0;

static const QChar kOldEmailDelimiter(L';');
static const QChar kNewEmailDelimiter(L' ');

} // namespace

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace nx {
namespace vms::server {
namespace event {

struct EmailAttachmentData
{
    EmailAttachmentData(EventType eventType)
    {
        switch (eventType)
        {
            case EventType::cameraMotionEvent:
                templatePath = lit(":/email_templates/camera_motion.mustache");
                imageName = lit("camera.png");
                break;
            case EventType::cameraInputEvent:
                templatePath = lit(":/email_templates/camera_input.mustache");
                imageName = lit("camera.png");
                break;
            case EventType::cameraDisconnectEvent:
                templatePath = lit(":/email_templates/camera_disconnect.mustache");
                imageName = lit("camera.png");
                break;
            case EventType::storageFailureEvent:
                templatePath = lit(":/email_templates/storage_failure.mustache");
                imageName = lit("storage.png");
                break;
            case EventType::networkIssueEvent:
                templatePath = lit(":/email_templates/network_issue.mustache");
                imageName = lit("server.png");
                break;
            case EventType::cameraIpConflictEvent:
                templatePath = lit(":/email_templates/camera_ip_conflict.mustache");
                imageName = lit("camera.png");
                break;
            case EventType::serverFailureEvent:
                templatePath = lit(":/email_templates/mediaserver_failure.mustache");
                imageName = lit("server.png");
                break;
            case EventType::serverConflictEvent:
                templatePath = lit(":/email_templates/mediaserver_conflict.mustache");
                imageName = lit("server.png");
                break;
            case EventType::serverStartEvent:
                templatePath = lit(":/email_templates/mediaserver_started.mustache");
                imageName = lit("server.png");
                break;
            case EventType::licenseIssueEvent:
                templatePath = lit(":/email_templates/license_issue.mustache");
                imageName = lit("license.png");
                break;
            case EventType::backupFinishedEvent:
                templatePath = lit(":/email_templates/backup_finished.mustache");
                imageName = lit("server.png");
                break;
            case EventType::analyticsSdkEvent:
                templatePath = lit(":/email_templates/analytics_event.mustache");
                imageName = lit("camera.png");
                break;
            case EventType::userDefinedEvent:
                templatePath = lit(":/email_templates/generic_event.mustache");
                imageName = lit("server.png");
                break;
            case EventType::softwareTriggerEvent:
                templatePath = lit(":/email_templates/software_trigger.mustache");
                break;
            default:
                NX_ASSERT(false, "All cases must be implemented.");
                break;
        }

        NX_ASSERT(!templatePath.isEmpty(), "Template path must be filled");
    }

    QString templatePath;
    QString imageName;
};

ExtendedRuleProcessor::ExtendedRuleProcessor(QnMediaServerModule* serverModule):
    base_type(serverModule),
    m_emailManager(new EmailManagerImpl(serverModule->commonModule()))
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &ExtendedRuleProcessor::onRemoveResource, Qt::QueuedConnection);
}

ExtendedRuleProcessor::~ExtendedRuleProcessor()
{
    quit();
    wait();
    m_emailThreadPool.waitForDone();

    std::set<quint64> timersToRemove;
    {
        QnMutexLocker lk(&m_mutex);
        for (const auto& value: m_aggregatedEmails)
        {
            if (value.periodicTaskID)
                timersToRemove.insert(value.periodicTaskID);
        }
        m_aggregatedEmails.clear();
    }

    for (const auto& timerId: timersToRemove)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(timerId);
}

void ExtendedRuleProcessor::onRemoveResource(const QnResourcePtr& resource)
{
    serverModule()->serverDb()->removeLogForRes(resource->getId());
}

void ExtendedRuleProcessor::prepareAdditionActionParams(const vms::event::AbstractActionPtr& action)
{
    switch (action->actionType())
    {
        case ActionType::sendMailAction:
            // Add user's email addresses to action emailAddress field and filter invalid addresses
            if (auto emailAction = action.dynamicCast<class vms::event::SendMailAction>())
                updateRecipientsList(emailAction);
            break;

        default:
            break;
    }
}

bool ExtendedRuleProcessor::executeActionInternal(const vms::event::AbstractActionPtr& action)
{
    bool result = base_type::executeActionInternal(action);
    if (!result)
    {
        switch (action->actionType())
        {
            case ActionType::sendMailAction:
                result = sendMail(action.dynamicCast<class vms::event::SendMailAction>());
                break;
            case ActionType::bookmarkAction:
                result = executeBookmarkAction(action);
                break;
            case ActionType::cameraOutputAction:
                result = triggerCameraOutput(action.dynamicCast<class vms::event::CameraOutputAction>());
                break;
            case ActionType::cameraRecordingAction:
                result = executeRecordingAction(action.dynamicCast<class vms::event::RecordingAction>());
                break;
            case ActionType::panicRecordingAction:
                result = executePanicAction(action.dynamicCast<class vms::event::PanicAction>());
                break;
            case ActionType::executePtzPresetAction:
                result = executePtzAction(action);
                break;
            case ActionType::execHttpRequestAction:
                result = executeHttpRequestAction(action);
                break;
            case ActionType::sayTextAction:
                result = executeSayTextAction(action);
                break;
            case ActionType::playSoundAction:
            case ActionType::playSoundOnceAction:
                result = executePlaySoundAction(action);
            default:
                break;
        }
    }

    if (result)
    {
        if (actionRequiresLogging(action))
        {
            serverModule()->serverDb()->saveActionToDB(action);
        }
        else
        {
            NX_DEBUG(this, "Omitted event logging at executeActionInternal");
        }
    }

    return result;
}

bool ExtendedRuleProcessor::executePlaySoundAction(
    const vms::event::AbstractActionPtr& action)
{
    const auto params = action->getParams();
    const auto resource = resourcePool()->getResourceById<nx::vms::server::resource::Camera>(
        params.actionResourceId);

    if (!resource)
        return false;

    QnAudioTransmitterPtr transmitter;
    if (resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        transmitter = resource->getAudioTransmitter();

    if (!transmitter)
        return false;

    if (action->actionType() == ActionType::playSoundOnceAction)
    {
        auto url = lit("dbfile://notifications/") + params.url;

        QnAviResourcePtr resource(new QnAviResource(url, serverModule()->commonModule()));
        resource->setStatus(Qn::Online);
        QnAbstractStreamDataProviderPtr provider(
            serverModule()->dataProviderFactory()->createDataProvider(resource));

        provider.dynamicCast<QnAbstractArchiveStreamReader>()->setCycleMode(false);

        transmitter->subscribe(provider, QnAbstractAudioTransmitter::kSingleNotificationPriority);

        provider->startIfNotRunning();
    }
    else
    {

        QnAbstractStreamDataProviderPtr provider;
        if (action->getToggleState() == vms::api::EventState::active)
        {
            provider = audioStreamPool()->getActionDataProvider(action);
            transmitter->subscribe(provider, QnAbstractAudioTransmitter::kContinuousNotificationPriority);
            provider->startIfNotRunning();
        }
        else if (action->getToggleState() == vms::api::EventState::inactive)
        {
            provider = audioStreamPool()->getActionDataProvider(action);
            transmitter->unsubscribe(provider.data());
            if (provider->processorsCount() == 0)
                audioStreamPool()->destroyActionDataProvider(action);
        }
    }

    return true;

}

bool ExtendedRuleProcessor::executeSayTextAction(const vms::event::AbstractActionPtr& action)
{
#if !defined(EDGE_SERVER) && !defined(__aarch64__)
    const auto params = action->getParams();
    const auto text = params.sayText;
    const auto resource = resourcePool()->getResourceById<nx::vms::server::resource::Camera>(
        params.actionResourceId);
    if (!resource)
        return false;

    QnAudioTransmitterPtr transmitter;
    if (resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        transmitter = resource->getAudioTransmitter();

    if (!transmitter)
        return false;

    QnAbstractStreamDataProviderPtr speechProvider(new QnSpeechSynthesisDataProvider(text));
    transmitter->subscribe(speechProvider, QnAbstractAudioTransmitter::kSingleNotificationPriority);
    speechProvider->startIfNotRunning();
    return true;
#else
    return true;
#endif
}

bool ExtendedRuleProcessor::executePanicAction(const vms::event::PanicActionPtr& action)
{
    const QnUuid serverGuid(serverModule()->settings().serverGuid());
    const QnResourcePtr& mediaServerRes = resourcePool()->getResourceById(serverGuid);
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (mediaServerRes.data());
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == Qn::PM_User)
        return true; // ignore panic business action if panic mode turn on by user

    Qn::PanicMode val = Qn::PM_None;
    if (action->getToggleState() == vms::api::EventState::active)
        val =  Qn::PM_BusinessEvents;
    mediaServer->setPanicMode(val);
    resourcePropertyDictionary()->saveParams(mediaServer->getId());
    return true;
}

bool ExtendedRuleProcessor::executeHttpRequestAction(const vms::event::AbstractActionPtr& action)
{
    const nx::vms::event::ActionParameters& actionParameters=action->getParams();
    nx::network::http::StringType requestType = actionParameters.requestType;

    nx::utils::Url url(action->getParams().url);
    if ((actionParameters.requestType == nx::network::http::Method::get) ||
        (actionParameters.requestType == nx::network::http::Method::delete_) ||
        (actionParameters.requestType.isEmpty() && actionParameters.text.isEmpty()))
    {
        auto callback = [action](
            SystemError::ErrorCode osErrorCode,
            int statusCode,
            nx::network::http::StringType, /*content type*/
            nx::network::http::BufferType messageBody,
            nx::network::http::HttpHeaders /*httpResponseHeaders*/)
            {
                if (osErrorCode != SystemError::noError ||
                    statusCode != nx::network::http::StatusCode::ok)
                {
                    qWarning() << "Failed to execute HTTP action for url "
                        << QUrl(action->getParams().url).toString(QUrl::RemoveUserInfo)
                        << "osErrorCode:" << osErrorCode
                        << "HTTP result:" << statusCode
                        << "message:" << messageBody;
                }
            };

        if (requestType.isEmpty())
            requestType = nx::network::http::Method::get;

        nx::network::http::downloadFileAsyncEx(
            url,
            callback,
            nx::network::http::HttpHeaders(),
            actionParameters.authType,
            nx::network::http::AsyncHttpClient::Timeouts(),
            requestType);
        return true;
    }
    else
    {
        auto callback = [action](SystemError::ErrorCode osErrorCode, int statusCode)
        {
            if (osErrorCode != SystemError::noError ||
                statusCode != nx::network::http::StatusCode::ok)
            {
                qWarning() << "Failed to execute HTTP action for url "
                           << QUrl(action->getParams().url).toString(QUrl::RemoveUserInfo)
                           << "osErrorCode:" << osErrorCode
                           << "HTTP result:" << statusCode;
            }
        };

        QByteArray contentType = actionParameters.contentType.toUtf8();
        if (contentType.isEmpty())
            contentType = nx::vms::server::Utils::autoDetectHttpContentType(actionParameters.text.toUtf8());

        if (requestType.isEmpty())
            requestType = nx::network::http::Method::post;

        nx::network::http::uploadDataAsync(url,
            action->getParams().text.toUtf8(),
            contentType,
            nx::network::http::HttpHeaders(),
            callback,
            actionParameters.authType,
            QString(), QString(), //< login/password.
            requestType);
        return true;
    }
}

bool ExtendedRuleProcessor::executePtzAction(const vms::event::AbstractActionPtr& action)
{
    auto camera = resourcePool()->getResourceById<nx::vms::server::resource::Camera>(action->getParams().actionResourceId);
    if (!camera)
        return false;
    if (camera->getDewarpingParams().enabled)
        return broadcastAction(action); // execute action on a client side

    QnPtzControllerPtr controller = camera->serverModule()->ptzControllerPool()->controller(camera);
    if (!controller)
        return false;
    return controller->activatePreset(action->getParams().presetId, QnAbstractPtzController::MaxPtzSpeed);
}

bool ExtendedRuleProcessor::executeRecordingAction(const vms::event::RecordingActionPtr& action)
{
    NX_ASSERT(action);
    auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(action->getParams().actionResourceId);
    //NX_ASSERT(camera);
    bool rez = false;
    if (camera)
    {
        auto toggleState = action->getToggleState();
        // todo: if camera is offline function return false. Need some tries on timer event
        if (toggleState == vms::api::EventState::active || //< Prolonged actions starts
            action->getDurationSec() > 0) //< Instant action
        {
            rez = recordingManager()->startForcedRecording(
                camera,
                action->getStreamQuality(),
                action->getFps(),
                action->getRecordBeforeSec(),
                action->getRecordAfterSec(),
                action->getDurationSec());
        }
        else
        {
            rez = recordingManager()->stopForcedRecording(camera);
        }
    }
    return rez;
}

bool ExtendedRuleProcessor::executeBookmarkAction(const vms::event::AbstractActionPtr& action)
{
    const auto cameraId = action
        ? action->getParams().actionResourceId
        : QnUuid();

    const auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(cameraId);
    if (!camera || !fixActionTimeFields(action))
        return false;

    const auto bookmark = helpers::bookmarkFromAction(action, camera);
    return serverModule()->serverDb()->addBookmark(bookmark);
}

QnUuid ExtendedRuleProcessor::getGuid() const
{
    return QnUuid(serverModule()->settings().serverGuid());
}

bool ExtendedRuleProcessor::triggerCameraOutput(const vms::event::CameraOutputActionPtr& action)
{
    auto resource = resourcePool()->getResourceById(action->getParams().actionResourceId);
    if (!resource)
    {
        NX_WARNING(this, lit("Received BA_CameraOutput with no resource reference. Ignoring..."));
        return false;
    }
    const auto securityCam = dynamic_cast<nx::vms::server::resource::Camera*>(resource.data());
    if (!securityCam)
    {
        NX_WARNING(this, lit("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId().toString()));
        return false;
    }
    QString relayOutputId = action->getRelayOutputId();
    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = action->getToggleState() != vms::api::EventState::inactive;

    return securityCam->setOutputPortState(
                relayOutputId,
                on,
                autoResetTimeout);
}

ExtendedRuleProcessor::TimespampedFrame ExtendedRuleProcessor::getEventScreenshotEncoded(
    const QnUuid& id,
    qint64 timestampUsec,
    QSize dstSize) const
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(moduleGUID());

    if (!camera || !server)
        return TimespampedFrame();

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.usecSinceEpoch = timestampUsec;
    request.size = dstSize;
    request.roundMethod = nx::api::CameraImageRequest::RoundMethod::precise;

    QnMultiserverThumbnailRestHandler handler(serverModule());
    TimespampedFrame timestemedFrame;
    //qint64 frameTimestampUsec = 0;
    QByteArray contentType;
    auto result = handler.getScreenshot(serverModule()->commonModule(), request, timestemedFrame.frame, contentType,
        server->getPort(), &timestemedFrame.timestampUsec);
    if (result != nx::network::http::StatusCode::ok)
        return TimespampedFrame();
    return timestemedFrame;
}

bool ExtendedRuleProcessor::sendMailInternal(const vms::event::SendMailActionPtr& action, int aggregatedResCount)
{
    NX_ASSERT(action);

    QStringList recipients = action->getParams().emailAddress.split(kNewEmailDelimiter, QString::SkipEmptyParts);

    if (recipients.isEmpty())
    {
        NX_WARNING(this, lit("Action SendMail (rule %1) missing valid recipients. Ignoring...").arg(action->getRuleId().toString()));
        NX_WARNING(this, lit("All recipients: ") + recipients.join(QLatin1String("; ")));
        return false;
    }

    NX_DEBUG(this, lit("Processing action SendMail. Sending mail to %1").
        arg(recipients.join(QLatin1String("; "))));

    const auto sendMailFunction =
        [this, action, recipients, aggregatedResCount]()
        {
            nx::utils::concurrent::run(
                &m_emailThreadPool,
                std::bind(&ExtendedRuleProcessor::sendEmailAsync, this, action, recipients, aggregatedResCount));
        };

    executeDelayed(sendMailFunction, kEmailSendDelay, serverModule()->eventConnector()->thread());
    return true;
}

void ExtendedRuleProcessor::sendEmailAsync(
    vms::event::SendMailActionPtr action,
    QStringList recipients, int aggregatedResCount)
{
    const bool isHtml = !globalSettings()->isUseTextEmailFormat();

    EmailManagerImpl::AttachmentList attachments;
    QVariantMap contextMap = eventDescriptionMap(action, action->aggregationInfo(), attachments);
    EmailAttachmentData attachmentData(action->getRuntimeParams().eventType);  // TODO: https://networkoptix.atlassian.net/browse/VMS-2831
    QnEmailSettings emailSettings = globalSettings()->emailSettings();
    QString cloudOwnerAccount = globalSettings()->cloudAccountName();

    auto addIcon =
        [&attachments, &contextMap](const QString& name, const QString& source)
        {
            attachments << EmailManagerImpl::AttachmentPtr(
                new EmailManagerImpl::Attachment(
                    name,
                    source,
                    tpImageMimeType));
            contextMap[name] = lit("cid:") + name;
        };

    if (isHtml)
    {
        addIcon(tpProductLogo, lit(":/skin/email_attachments/productLogo.png"));
        addIcon(tpSystemIcon, lit(":/skin/email_attachments/systemIcon.png"));

        const auto eventType = action->getRuntimeParams().eventType;
        switch (eventType)
        {
            case EventType::cameraDisconnectEvent:
            case EventType::licenseIssueEvent:
            case EventType::networkIssueEvent:
            case EventType::softwareTriggerEvent:
                addIcon(tpSourceIcon, lit(":/skin/email_attachments/cameraIcon.png"));
                break;
            default:
                break;
        }
    }

    if (!cloudOwnerAccount.isEmpty())
    {
        if (isHtml)
            addIcon(tpOwnerIcon, lit(":/skin/email_attachments/ownerIcon.png"));

        contextMap[tpCloudOwnerEmail] = cloudOwnerAccount;

        const auto allUsers = resourcePool()->getResources<QnUserResource>();
        for (const auto& user: allUsers)
        {
            if (user->isCloud() && user->getEmail() == cloudOwnerAccount)
            {
                contextMap[tpCloudOwner] = user->fullName();
                break;
            }
        }
    }

    QnEmailAddress supportEmail(emailSettings.supportEmail);

    contextMap[tpCompanyName] = QnAppInfo::organizationName();
    contextMap[tpCompanyUrl] = QnAppInfo::companyUrl();
    contextMap[tpSupportLink] = supportEmail.isValid()
        ? lit("mailto:%1").arg(supportEmail.value())
        : emailSettings.supportEmail;
    contextMap[tpSupportLinkText] = emailSettings.supportEmail;
    contextMap[tpSystemName] = globalSettings()->systemName();
    contextMap[tpSystemSignature] = emailSettings.signature;

    contextMap[tpCaption] = action->getRuntimeParams().caption;
    contextMap[tpDescription] = action->getRuntimeParams().description;
    vms::event::StringsHelper helper(serverModule()->commonModule());
    contextMap[tpSource] = helper.getResoureNameFromParams(action->getRuntimeParams(), Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(action->getRuntimeParams());

    QString messageBody;
    if (isHtml)
        renderTemplateFromFile(attachmentData.templatePath, contextMap, &messageBody);

    QString messagePlainBody;
    QFileInfo fileInfo(attachmentData.templatePath);
    QString plainTemplatePath = fileInfo.dir().path() + lit("/") + fileInfo.baseName() + lit("_plain.mustache");
    renderTemplateFromFile(plainTemplatePath, contextMap, &messagePlainBody);

    const bool isWindowsLineFeed = globalSettings()->isUseWindowsEmailLineFeed();
    if (isWindowsLineFeed)
    {
        messageBody.replace("\n", "\r\n");
        messagePlainBody.replace("\n", "\r\n");
    }

    // TODO: #vkutin #gdm Need to refactor aggregation entirely.
    // I can't figure a proper abstraction for it at this point.
    const int aggregatedCount = action->getRuntimeParams().eventType == EventType::softwareTriggerEvent
        ? action->aggregationInfo().toList().count()
        : aggregatedResCount;

    const auto subject = aggregatedCount > 1
        ? helper.eventAtResources(action->getRuntimeParams())
        : helper.eventAtResource(action->getRuntimeParams(), Qn::RI_WithUrl);

    EmailManagerImpl::Message message(
        recipients,
        subject,
        messageBody,
        messagePlainBody,
        emailSettings.timeout,
        attachments);

    if (!m_emailManager->sendEmail(emailSettings, message))
    {
        vms::event::AbstractActionPtr action(new vms::event::SystemHealthAction(QnSystemHealth::EmailSendError));
        broadcastAction(action);
        NX_WARNING(this, lit("Error processing action SendMail."));
    }
}

bool ExtendedRuleProcessor::sendMail(const vms::event::SendMailActionPtr& action)
{
    // QnMutexLocker lk(&m_mutex); <- m_mutex is already locked down the stack.

    if( action->getRuntimeParams().eventType != EventType::cameraDisconnectEvent &&
        action->getRuntimeParams().eventType != EventType::networkIssueEvent )
    {
        return sendMailInternal(action, 1);
    }

    // Aggregating by recipients and event type.

    SendEmailAggregationData& aggregatedData = m_aggregatedEmails[action->getRuleId()];

    auto ruleManager = serverModule()->commonModule()->eventRuleManager();
    auto rule = ruleManager->rule(action->getRuleId());
    std::chrono::seconds aggregationPeriod = kDefaultMailAggregationPeriod;
    if (rule && rule->aggregationPeriod())
        aggregationPeriod = std::chrono::seconds(rule->aggregationPeriod());

    const bool dontGroupMail = !aggregatedData.periodicTaskID
        && aggregatedData.lastMailTime.hasExpired(aggregationPeriod);
    aggregatedData.lastMailTime.restart();
    if (dontGroupMail)
        return sendMailInternal(action, 1);

    if (!aggregatedData.action)
    {
        aggregatedData.action =
            vms::event::SendMailActionPtr(new vms::event::SendMailAction(*action));
    }
    if (!aggregatedData.periodicTaskID)
    {
        aggregatedData.periodicTaskID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&ExtendedRuleProcessor::sendAggregationEmail, this, action->getRuleId()),
            aggregationPeriod);
    }

    auto aggregationInfo = aggregatedData.action->aggregationInfo();
    aggregationInfo.append(action->getRuntimeParams(), action->aggregationInfo(), /*oneRecordPerKey*/ true);
    aggregatedData.action->setAggregationInfo(aggregationInfo);

    return false; //< Don't write action to log so far.
}

void ExtendedRuleProcessor::sendAggregationEmail(const QnUuid& ruleId)
{
    QnMutexLocker lk(&m_mutex);

    auto aggregatedActionIter = m_aggregatedEmails.find(ruleId);
    if (aggregatedActionIter == m_aggregatedEmails.end())
        return;

    const int eventCount = aggregatedActionIter->action->aggregationInfo().totalCount();
    if (sendMailInternal(aggregatedActionIter->action, eventCount))
    {
        auto actionCopy(aggregatedActionIter->action);
        actionCopy->getRuntimeParams().eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
        if (eventCount > 1)
            actionCopy->getRuntimeParams().eventResourceId = QnUuid();
        actionCopy->setAggregationCount(eventCount);
        serverModule()->serverDb()->saveActionToDB(actionCopy);
    }
    else
    {
        NX_DEBUG(this, lit("Failed to send aggregated email"));
    }

    aggregatedActionIter->action.clear();
    aggregatedActionIter->periodicTaskID = 0;
}

QVariantMap ExtendedRuleProcessor::eventDescriptionMap(
    const vms::event::AbstractActionPtr& action,
    const vms::event::AggregationInfo &aggregationInfo,
    EmailManagerImpl::AttachmentList& attachments) const
{
    const auto currentMsecSinceEpoch = qnSyncTime->currentMSecsSinceEpoch();

    vms::event::EventParameters params = action->getRuntimeParams();
    EventType eventType = params.eventType;

    QVariantMap contextMap;
    vms::event::StringsHelper helper(serverModule()->commonModule());

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    const int deviceCount = aggregationInfo.toList().size();
    contextMap[tpEvent] = helper.eventName(eventType, qMax(1, deviceCount));
    contextMap[tpSource] = helper.getResoureNameFromParams(
        params, Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(params);

    const auto aggregationCount = action->getAggregationCount();
    if (aggregationCount > 1)
        contextMap[tpCount] = QString::number(aggregationCount);

    switch (eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        {
            auto camRes = resourcePool()->getResourceById<QnVirtualCameraResource>(
                action->getRuntimeParams().eventResourceId);
            cameraHistoryPool()->updateCameraHistorySync(camRes);
            if (camRes->hasVideo(nullptr))
            {
                const qint64 eventTimeUs = action->getRuntimeParams().eventTimestampUsec;
                const qint64 currentTimeBeforeGetUs = qnSyncTime->currentUSecsSinceEpoch();

                TimespampedFrame timestempedFrame = getEventScreenshotEncoded(
                    action->getRuntimeParams().eventResourceId, eventTimeUs, SCREENSHOT_SIZE);

                if (!timestempedFrame.frame.isNull())
                {
                    static const qint64 kIntervalUs = 5'000'000;
                    const qint64 currentTimeUs = qnSyncTime->currentUSecsSinceEpoch();

                    if (camRes->getStatus() == Qn::Recording)
                    {
                        contextMap[tpUrlInt] = helper.urlForCamera(
                            params.eventResourceId, params.eventTimestampUsec, /*isPublic*/ false);
                        contextMap[tpUrlExt] = helper.urlForCamera(
                            params.eventResourceId, params.eventTimestampUsec, /*isPublic*/ true);
                    }

                    if (std::abs(currentTimeUs - timestempedFrame.timestampUsec) < kIntervalUs)
                    {
                        // Only fresh screenshots are sent.
                        QBuffer screenshotStream(&timestempedFrame.frame);
                        const auto fileName = tpScreenshotAttachmentName.arg(currentMsecSinceEpoch);
                        attachments.append(
                            EmailManagerImpl::AttachmentPtr(
                                new EmailManagerImpl::Attachment(
                                    fileName,
                                    screenshotStream,
                                    lit("image/jpeg"))));
                        contextMap[tpScreenshotFilename] = lit("cid:") + fileName;
                    }
                }
            }
            break;
        }

        case EventType::softwareTriggerEvent:
        {
            contextMap[tpTriggerName] = params.caption.trimmed().isEmpty()
                ? helper.defaultSoftwareTriggerName()
                : params.caption;

            contextMap[tpTimestamp] = helper.eventTimestampInHtml(params, aggregationCount);
            contextMap[tpTimestampDate] = helper.eventTimestampDate(params);
            contextMap[tpTimestampTime] = helper.eventTimestampTime(params);

            NX_ASSERT(params.metadata.instigators.size() == 1);
            if (params.metadata.instigators.empty())
                break;

            const auto& userId = params.metadata.instigators[0];
            const auto user = resourcePool()->getResourceById(userId);
            NX_ASSERT(user, "Unknown user id as soft trigger instigator");

            contextMap[tpUser] = user ? user->getName() : userId.toString();
            break;
        }

        case EventType::analyticsSdkEvent:
        {
            contextMap[tpAnalyticsSdkEventType] = helper.getAnalyticsSdkEventName(params);
            contextMap[tpTimestamp] = helper.eventTimestampInHtml(params, aggregationCount);
            contextMap[tpTimestampDate] = helper.eventTimestampDate(params);
            contextMap[tpTimestampTime] = helper.eventTimestampTime(params);

            auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(
                params.eventResourceId);

            cameraHistoryPool()->updateCameraHistorySync(camera);
            if (camera->hasVideo(nullptr))
            {
                QByteArray screenshotData = getEventScreenshotEncoded(params.eventResourceId,
                    params.eventTimestampUsec, SCREENSHOT_SIZE).frame;
                if (!screenshotData.isNull())
                {
                    contextMap[tpUrlInt] = helper.urlForCamera(
                        params.eventResourceId, params.eventTimestampUsec, /*isPublic*/ false);
                    contextMap[tpUrlExt] = helper.urlForCamera(
                        params.eventResourceId, params.eventTimestampUsec, /*isPublic*/ true);

                    QBuffer screenshotStream(&screenshotData);
                    const auto fileName = tpScreenshotAttachmentName.arg(currentMsecSinceEpoch);
                    attachments.append(
                        EmailManagerImpl::AttachmentPtr(
                            new EmailManagerImpl::Attachment(
                                fileName,
                                screenshotStream,
                                lit("image/jpeg"))));
                    contextMap[tpScreenshotFilename] = lit("cid:") + fileName;
                }
            }

            break;
        }

        case EventType::userDefinedEvent:
        {
            auto metadata = action->getRuntimeParams().metadata;
            if (!metadata.cameraRefs.empty())
            {
                QVariantList cameras;
                int screenshotNum = 1;
                for (const auto& cameraId: metadata.cameraRefs)
                {
                    auto camRes = camera_id_helper::findCameraByFlexibleId(
                        resourcePool(), cameraId);
                    if (camRes)
                    {
                        QVariantMap camera;

                        QnResourceDisplayInfo camInfo(camRes);
                        camera[tpCameraName] = camInfo.name();
                        camera[tpCameraIP] = camInfo.host();

                        cameraHistoryPool()->updateCameraHistorySync(camRes);
                        camera[tpUrlInt] = helper.urlForCamera(
                            camRes->getId(), params.eventTimestampUsec, /*isPublic*/ false);
                        camera[tpUrlExt] = helper.urlForCamera(
                            camRes->getId(), params.eventTimestampUsec, /*isPublic*/ true);

                        QByteArray screenshotData = getEventScreenshotEncoded(
                            camRes->getId(), params.eventTimestampUsec, SCREENSHOT_SIZE).frame;
                        if (!screenshotData.isNull())
                        {
                            QBuffer screenshotStream(&screenshotData);
                            const auto fileName = tpScreenshotWithNumAttachmentName
                                .arg(currentMsecSinceEpoch).arg(screenshotNum++);
                            attachments.append(
                                EmailManagerImpl::AttachmentPtr(
                                    new EmailManagerImpl::Attachment(
                                        fileName,
                                        screenshotStream,
                                        lit("image/jpeg"))));
                            camera[QLatin1String("screenshot")] = lit("cid:") + fileName;
                        }

                        cameras << camera;
                    }
                }
                if (!cameras.isEmpty())
                {
                    contextMap[tpHasCameras] = lit("1");
                    contextMap[tpCameras] = cameras;
                }
            }

            break;
        }

        default:
            break;
    }

    contextMap[tpAggregated] = aggregatedEventDetailsMap(action, aggregationInfo, Qn::ResourceInfoLevel::RI_NameOnly);
    // TODO: CHECK!
    return contextMap;
}

QVariantList ExtendedRuleProcessor::aggregatedEventDetailsMap(
    const vms::event::AbstractActionPtr& action,
    const vms::event::AggregationInfo& aggregationInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    if (!aggregationInfo.isEmpty())
        return aggregatedEventDetailsMap(action, aggregationInfo.toList(), detailLevel);

    return { eventDetailsMap(action, vms::event::InfoDetail(action->getRuntimeParams(),
        action->getAggregationCount()), detailLevel) };
}

QVariantList ExtendedRuleProcessor::aggregatedEventDetailsMap(
    const vms::event::AbstractActionPtr& action,
    const QList<vms::event::InfoDetail>& aggregationDetailList,
    Qn::ResourceInfoLevel detailLevel) const
{
    int index = 0;
    QVariantList result;

    auto sortedList = aggregationDetailList;
    std::sort(sortedList.begin(), sortedList.end(),
        [](const vms::event::InfoDetail& left, const vms::event::InfoDetail& right)
        {
            return left.runtimeParams().eventTimestampUsec
                < right.runtimeParams().eventTimestampUsec;
        });

    for (const auto& detail: sortedList)
    {
        auto detailsMap = eventDetailsMap(action, detail, detailLevel);
        detailsMap[tpIndex] = (++index);
        result << detailsMap;
    }

    return result;
}

QVariantMap ExtendedRuleProcessor::eventDetailsMap(
    const vms::event::AbstractActionPtr& action,
    const vms::event::InfoDetail& aggregationData,
    Qn::ResourceInfoLevel detailLevel,
    bool addSubAggregationData) const
{
    const vms::event::EventParameters& params = aggregationData.runtimeParams();
    const int aggregationCount = aggregationData.count();
    vms::event::StringsHelper helper(serverModule()->commonModule());

    QVariantMap detailsMap;

    if (addSubAggregationData)
    {
        const vms::event::AggregationInfo& subAggregationData = aggregationData.subAggregationData();
        detailsMap[tpAggregated] = !subAggregationData.isEmpty()
            ? aggregatedEventDetailsMap(action, subAggregationData, detailLevel)
            : (QVariantList() << eventDetailsMap(action, aggregationData, detailLevel, false));
    }

    detailsMap[tpCount] = QString::number(aggregationCount);
    detailsMap[tpTimestamp] = helper.eventTimestampInHtml(params, aggregationCount);
    detailsMap[tpTimestampDate] = helper.eventTimestampDate(params);
    detailsMap[tpTimestampTime] = helper.eventTimestampTime(params);

    switch (params.eventType)
    {
        case EventType::cameraDisconnectEvent:
        case EventType::softwareTriggerEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            break;
        }

        case EventType::cameraInputEvent:
        {
            // Try to use port name if possible, otherwise use port ID as is.
            auto resource = resourcePool()->getResourceById<QnVirtualCameraResource>(
                params.eventResourceId);
            NX_ASSERT(resource);
            auto ports = resource->ioPortDescriptions();
            auto portIt = std::find_if(ports.begin(), ports.end(),
                [&](auto port) { return port.id == params.inputPortId; });

            if (portIt != ports.end() && !portIt->inputName.isEmpty())
                detailsMap[tpInputPort] =
                    QString("%1 (%2)").arg(params.inputPortId).arg(portIt->inputName);
            else
                detailsMap[tpInputPort] = params.inputPortId;
            break;
        }

        case EventType::networkIssueEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            detailsMap[tpReason] = helper.eventReason(params);
            break;
        }

        case EventType::storageFailureEvent:
        case EventType::serverFailureEvent:
        case EventType::licenseIssueEvent:
        case EventType::backupFinishedEvent:
        {
            detailsMap[tpReason] = helper.eventReason(params);

            // Fill event-specific reason context here
            QVariantMap reasonContext;

            if (params.reasonCode == vms::api::EventReason::licenseRemoved)
            {
                QVariantList disabledCameras;

                for (const auto& id: params.description.split(L';'))
                {
                    if (const auto& camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid(id)))
                        disabledCameras << QnResourceDisplayInfo(camera).toString(Qn::RI_WithUrl);
                }

                reasonContext[lit("disabledCameras")] = disabledCameras;
            }

            detailsMap[tpReasonContext] = reasonContext;
            break;
        }

        case EventType::cameraIpConflictEvent:
        {
            detailsMap[lit("cameraConflictAddress")] = params.caption;
            QVariantList conflicts;
            int n = 0;
            for (const auto& mac: params.description.split(vms::event::IpConflictEvent::delimiter()))
            {
                QVariantMap conflict;
                conflict[lit("number")] = ++n;
                conflict[lit("mac")] = mac;
                conflicts << conflict;
            }

            detailsMap[lit("cameraConflicts")] = conflicts;
            break;
        }

        case EventType::serverConflictEvent:
        {
            QnCameraConflictList conflicts;
            conflicts.sourceServer = params.caption;
            conflicts.decode(params.description);

            QVariantList conflictsList;
            int n = 0;
            for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr)
            {
                const auto& server = itr.key();
                for (const auto& camera: conflicts.camerasByServer[server])
                {
                    QVariantMap conflict;
                    conflict[lit("number")] = ++n;
                    conflict[lit("ip")] = server;
                    conflict[lit("mac")] = camera;
                    conflictsList << conflict;
                }
            }

            detailsMap[lit("msConflicts")] = conflictsList;
            break;
        }

        default:
            break;
    }

    return detailsMap;
}

/*
* This method is called once per action, calculates all recipients and packs them into
* emailAddress action parameter with new delimiter.
*/
void ExtendedRuleProcessor::updateRecipientsList(
    const vms::event::SendMailActionPtr& action) const
{
    QStringList additional = action->getParams().emailAddress.split(kOldEmailDelimiter,
        QString::SkipEmptyParts);

    QList<QnUuid> userRoles;
    QnUserResourceList users;
    userRolesManager()->usersAndRoles(action->getResources(), users, userRoles);

    QStringList recipients;
    auto addRecipient = [&recipients](const QString& email)
        {
            const QString simplified = email.trimmed().toLower();
            if (simplified.isEmpty())
                return;

            QnEmailAddress address(simplified);
            if (address.isValid())
                recipients.append(address.value());
        };

    for (const auto& email: additional)
        addRecipient(email);

    for (const auto& user: users)
    {
        if (user->isEnabled())
            addRecipient(user->getEmail());
    }

    for (const auto& userRole: userRoles)
    {
        for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(userRole))
        {
            const auto& user = subject.user();
            NX_ASSERT(user);
            if (user && user->isEnabled())
                addRecipient(user->getEmail());
        }
    }

    recipients.removeDuplicates();
    action->getParams().emailAddress = recipients.join(kNewEmailDelimiter);
}

} // namespace event
} // namespace vms::server
} // namespace nx
