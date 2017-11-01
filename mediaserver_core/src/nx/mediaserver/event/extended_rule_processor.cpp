#include "extended_rule_processor.h"

#include <map>

#include <QtCore/QList>

#include <api/app_server_connection.h>

#include <nx/vms/event/actions/actions.h>
#include <nx/vms/event/events/ip_conflict_event.h>
#include <nx/vms/event/events/server_conflict_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/mediaserver/event/event_connector.h>

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

#include <plugins/resource/avi/avi_resource.h>

#include <database/server_db.h>

#include <camera/camera_pool.h>

#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <media_server/serverutil.h>

#include <nx/utils/log/log.h>
#include <camera/get_image_helper.h>

#include <QtConcurrent/QtConcurrent>
#include <utils/email/email.h>
#include <nx/email/email_manager_impl.h>
#include "nx_ec/data/api_email_data.h"
#include <nx/utils/timer_manager.h>
#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <nx/email/mustache/mustache_helper.h>

#include <common/common_module.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <utils/common/delayed.h>

#if !defined(EDGE_SERVER)
#include <providers/speech_synthesis_data_provider.h>
#endif

#include <providers/stored_file_data_provider.h>
#include <streaming/audio_streamer_pool.h>
#include <nx/network/http/asynchttpclient.h>

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

namespace {

static const QString tpProductLogoFilename(lit("productLogoFilename"));
static const QString tpEventLogoFilename(lit("eventLogoFilename"));
static const QString tpProductLogo(lit("logo"));
static const QString tpSystemIcon(lit("systemIcon"));
static const QString tpOwnerIcon(lit("ownerIcon"));
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
static const QString tpScreenshot(lit("screenshot.jpeg"));
static const QString tpScreenshotNum(lit("screenshot%1.jpeg"));

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
static const unsigned int emailAggregationPeriodMS = 30 * MS_PER_SEC;

static const int kEmailSendDelay = 0;

static const QChar kOldEmailDelimiter(L';');
static const QChar kNewEmailDelimiter(L' ');

} // namespace

namespace nx {
namespace mediaserver {
namespace event {

struct EmailAttachmentData
{
    EmailAttachmentData(vms::event::EventType eventType)
    {
        switch (eventType)
        {
            case vms::event::cameraMotionEvent:
                templatePath = lit(":/email_templates/camera_motion.mustache");
                imageName = lit("camera.png");
                break;
            case vms::event::cameraInputEvent:
                templatePath = lit(":/email_templates/camera_input.mustache");
                imageName = lit("camera.png");
                break;
            case vms::event::cameraDisconnectEvent:
                templatePath = lit(":/email_templates/camera_disconnect.mustache");
                imageName = lit("camera.png");
                break;
            case vms::event::storageFailureEvent:
                templatePath = lit(":/email_templates/storage_failure.mustache");
                imageName = lit("storage.png");
                break;
            case vms::event::networkIssueEvent:
                templatePath = lit(":/email_templates/network_issue.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::cameraIpConflictEvent:
                templatePath = lit(":/email_templates/camera_ip_conflict.mustache");
                imageName = lit("camera.png");
                break;
            case vms::event::serverFailureEvent:
                templatePath = lit(":/email_templates/mediaserver_failure.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::serverConflictEvent:
                templatePath = lit(":/email_templates/mediaserver_conflict.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::serverStartEvent:
                templatePath = lit(":/email_templates/mediaserver_started.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::licenseIssueEvent:
                templatePath = lit(":/email_templates/license_issue.mustache");
                imageName = lit("license.png");
                break;
            case vms::event::backupFinishedEvent:
                templatePath = lit(":/email_templates/backup_finished.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::analyticsSdkEvent:
                templatePath = lit(":/email_templates/analytics_event.mustache");
                imageName = lit("camera.png");
                break;
            case vms::event::userDefinedEvent:
                templatePath = lit(":/email_templates/generic_event.mustache");
                imageName = lit("server.png");
                break;
            case vms::event::softwareTriggerEvent:
                templatePath = lit(":/email_templates/software_trigger.mustache");
                break;
            default:
                NX_ASSERT(false, Q_FUNC_INFO, "All cases must be implemented.");
                break;
        }

        NX_ASSERT(!templatePath.isEmpty(), Q_FUNC_INFO, "Template path must be filled");
    }

    QString templatePath;
    QString imageName;
};

ExtendedRuleProcessor::ExtendedRuleProcessor(QnCommonModule* commonModule):
    base_type(commonModule),
    m_emailManager(new EmailManagerImpl(commonModule))
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &ExtendedRuleProcessor::onRemoveResource, Qt::QueuedConnection);
}

ExtendedRuleProcessor::~ExtendedRuleProcessor()
{
    quit();
    wait();
    m_emailThreadPool.waitForDone();

    QnMutexLocker lk(&m_mutex);
    while(!m_aggregatedEmails.isEmpty())
    {
        const quint64 taskID = m_aggregatedEmails.begin()->periodicTaskID;
        lk.unlock();
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(taskID);
        lk.relock();
        if (m_aggregatedEmails.begin()->periodicTaskID == taskID)  //task has not been removed in sendAggregationEmail while we were waiting
            m_aggregatedEmails.erase(m_aggregatedEmails.begin());
    }
}

void ExtendedRuleProcessor::onRemoveResource(const QnResourcePtr& resource)
{
    qnServerDb->removeLogForRes(resource->getId());
}

void ExtendedRuleProcessor::prepareAdditionActionParams(const vms::event::AbstractActionPtr& action)
{
    switch (action->actionType())
    {
        case vms::event::sendMailAction:
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
            case vms::event::sendMailAction:
                result = sendMail(action.dynamicCast<class vms::event::SendMailAction>());
                break;
            case vms::event::bookmarkAction:
                result = executeBookmarkAction(action);
                break;
            case vms::event::cameraOutputAction:
                result = triggerCameraOutput(action.dynamicCast<class vms::event::CameraOutputAction>());
                break;
            case vms::event::cameraRecordingAction:
                result = executeRecordingAction(action.dynamicCast<class vms::event::RecordingAction>());
                break;
            case vms::event::panicRecordingAction:
                result = executePanicAction(action.dynamicCast<class vms::event::PanicAction>());
                break;
            case vms::event::executePtzPresetAction:
                result = executePtzAction(action);
                break;
            case vms::event::execHttpRequestAction:
                result = executeHttpRequestAction(action);
                break;
            case vms::event::sayTextAction:
                result = executeSayTextAction(action);
                break;
            case vms::event::playSoundAction:
            case vms::event::playSoundOnceAction:
                result = executePlaySoundAction(action);
            default:
                break;
        }
    }

    if (result)
        qnServerDb->saveActionToDB(action);

    return result;
}

bool ExtendedRuleProcessor::executePlaySoundAction(
    const vms::event::AbstractActionPtr& action)
{
    const auto params = action->getParams();
    const auto resource = resourcePool()->getResourceById<QnSecurityCamResource>(
        params.actionResourceId);

    if (!resource)
        return false;

    QnAudioTransmitterPtr transmitter;
    if (resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        transmitter = resource->getAudioTransmitter();

    if (!transmitter)
        return false;


    if (action->actionType() == vms::event::playSoundOnceAction)
    {
        auto url = lit("dbfile://notifications/") + params.url;

        QnAviResourcePtr resource(new QnAviResource(url));
        resource->setCommonModule(commonModule());
        resource->setStatus(Qn::Online);
        QnAbstractStreamDataProviderPtr provider(
            resource->createDataProvider(Qn::ConnectionRole::CR_Default));

        provider.dynamicCast<QnAbstractArchiveStreamReader>()->setCycleMode(false);

        transmitter->subscribe(provider, QnAbstractAudioTransmitter::kSingleNotificationPriority);

        provider->startIfNotRunning();
    }
    else
    {

        QnAbstractStreamDataProviderPtr provider;
        if (action->getToggleState() == vms::event::EventState::active)
        {
            provider = QnAudioStreamerPool::instance()->getActionDataProvider(action);
            transmitter->subscribe(provider, QnAbstractAudioTransmitter::kContinuousNotificationPriority);
            provider->startIfNotRunning();
        }
        else if (action->getToggleState() == vms::event::EventState::inactive)
        {
            provider = QnAudioStreamerPool::instance()->getActionDataProvider(action);
            transmitter->unsubscribe(provider.data());
            if (provider->processorsCount() == 0)
                QnAudioStreamerPool::instance()->destroyActionDataProvider(action);
        }
    }

    return true;

}

bool ExtendedRuleProcessor::executeSayTextAction(const vms::event::AbstractActionPtr& action)
{
#if !defined(EDGE_SERVER)
    const auto params = action->getParams();
    const auto text = params.sayText;
    const auto resource = resourcePool()->getResourceById<QnSecurityCamResource>(
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
    const QnResourcePtr& mediaServerRes = resourcePool()->getResourceById(serverGuid());
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (mediaServerRes.data());
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == Qn::PM_User)
        return true; // ignore panic business action if panic mode turn on by user

    Qn::PanicMode val = Qn::PM_None;
    if (action->getToggleState() == vms::event::EventState::active)
        val =  Qn::PM_BusinessEvents;
    mediaServer->setPanicMode(val);
    commonModule()->propertyDictionary()->saveParams(mediaServer->getId());
    return true;
}

bool ExtendedRuleProcessor::executeHttpRequestAction(const vms::event::AbstractActionPtr& action)
{
    nx::utils::Url url(action->getParams().url);

    if (action->getParams().text.isEmpty())
    {
        auto callback = [action](SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType messageBody)
        {
            if (osErrorCode != SystemError::noError ||
                statusCode != nx_http::StatusCode::ok)
            {
                qWarning() << "Failed to execute HTTP action for url "
                    << QUrl(action->getParams().url).toString(QUrl::RemoveUserInfo)
                    << "osErrorCode:" << osErrorCode
                    << "HTTP result:" << statusCode
                    << "message:" << messageBody;
            }
        };

        nx_http::downloadFileAsync(
            url,
            callback);
        return true;
    }
    else
    {
        auto callback = [action](SystemError::ErrorCode osErrorCode, int statusCode)
        {
            if (osErrorCode != SystemError::noError ||
                statusCode != nx_http::StatusCode::ok)
            {
                qWarning() << "Failed to execute HTTP action for url "
                           << QUrl(action->getParams().url).toString(QUrl::RemoveUserInfo)
                           << "osErrorCode:" << osErrorCode
                           << "HTTP result:" << statusCode;
            }
        };

        QByteArray contentType = action->getParams().contentType.toUtf8();
        if (contentType.isEmpty())
            contentType = autoDetectHttpContentType(action->getParams().text.toUtf8());

        nx_http::uploadDataAsync(url,
            action->getParams().text.toUtf8(),
            contentType,
            nx_http::HttpHeaders(),
            callback);
        return true;
    }
}

bool ExtendedRuleProcessor::executePtzAction(const vms::event::AbstractActionPtr& action)
{
    auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(action->getParams().actionResourceId);
    if (!camera)
        return false;
    if (camera->getDewarpingParams().enabled)
        return broadcastAction(action); // execute action on a client side

    QnPtzControllerPtr controller = qnPtzPool->controller(camera);
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
        if (toggleState == vms::event::EventState::active || //< Prolonged actions starts
            action->getDurationSec() > 0) //< Instant action
        {
            rez = qnRecordingManager->startForcedRecording(
                camera,
                action->getStreamQuality(),
                action->getFps(),
                0, /* Record-before setup is forbidden */
                action->getRecordAfterSec(),
                action->getDurationSec());
        }
        else
        {
            rez = qnRecordingManager->stopForcedRecording(camera);
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
    return qnServerDb->addBookmark(bookmark);
}

QnUuid ExtendedRuleProcessor::getGuid() const
{
    return serverGuid();
}

bool ExtendedRuleProcessor::triggerCameraOutput(const vms::event::CameraOutputActionPtr& action)
{
    auto resource = resourcePool()->getResourceById(action->getParams().actionResourceId);
    if (!resource)
    {
        NX_LOG(lit("Received BA_CameraOutput with no resource reference. Ignoring..."), cl_logWARNING);
        return false;
    }
    QnSecurityCamResource* securityCam = dynamic_cast<QnSecurityCamResource*>(resource.data());
    if (!securityCam)
    {
        NX_LOG(lit("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId().toString()), cl_logWARNING);
        return false;
    }
    QString relayOutputId = action->getRelayOutputId();
    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = action->getToggleState() != vms::event::EventState::inactive;

    return securityCam->setRelayOutputState(
                relayOutputId,
                on,
                autoResetTimeout);
}

QByteArray ExtendedRuleProcessor::getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const
{
    QnVirtualCameraResourcePtr cameraRes = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    const QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (!cameraRes || !server)
        return QByteArray();

    QnThumbnailRequestData request;
    request.camera = cameraRes;
    request.msecSinceEpoch = timestampUsec / 1000;
    request.size = dstSize;
    request.imageFormat = QnThumbnailRequestData::JpgFormat;
    request.roundMethod = QnThumbnailRequestData::PreciseMethod;

    QnMultiserverThumbnailRestHandler handler;
    QByteArray frame;
    QByteArray contentType;
    auto result = handler.getScreenshot(commonModule(), request, frame, contentType, server->getPort());
    if (result != nx_http::StatusCode::ok)
        return QByteArray();
    return frame;

    //QSharedPointer<CLVideoDecoderOutput> frame = QnGetImageHelper::getImage(cameraRes.dynamicCast<QnVirtualCameraResource>(), timestampUsec, dstSize, QnThumbnailRequestData::KeyFrameAfterMethod);
    //return frame ? QnGetImageHelper::encodeImage(frame, "jpg") : QByteArray();
}

bool ExtendedRuleProcessor::sendMailInternal(const vms::event::SendMailActionPtr& action, int aggregatedResCount)
{
    NX_ASSERT(action);

    QStringList recipients = action->getParams().emailAddress.split(kNewEmailDelimiter);

    if (recipients.isEmpty())
    {
        NX_LOG(lit("Action SendMail (rule %1) missing valid recipients. Ignoring...").arg(action->getRuleId().toString()), cl_logWARNING);
        NX_LOG(lit("All recipients: ") + recipients.join(QLatin1String("; ")), cl_logWARNING);
        return false;
    }

    NX_LOG(lit("Processing action SendMail. Sending mail to %1").
        arg(recipients.join(QLatin1String("; "))), cl_logDEBUG1);

    const auto sendMailFunction =
        [this, action, recipients, aggregatedResCount]()
        {
            nx::utils::concurrent::run(
                &m_emailThreadPool,
                std::bind(&ExtendedRuleProcessor::sendEmailAsync, this, action, recipients, aggregatedResCount));
        };

    executeDelayed(sendMailFunction, kEmailSendDelay, qnEventRuleConnector->thread());
    return true;
}

void ExtendedRuleProcessor::sendEmailAsync(
    vms::event::SendMailActionPtr action,
    QStringList recipients, int aggregatedResCount)
{
    bool isHtml = !qnGlobalSettings->isUseTextEmailFormat();

    QnEmailAttachmentList attachments;
    QVariantMap contextMap = eventDescriptionMap(action, action->aggregationInfo(), attachments);
    EmailAttachmentData attachmentData(action->getRuntimeParams().eventType);  // TODO: https://networkoptix.atlassian.net/browse/VMS-2831
    QnEmailSettings emailSettings = commonModule()->globalSettings()->emailSettings();
    QString cloudOwnerAccount = commonModule()->globalSettings()->cloudAccountName();

    if (isHtml)
    {
        attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpProductLogo, lit(":/skin/email_attachments/productLogo.png"), tpImageMimeType)));
        attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpSystemIcon, lit(":/skin/email_attachments/systemIcon.png"), tpImageMimeType)));

        contextMap[tpProductLogoFilename] = lit("cid:") + tpProductLogo;
        contextMap[tpSystemIcon] = lit("cid:") + tpSystemIcon;
    }

    if (!cloudOwnerAccount.isEmpty())
    {
        if (isHtml)
        {
            attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpOwnerIcon, lit(":/skin/email_attachments/ownerIcon.png"), tpImageMimeType)));
            contextMap[tpOwnerIcon] = lit("cid:") + tpOwnerIcon;
        }

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
    contextMap[tpSystemName] = commonModule()->globalSettings()->systemName();
    contextMap[tpSystemSignature] = emailSettings.signature;

    contextMap[tpCaption] = action->getRuntimeParams().caption;
    contextMap[tpDescription] = action->getRuntimeParams().description;
    vms::event::StringsHelper helper(commonModule());
    contextMap[tpSource] = helper.getResoureNameFromParams(action->getRuntimeParams(), Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(action->getRuntimeParams());

    QString messageBody;
    if (isHtml)
        renderTemplateFromFile(attachmentData.templatePath, contextMap, &messageBody);

    QString messagePlainBody;
    QFileInfo fileInfo(attachmentData.templatePath);
    QString plainTemplatePath = fileInfo.dir().path() + lit("/") + fileInfo.baseName() + lit("_plain.mustache");
    renderTemplateFromFile(plainTemplatePath, contextMap, &messagePlainBody);


    // TODO: #vkutin #gdm Need to refactor aggregation entirely.
    // I can't figure a proper abstraction for it at this point.
    const int aggregatedCount = action->getRuntimeParams().eventType == vms::event::softwareTriggerEvent
        ? action->aggregationInfo().toList().count()
        : aggregatedResCount;

    const auto subject = aggregatedCount > 1
        ? helper.eventAtResources(action->getRuntimeParams())
        : helper.eventAtResource(action->getRuntimeParams(), Qn::RI_WithUrl);

    ec2::ApiEmailData data(
        recipients,
        subject,
        messageBody,
        messagePlainBody,
        emailSettings.timeout,
        attachments);

    if (!m_emailManager->sendEmail(emailSettings, data))
    {
        vms::event::AbstractActionPtr action(new vms::event::SystemHealthAction(QnSystemHealth::EmailSendError));
        broadcastAction(action);
        NX_LOG(lit("Error processing action SendMail."), cl_logWARNING);
    }
}

bool ExtendedRuleProcessor::sendMail(const vms::event::SendMailActionPtr& action)
{
    // QnMutexLocker lk(&m_mutex); <- m_mutex is already locked down the stack.

    if( action->getRuntimeParams().eventType != vms::event::cameraDisconnectEvent &&
        action->getRuntimeParams().eventType != vms::event::networkIssueEvent )
    {
        return sendMailInternal(action, 1);
    }

    // Aggregating by recipients and event type.

    SendEmailAggregationKey aggregationKey(action->getRuntimeParams().eventType,
        action->getParams().emailAddress); //< all recipients are already computed and packed here.

    SendEmailAggregationData& aggregatedData = m_aggregatedEmails[aggregationKey];

    vms::event::AggregationInfo aggregationInfo = aggregatedData.action
        ? aggregatedData.action->aggregationInfo() //< adding event source (camera) to the existing aggregation info.
        : vms::event::AggregationInfo();           //< creating new aggregation info.

    if (!aggregatedData.action)
    {
        aggregatedData.action = vms::event::SendMailActionPtr(new vms::event::SendMailAction(*action));
        using namespace std::placeholders;
        aggregatedData.periodicTaskID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&ExtendedRuleProcessor::sendAggregationEmail, this, aggregationKey),
            std::chrono::milliseconds(emailAggregationPeriodMS));
    }

    ++aggregatedData.eventCount;

    aggregationInfo.append(action->getRuntimeParams(), action->aggregationInfo());
    aggregatedData.action->setAggregationInfo(aggregationInfo);

    return true;
}

void ExtendedRuleProcessor::sendAggregationEmail(const SendEmailAggregationKey& aggregationKey)
{
    QnMutexLocker lk(&m_mutex);

    auto aggregatedActionIter = m_aggregatedEmails.find(aggregationKey);
    if (aggregatedActionIter == m_aggregatedEmails.end())
        return;

    if (!sendMailInternal(aggregatedActionIter->action, aggregatedActionIter->eventCount))
    {
        NX_LOG(lit("Failed to send aggregated email"), cl_logDEBUG1);
    }

    m_aggregatedEmails.erase(aggregatedActionIter);
}

QVariantMap ExtendedRuleProcessor::eventDescriptionMap(
    const vms::event::AbstractActionPtr& action,
    const vms::event::AggregationInfo &aggregationInfo,
    QnEmailAttachmentList& attachments) const
{
    vms::event::EventParameters params = action->getRuntimeParams();
    vms::event::EventType eventType = params.eventType;

    QVariantMap contextMap;
    vms::event::StringsHelper helper(commonModule());

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    const int deviceCount = aggregationInfo.toList().size();
    contextMap[tpEvent] = helper.eventName(eventType, qMax(1, deviceCount));
    contextMap[tpSource] = helper.getResoureNameFromParams(params, Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(params);

    switch (eventType)
    {
        case vms::event::cameraMotionEvent:
        case vms::event::cameraInputEvent:
        {
            auto camRes = resourcePool()->getResourceById<QnVirtualCameraResource>(action->getRuntimeParams().eventResourceId);
            cameraHistoryPool()->updateCameraHistorySync(camRes);
            if (camRes->hasVideo(nullptr))
            {
                QByteArray screenshotData = getEventScreenshotEncoded(action->getRuntimeParams().eventResourceId, action->getRuntimeParams().eventTimestampUsec, SCREENSHOT_SIZE);
                if (!screenshotData.isNull())
                {
                    contextMap[tpUrlInt] = helper.urlForCamera(params.eventResourceId, params.eventTimestampUsec, false);
                    contextMap[tpUrlExt] = helper.urlForCamera(params.eventResourceId, params.eventTimestampUsec, true);

                    QBuffer screenshotStream(&screenshotData);
                    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpScreenshot, screenshotStream, lit("image/jpeg"))));
                    contextMap[tpScreenshotFilename] = lit("cid:") + tpScreenshot;
                }
            }

            break;
        }

        case vms::event::softwareTriggerEvent:
        {
            contextMap[tpTriggerName] = params.caption.trimmed().isEmpty()
                ? helper.defaultSoftwareTriggerName()
                : params.caption;

            const auto aggregationCount = action->getAggregationCount();

            if (aggregationCount > 1)
                contextMap[tpCount] = QString::number(aggregationCount);

            contextMap[tpTimestamp] = helper.eventTimestampShort(params, aggregationCount);
            contextMap[tpTimestampDate] = helper.eventTimestampDate(params);
            contextMap[tpTimestampTime] = helper.eventTimestampTime(params);

            NX_ASSERT(params.metadata.instigators.size() == 1);
            if (params.metadata.instigators.empty())
                break;

            const auto& userId = params.metadata.instigators[0];
            const auto user = resourcePool()->getResourceById(userId);
            NX_ASSERT(user, Q_FUNC_INFO, "Unknown user id as soft trigger instigator");

            contextMap[tpUser] = user ? user->getName() : userId.toString();
            break;
        }

        case vms::event::analyticsSdkEvent:
        {
            contextMap[tpAnalyticsSdkEventType] = helper.getAnalyticsSdkEventName(params);

            const auto aggregationCount = action->getAggregationCount();

            if (aggregationCount > 1)
                contextMap[tpCount] = QString::number(aggregationCount);

            contextMap[tpTimestamp] = helper.eventTimestampShort(params, aggregationCount);
            contextMap[tpTimestampDate] = helper.eventTimestampDate(params);
            contextMap[tpTimestampTime] = helper.eventTimestampTime(params);

            auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(
                params.eventResourceId);

            cameraHistoryPool()->updateCameraHistorySync(camera);
            if (camera->hasVideo(nullptr))
            {
                QByteArray screenshotData = getEventScreenshotEncoded(params.eventResourceId,
                    params.eventTimestampUsec, SCREENSHOT_SIZE);
                if (!screenshotData.isNull())
                {
                    contextMap[tpUrlInt] = helper.urlForCamera(params.eventResourceId,
                        params.eventTimestampUsec, false);
                    contextMap[tpUrlExt] = helper.urlForCamera(params.eventResourceId,
                        params.eventTimestampUsec, true);

                    QBuffer screenshotStream(&screenshotData);
                    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpScreenshot,
                        screenshotStream, lit("image/jpeg"))));
                    contextMap[tpScreenshotFilename] = lit("cid:") + tpScreenshot;
                }
            }

            break;
        }

        case vms::event::userDefinedEvent:
        {
            auto metadata = action->getRuntimeParams().metadata;
            if (!metadata.cameraRefs.empty())
            {
                QVariantList cameras;
                int screenshotNum = 1;
                for (const QnUuid& cameraId: metadata.cameraRefs)
                {
                    if (QnVirtualCameraResourcePtr camRes = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId))
                    {
                        QVariantMap camera;

                        QnResourceDisplayInfo camInfo(camRes);
                        camera[tpCameraName] = camInfo.name();
                        camera[tpCameraIP] = camInfo.host();

                        cameraHistoryPool()->updateCameraHistorySync(camRes);
                        camera[tpUrlInt] = helper.urlForCamera(cameraId, params.eventTimestampUsec, false);
                        camera[tpUrlExt] = helper.urlForCamera(cameraId, params.eventTimestampUsec, true);

                        QByteArray screenshotData = getEventScreenshotEncoded(cameraId, params.eventTimestampUsec, SCREENSHOT_SIZE);
                        if (!screenshotData.isNull()) {
                            QBuffer screenshotStream(&screenshotData);
                            attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpScreenshotNum.arg(screenshotNum), screenshotStream, lit("image/jpeg"))));
                            camera[QLatin1String("screenshot")] = lit("cid:") + tpScreenshotNum.arg(screenshotNum++);
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
    vms::event::StringsHelper helper(commonModule());

    QVariantMap detailsMap;

    if (addSubAggregationData)
    {
        const vms::event::AggregationInfo& subAggregationData = aggregationData.subAggregationData();
        detailsMap[tpAggregated] = !subAggregationData.isEmpty()
            ? aggregatedEventDetailsMap(action, subAggregationData, detailLevel)
            : (QVariantList() << eventDetailsMap(action, aggregationData, detailLevel, false));
    }

    detailsMap[tpCount] = QString::number(aggregationCount);
    detailsMap[tpTimestamp] = helper.eventTimestampShort(params, aggregationCount);
    detailsMap[tpTimestampDate] = helper.eventTimestampDate(params);
    detailsMap[tpTimestampTime] = helper.eventTimestampTime(params);

    switch (params.eventType)
    {
        case vms::event::cameraDisconnectEvent:
        case vms::event::softwareTriggerEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            break;
        }

        case vms::event::cameraInputEvent:
        {
            detailsMap[tpInputPort] = params.inputPortId;
            break;
        }

        case vms::event::networkIssueEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            detailsMap[tpReason] = helper.eventReason(params);
            break;
        }

        case vms::event::storageFailureEvent:
        case vms::event::serverFailureEvent:
        case vms::event::licenseIssueEvent:
        case vms::event::backupFinishedEvent:
        {
            detailsMap[tpReason] = helper.eventReason(params);

            // Fill event-specific reason context here
            QVariantMap reasonContext;

            if (params.reasonCode == vms::event::EventReason::licenseRemoved)
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

        case vms::event::cameraIpConflictEvent:
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

        case vms::event::serverConflictEvent:
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
} // namespace mediaserver
} // namespace nx
