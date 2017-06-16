#include "mserver_business_rule_processor.h"

#include <map>

#include <QtCore/QList>

#include "api/app_server_connection.h"

#include "business/actions/panic_business_action.h"

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
#include "camera/get_image_helper.h"

#include <QtConcurrent/QtConcurrent>
#include <utils/email/email.h>
#include <nx/email/email_manager_impl.h>
#include "nx_ec/data/api_email_data.h"
#include <nx/utils/timer_manager.h>
#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <nx/email/mustache/mustache_helper.h>
#include "business/business_strings_helper.h"
#include <business/actions/system_health_business_action.h>
#include "business/events/mserver_conflict_business_event.h"

#include <common/common_module.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include "utils/common/delayed.h"
#include <business/business_event_connector.h>

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

struct QnEmailAttachmentData
{
    QnEmailAttachmentData(QnBusiness::EventType eventType)
    {
        switch (eventType)
        {
            case QnBusiness::CameraMotionEvent:
                templatePath = lit(":/email_templates/camera_motion.mustache");
                imageName = lit("camera.png");
                break;
            case QnBusiness::CameraInputEvent:
                templatePath = lit(":/email_templates/camera_input.mustache");
                imageName = lit("camera.png");
                break;
            case QnBusiness::CameraDisconnectEvent:
                templatePath = lit(":/email_templates/camera_disconnect.mustache");
                imageName = lit("camera.png");
                break;
            case QnBusiness::StorageFailureEvent:
                templatePath = lit(":/email_templates/storage_failure.mustache");
                imageName = lit("storage.png");
                break;
            case QnBusiness::NetworkIssueEvent:
                templatePath = lit(":/email_templates/network_issue.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::CameraIpConflictEvent:
                templatePath = lit(":/email_templates/camera_ip_conflict.mustache");
                imageName = lit("camera.png");
                break;
            case QnBusiness::ServerFailureEvent:
                templatePath = lit(":/email_templates/mediaserver_failure.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::ServerConflictEvent:
                templatePath = lit(":/email_templates/mediaserver_conflict.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::ServerStartEvent:
                templatePath = lit(":/email_templates/mediaserver_started.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::LicenseIssueEvent:
                templatePath = lit(":/email_templates/license_issue.mustache");
                imageName = lit("license.png");
                break;
            case QnBusiness::BackupFinishedEvent:
                templatePath = lit(":/email_templates/backup_finished.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::UserDefinedEvent:
                templatePath = lit(":/email_templates/generic_event.mustache");
                imageName = lit("server.png");
                break;
            case QnBusiness::SoftwareTriggerEvent:
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

QnMServerBusinessRuleProcessor::QnMServerBusinessRuleProcessor(QnCommonModule* commonModule):
    QnBusinessRuleProcessor(commonModule),
    m_emailManager(new EmailManagerImpl(commonModule))
{
    connect(resourcePool(), SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)), Qt::QueuedConnection);
}

QnMServerBusinessRuleProcessor::~QnMServerBusinessRuleProcessor()
{
    quit();
    wait();
    m_emailThreadPool.waitForDone();

    QnMutexLocker lk( &m_mutex );
    while( !m_aggregatedEmails.isEmpty() )
    {
        const quint64 taskID = m_aggregatedEmails.begin()->periodicTaskID;
        lk.unlock();
        nx::utils::TimerManager::instance()->joinAndDeleteTimer( taskID );
        lk.relock();
        if( m_aggregatedEmails.begin()->periodicTaskID == taskID )  //task has not been removed in sendAggregationEmail while we were waiting
            m_aggregatedEmails.erase( m_aggregatedEmails.begin() );
    }
}

void QnMServerBusinessRuleProcessor::onRemoveResource(const QnResourcePtr &resource)
{
    qnServerDb->removeLogForRes(resource->getId());
}

void QnMServerBusinessRuleProcessor::prepareAdditionActionParams(const QnAbstractBusinessActionPtr& action)
{
    switch(action->actionType())
    {
    case QnBusiness::SendMailAction:
        // Add user's email addresses to action emailAddress field and filter invalid addresses
        if (auto emailAction = action.dynamicCast<QnSendMailBusinessAction>())
            updateRecipientsList(emailAction);
        break;
    default:
        break;
    }
}

bool QnMServerBusinessRuleProcessor::executeActionInternal(const QnAbstractBusinessActionPtr& action)
{
    bool result = QnBusinessRuleProcessor::executeActionInternal(action);
    if (!result) {
        switch(action->actionType())
        {
        case QnBusiness::SendMailAction:
            result = sendMail(action.dynamicCast<QnSendMailBusinessAction>());
            break;
        case QnBusiness::BookmarkAction:
            result = executeBookmarkAction(action);
            break;
        case QnBusiness::CameraOutputAction:
            result = triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>());
            break;
        case QnBusiness::CameraRecordingAction:
            result = executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>());
            break;
        case QnBusiness::PanicRecordingAction:
            result = executePanicAction(action.dynamicCast<QnPanicBusinessAction>());
            break;
        case QnBusiness::ExecutePtzPresetAction:
            result = executePtzAction(action);
            break;
        case QnBusiness::ExecHttpRequestAction:
            result = executeHttpRequestAction(action);
            break;

        case QnBusiness::SayTextAction:
            result = executeSayTextAction(action);
            break;
        case QnBusiness::PlaySoundAction:
        case QnBusiness::PlaySoundOnceAction:
            result = executePlaySoundAction(action);
        default:
            break;
        }
    }

    if (result)
        qnServerDb->saveActionToDB(action);

    return result;
}

bool QnMServerBusinessRuleProcessor::executePlaySoundAction(const QnAbstractBusinessActionPtr &action)
{
    const auto params = action->getParams();
    const auto resource = resourcePool()->getResourceById<QnSecurityCamResource>(params.actionResourceId);

    if (!resource)
        return false;

    QnAudioTransmitterPtr transmitter;
    if (resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        transmitter = resource->getAudioTransmitter();

    if (!transmitter)
        return false;


    if (action->actionType() == QnBusiness::PlaySoundOnceAction)
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
        if (action->getToggleState() == QnBusiness::ActiveState)
        {
            provider = QnAudioStreamerPool::instance()->getActionDataProvider(action);
            transmitter->subscribe(provider, QnAbstractAudioTransmitter::kContinuousNotificationPriority);
            provider->startIfNotRunning();
        }
        else if (action->getToggleState() == QnBusiness::InactiveState)
        {
            provider = QnAudioStreamerPool::instance()->getActionDataProvider(action);
            transmitter->unsubscribe(provider.data());
            if (provider->processorsCount() == 0)
                QnAudioStreamerPool::instance()->destroyActionDataProvider(action);
        }
    }

    return true;

}

bool QnMServerBusinessRuleProcessor::executeSayTextAction(const QnAbstractBusinessActionPtr& action)
{
#if !defined(EDGE_SERVER)
    const auto params = action->getParams();
    const auto text = params.sayText;
    const auto resource = resourcePool()->getResourceById<QnSecurityCamResource>(params.actionResourceId);
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

bool QnMServerBusinessRuleProcessor::executePanicAction(const QnPanicBusinessActionPtr& action)
{
    const QnResourcePtr& mediaServerRes = resourcePool()->getResourceById(serverGuid());
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (mediaServerRes.data());
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == Qn::PM_User)
        return true; // ignore panic business action if panic mode turn on by user

    Qn::PanicMode val = Qn::PM_None;
    if (action->getToggleState() == QnBusiness::ActiveState)
        val =  Qn::PM_BusinessEvents;
    mediaServer->setPanicMode(val);
    commonModule()->propertyDictionary()->saveParams(mediaServer->getId());
    return true;
}

bool QnMServerBusinessRuleProcessor::executeHttpRequestAction(const QnAbstractBusinessActionPtr& action)
{
    QUrl url(action->getParams().url);

    if (action->getParams().text.isEmpty())
    {
        auto callback = [action](SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType messageBody)
        {
            if( osErrorCode != SystemError::noError ||
                statusCode != nx_http::StatusCode::ok )
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
            callback );
        return true;
    }
    else
    {
        auto callback = [action](SystemError::ErrorCode osErrorCode, int statusCode)
        {
            if( osErrorCode != SystemError::noError ||
                statusCode != nx_http::StatusCode::ok )
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

bool QnMServerBusinessRuleProcessor::executePtzAction(const QnAbstractBusinessActionPtr& action)
{
    auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(action->getParams().actionResourceId);
    if (!camera)
        return false;
    if (camera->getDewarpingParams().enabled)
        return broadcastBusinessAction(action); // execute action on a client side

    QnPtzControllerPtr controller = qnPtzPool->controller(camera);
    if (!controller)
        return false;
    return controller->activatePreset(action->getParams().presetId, QnAbstractPtzController::MaxPtzSpeed);
}

bool QnMServerBusinessRuleProcessor::executeRecordingAction(const QnRecordingBusinessActionPtr& action)
{
    NX_ASSERT(action);
    auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(action->getParams().actionResourceId);
    //NX_ASSERT(camera);
    bool rez = false;
    if (camera)
    {
        auto toggleState = action->getToggleState();
        // todo: if camera is offline function return false. Need some tries on timer event
        if (toggleState == QnBusiness::ActiveState || //< Prolonged actions starts
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

bool QnMServerBusinessRuleProcessor::executeBookmarkAction(
    const QnAbstractBusinessActionPtr &action)
{
    NX_ASSERT(action);
    const auto camera = resourcePool()->getResourceById<QnSecurityCamResource>(
        action->getParams().actionResourceId);
    if (!camera)
        return false;

    const auto key = action->getExternalUniqKey();
    auto runningKey = guidFromArbitraryData(key);
    if (action->getParams().durationMs <= 0)
    {
        if (action->getToggleState() == QnBusiness::ActiveState)
        {
            m_runningBookmarkActions[runningKey] = action->getRuntimeParams().eventTimestampUsec;
            return true;
        }

        if (!m_runningBookmarkActions.contains(runningKey))
            return false;

        action->getRuntimeParams().eventTimestampUsec =
            m_runningBookmarkActions.take(runningKey);
    }

    return qnServerDb->addBookmark(helpers::bookmarkFromAction(action, camera, commonModule()));
}


QnUuid QnMServerBusinessRuleProcessor::getGuid() const {
    return serverGuid();
}

bool QnMServerBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action)
{
    auto resource = resourcePool()->getResourceById(action->getParams().actionResourceId);
    if( !resource )
    {
        NX_LOG( lit("Received BA_CameraOutput with no resource reference. Ignoring..."), cl_logWARNING );
        return false;
    }
    QnSecurityCamResource* securityCam = dynamic_cast<QnSecurityCamResource*>(resource.data());
    if( !securityCam )
    {
        NX_LOG( lit("Received BA_CameraOutput action for resource %1 which is not of required type QnSecurityCamResource. Ignoring...").
            arg(resource->getId().toString()), cl_logWARNING );
        return false;
    }
    QString relayOutputId = action->getRelayOutputId();
    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = action->getToggleState() != QnBusiness::InactiveState;

    return securityCam->setRelayOutputState(
                relayOutputId,
                on,
                autoResetTimeout );
}

QByteArray QnMServerBusinessRuleProcessor::getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const
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

bool QnMServerBusinessRuleProcessor::sendMailInternal( const QnSendMailBusinessActionPtr& action, int aggregatedResCount )
{
    NX_ASSERT( action );

    QStringList recipients = action->getParams().emailAddress.split(kNewEmailDelimiter);

    if( recipients.isEmpty() )
    {
        NX_LOG( lit("Action SendMail (rule %1) missing valid recipients. Ignoring...").arg(action->getBusinessRuleId().toString()), cl_logWARNING );
        NX_LOG( lit("All recipients: ") + recipients.join(QLatin1String("; ")), cl_logWARNING );
        return false;
    }

    NX_LOG( lit("Processing action SendMail. Sending mail to %1").
        arg(recipients.join(QLatin1String("; "))), cl_logDEBUG1 );

    executeDelayed([this, action, recipients, aggregatedResCount]()
    {
        nx::utils::concurrent::run(
            &m_emailThreadPool,
            std::bind(&QnMServerBusinessRuleProcessor::sendEmailAsync, this, action, recipients, aggregatedResCount));
    }, kEmailSendDelay, qnBusinessRuleConnector->thread());

    return true;
}

void QnMServerBusinessRuleProcessor::sendEmailAsync(
    QnSendMailBusinessActionPtr action,
    QStringList recipients, int aggregatedResCount)
{
    QnEmailAttachmentList attachments;
    QVariantMap contextMap = eventDescriptionMap(action, action->aggregationInfo(), attachments);
    QnEmailAttachmentData attachmentData(action->getRuntimeParams().eventType);  //TODO: https://networkoptix.atlassian.net/browse/VMS-2831
    QnEmailSettings emailSettings = commonModule()->globalSettings()->emailSettings();
    QString cloudOwnerAccount = commonModule()->globalSettings()->cloudAccountName();

    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpProductLogo, lit(":/skin/email_attachments/productLogo.png"), tpImageMimeType)));
    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpSystemIcon, lit(":/skin/email_attachments/systemIcon.png"), tpImageMimeType)));
    contextMap[tpProductLogoFilename] = lit("cid:") + tpProductLogo;
    contextMap[tpSystemIcon] = lit("cid:") + tpSystemIcon;
    if (!cloudOwnerAccount.isEmpty())
    {
        attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpOwnerIcon, lit(":/skin/email_attachments/ownerIcon.png"), tpImageMimeType)));
        contextMap[tpOwnerIcon] = lit("cid:") + tpOwnerIcon;
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

//    contextMap[tpEventLogoFilename] = lit("cid:") + attachmentData.imageName;
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
    QnBusinessStringsHelper helper(commonModule());
    contextMap[tpSource] = helper.getResoureNameFromParams(action->getRuntimeParams(), Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(action->getRuntimeParams());

    QString messageBody;
    renderTemplateFromFile(attachmentData.templatePath, contextMap, &messageBody);

    //TODO: #vkutin #gdm Need to refactor aggregation entirely.
    // I can't figure a proper abstraction for it at this point.
    const int aggregatedCount = action->getRuntimeParams().eventType == QnBusiness::SoftwareTriggerEvent
        ? action->aggregationInfo().toList().count()
        : aggregatedResCount;

    const auto subject = aggregatedCount > 1
        ? helper.eventAtResources(action->getRuntimeParams())
        : helper.eventAtResource(action->getRuntimeParams(), Qn::RI_WithUrl);

    ec2::ApiEmailData data(
        recipients,
        subject,
        messageBody,
        emailSettings.timeout,
        attachments);

    if (!m_emailManager->sendEmail(emailSettings, data))
    {
        QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::EmailSendError));
        broadcastBusinessAction(action);
        NX_LOG(lit("Error processing action SendMail."), cl_logWARNING);
    }
}

bool QnMServerBusinessRuleProcessor::sendMail(const QnSendMailBusinessActionPtr& action )
{
    // QnMutexLocker lk(&m_mutex); <- m_mutex is already locked down the stack.

    // Currently, aggregating only cameraDisconnected and networkIssue events.
    if( action->getRuntimeParams().eventType != QnBusiness::CameraDisconnectEvent &&
        action->getRuntimeParams().eventType != QnBusiness::NetworkIssueEvent )
    {
        return sendMailInternal(action, 1);
    }

    // Aggregating by recipients and event type.

    SendEmailAggregationKey aggregationKey(action->getRuntimeParams().eventType,
        action->getParams().emailAddress); //< all recipients are already computed and packed here.

    SendEmailAggregationData& aggregatedData = m_aggregatedEmails[aggregationKey];

    QnBusinessAggregationInfo aggregationInfo = aggregatedData.action
        ? aggregatedData.action->aggregationInfo() //< adding event source (camera) to the existing aggregation info.
        : QnBusinessAggregationInfo();             //< creating new aggregation info.

    if( !aggregatedData.action )
    {
        aggregatedData.action = QnSendMailBusinessActionPtr( new QnSendMailBusinessAction( *action ) );
        using namespace std::placeholders;
        aggregatedData.periodicTaskID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&QnMServerBusinessRuleProcessor::sendAggregationEmail, this, aggregationKey),
            std::chrono::milliseconds(emailAggregationPeriodMS));
    }

    ++aggregatedData.eventCount;

    aggregationInfo.append( action->getRuntimeParams(), action->aggregationInfo() );
    aggregatedData.action->setAggregationInfo( aggregationInfo );

    return true;
}

void QnMServerBusinessRuleProcessor::sendAggregationEmail( const SendEmailAggregationKey& aggregationKey )
{
    QnMutexLocker lk( &m_mutex );

    auto aggregatedActionIter = m_aggregatedEmails.find(aggregationKey);
    if( aggregatedActionIter == m_aggregatedEmails.end() )
        return;

    if( !sendMailInternal( aggregatedActionIter->action, aggregatedActionIter->eventCount ) )
    {
        NX_LOG( lit("Failed to send aggregated email"), cl_logDEBUG1 );
    }

    m_aggregatedEmails.erase( aggregatedActionIter );
}

QVariantMap QnMServerBusinessRuleProcessor::eventDescriptionMap(
    const QnAbstractBusinessActionPtr& action,
    const QnBusinessAggregationInfo &aggregationInfo,
    QnEmailAttachmentList& attachments) const
{
    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    QVariantMap contextMap;
    QnBusinessStringsHelper helper(commonModule());

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    const int deviceCount = aggregationInfo.toList().size();
    contextMap[tpEvent] = helper.eventName(eventType, qMax(1, deviceCount));
    contextMap[tpSource] = helper.getResoureNameFromParams(params, Qn::ResourceInfoLevel::RI_NameOnly);
    contextMap[tpSourceIP] = helper.getResoureIPFromParams(params);

    switch (eventType)
    {
        case QnBusiness::CameraMotionEvent:
        case QnBusiness::CameraInputEvent:
        {
            auto camRes = resourcePool()->getResourceById<QnVirtualCameraResource>( action->getRuntimeParams().eventResourceId);
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

        case QnBusiness::SoftwareTriggerEvent:
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
            NX_ASSERT(user, Q_FUNC_INFO, "Unknown user id as software trigger instigator");

            contextMap[tpUser] = user ? user->getName() : userId.toString();
            break;
        }

        case QnBusiness::UserDefinedEvent:
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

QVariantList QnMServerBusinessRuleProcessor::aggregatedEventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QnBusinessAggregationInfo& aggregationInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    if (!aggregationInfo.isEmpty())
        return aggregatedEventDetailsMap(action, aggregationInfo.toList(), detailLevel);

    return { eventDetailsMap(action, QnInfoDetail(action->getRuntimeParams(),
        action->getAggregationCount()), detailLevel) };
}

QVariantList QnMServerBusinessRuleProcessor::aggregatedEventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QList<QnInfoDetail>& aggregationDetailList,
    Qn::ResourceInfoLevel detailLevel) const
{
    int index = 0;
    QVariantList result;

    for (const auto& detail: aggregationDetailList)
    {
        auto detailsMap = eventDetailsMap(action, detail, detailLevel);
        detailsMap[tpIndex] = (++index);
        result << eventDetailsMap(action, detail, detailLevel);
    }

    return result;
}

QVariantMap QnMServerBusinessRuleProcessor::eventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QnInfoDetail& aggregationData,
    Qn::ResourceInfoLevel detailLevel,
    bool addSubAggregationData) const
{
    using namespace QnBusiness;

    const QnBusinessEventParameters& params = aggregationData.runtimeParams();
    const int aggregationCount = aggregationData.count();
    QnBusinessStringsHelper helper(commonModule());

    QVariantMap detailsMap;

    if (addSubAggregationData)
    {
        const QnBusinessAggregationInfo& subAggregationData = aggregationData.subAggregationData();
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
        case CameraDisconnectEvent:
        case SoftwareTriggerEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            break;
        }

        case CameraInputEvent:
        {
            detailsMap[tpInputPort] = params.inputPortId;
            break;
        }

        case NetworkIssueEvent:
        {
            detailsMap[tpSource] = helper.getResoureNameFromParams(params, detailLevel);
            detailsMap[tpSourceIP] = helper.getResoureIPFromParams(params);
            detailsMap[tpReason] = helper.eventReason(params);
            break;
        }

        case StorageFailureEvent:
        case ServerFailureEvent:
        case LicenseIssueEvent:
        case BackupFinishedEvent:
        {
            detailsMap[tpReason] = helper.eventReason(params);

            // Fill event-specific reason context here
            QVariantMap reasonContext;

            if (params.reasonCode == LicenseRemoved)
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

        case CameraIpConflictEvent:
        {
            detailsMap[lit("cameraConflictAddress")] = params.caption;
            QVariantList conflicts;
            int n = 0;
            for (const auto& mac: params.description.split(QnConflictBusinessEvent::Delimiter))
            {
                QVariantMap conflict;
                conflict[lit("number")] = ++n;
                conflict[lit("mac")] = mac;
                conflicts << conflict;
            }

            detailsMap[lit("cameraConflicts")] = conflicts;
            break;
        }

        case ServerConflictEvent:
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
void QnMServerBusinessRuleProcessor::updateRecipientsList(
    const QnSendMailBusinessActionPtr& action) const
{
    QStringList additional = action->getParams().emailAddress.split(kOldEmailDelimiter,
        QString::SkipEmptyParts);

    QStringList recipients;
    auto addRecipient = [&recipients](const QString& email)
        {
            const QString simplified = email.trimmed().toLower();
            if (simplified.isEmpty()) //fast check
                return;

            QnEmailAddress address(simplified);
            if (address.isValid())
                recipients.append(address.value());
        };

    for (const auto& email: additional)
        addRecipient(email);

    const auto subjects = action->getResources().toList().toSet();

    //TODO: #vkutin Optimize?
    for (const auto& user: resourcePool()->getResources<QnUserResource>())
    {
        if (!user->isEnabled())
            continue;

        if (subjects.contains(user->getId()))
        {
            addRecipient(user->getEmail());
            continue;
        }

        const auto role = user->userRole();
        if (role == Qn::UserRole::CustomPermissions)
            continue;

        const auto roleId = role == Qn::UserRole::CustomUserRole
            ? user->userRoleId()
            : QnUserRolesManager::predefinedRoleId(role);

        if (subjects.contains(roleId))
            addRecipient(user->getEmail());
    }

    recipients.removeDuplicates();
    action->getParams().emailAddress = recipients.join(kNewEmailDelimiter);
}
