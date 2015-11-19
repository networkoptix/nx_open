#include "mserver_business_rule_processor.h"

#include <QtCore/QList>

#include "api/app_server_connection.h"

#include "business/actions/panic_business_action.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>

#include <database/server_db.h>

#include "camera/camera_pool.h"
#include "decoders/video/ffmpeg.h"

#include <recorder/recording_manager.h>
#include <recorder/storage_manager.h>

#include <media_server/serverutil.h>

#include <utils/math/math.h>
#include <utils/common/log.h>
#include "camera/get_image_helper.h"
#include "core/resource_management/resource_properties.h"

#include <QtConcurrent>
#include <utils/email/email.h>
#include <nxemail/email_manager_impl.h>
#include "nx_ec/data/api_email_data.h"
#include <utils/common/timermanager.h>
#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <nxemail/mustache/mustache_helper.h>
#include "business/business_strings_helper.h"
#include <business/actions/system_health_business_action.h>
#include "core/resource/resource_name.h"
#include "business/events/mserver_conflict_business_event.h"
#include "core/resource/camera_history.h"
#include <utils/common/synctime.h>

namespace {
    const QString tpProductLogoFilename(lit("productLogoFilename"));
    const QString tpEventLogoFilename(lit("eventLogoFilename"));
    const QString tpProductLogo(lit("productLogo.png"));
    const QString tpCompanyName(lit("companyName"));
    const QString tpCompanyUrl(lit("companyUrl"));
    const QString tpSupportLink(lit("supportLink"));
    const QString tpSupportLinkText(lit("supportLinkText"));
    const QString tpSystemName(lit("systemName"));
    const QString tpImageMimeType(lit("image/png"));
    const QString tpScreenshotFilename(lit("screenshot"));
    const QString tpScreenshot(lit("screenshot.jpeg"));
    const QString tpScreenshotNum(lit("screenshot%1.jpeg"));

    static const QString tpProductName(lit("productName"));
    static const QString tpEvent(lit("event"));
    static const QString tpSource(lit("source"));
    static const QString tpUrlInt(lit("urlint"));
    static const QString tpUrlExt(lit("urlext"));
    static const QString tpTimestamp(lit("timestamp"));
    static const QString tpReason(lit("reason"));
    static const QString tpAggregated(lit("aggregated"));
    static const QString tpInputPort(lit("inputPort"));
    static const QString tpHasCameras(lit("hasCameras"));
    static const QString tpCameras(lit("cameras"));

    static const QString tpCaption(lit("caption"));
    static const QString tpDescription(lit("description"));

    static const QSize SCREENSHOT_SIZE(640, 480);
    static const unsigned int MS_PER_SEC = 1000;
    static const unsigned int emailAggregationPeriodMS = 30 * MS_PER_SEC;
};

struct QnEmailAttachmentData {

    QnEmailAttachmentData(QnBusiness::EventType eventType) {
        switch (eventType) {
        case QnBusiness::CameraMotionEvent:
            templatePath = lit(":/email_templates/camera_motion.mustache");
            imageName = lit("camera.png");
            imagePath = lit(":/skin/email_attachments/camera.png");
            break;
        case QnBusiness::CameraInputEvent:
            templatePath = lit(":/email_templates/camera_input.mustache");
            imageName = lit("camera.png");
            imagePath = lit(":/skin/email_attachments/camera.png");
            break;
        case QnBusiness::CameraDisconnectEvent:
            templatePath = lit(":/email_templates/camera_disconnect.mustache");
            imageName = lit("camera.png");
            imagePath = lit(":/skin/email_attachments/camera.png");
            break;
        case QnBusiness::StorageFailureEvent:
            templatePath = lit(":/email_templates/storage_failure.mustache");
            imageName = lit("storage.png");
            imagePath = lit(":/skin/email_attachments/storage.png");
            break;
        case QnBusiness::NetworkIssueEvent:
            templatePath = lit(":/email_templates/network_issue.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::CameraIpConflictEvent:
            templatePath = lit(":/email_templates/camera_ip_conflict.mustache");
            imageName = lit("camera.png");
            imagePath = lit(":/skin/email_attachments/camera.png");
            break;
        case QnBusiness::ServerFailureEvent:
            templatePath = lit(":/email_templates/mediaserver_failure.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::ServerConflictEvent:
            templatePath = lit(":/email_templates/mediaserver_conflict.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::ServerStartEvent:
            templatePath = lit(":/email_templates/mediaserver_started.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::LicenseIssueEvent:
            templatePath = lit(":/email_templates/license_issue.mustache");
            imageName = lit("license.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::BackupFinishedEvent:
            templatePath = lit(":/email_templates/backup_finished.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        case QnBusiness::UserDefinedEvent:
            templatePath = lit(":/email_templates/generic_event.mustache");
            imageName = lit("server.png");
            imagePath = lit(":/skin/email_attachments/server.png");
            break;
        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "All cases must be implemented.");
            break;
        }

        Q_ASSERT_X(!templatePath.isEmpty() && !imageName.isEmpty() && !imagePath.isEmpty(), Q_FUNC_INFO, "Template path must be filled");
    }

    QString templatePath;
    QString imageName;
    QString imagePath;
};

QnMServerBusinessRuleProcessor::QnMServerBusinessRuleProcessor(): 
    QnBusinessRuleProcessor(),
    m_emailManager(new EmailManagerImpl())
{
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(onRemoveResource(QnResourcePtr)), Qt::QueuedConnection);
}

QnMServerBusinessRuleProcessor::~QnMServerBusinessRuleProcessor()
{
    quit();
    wait();

    QnMutexLocker lk( &m_mutex );
    while( !m_aggregatedEmails.isEmpty() )
    {
        const quint64 taskID = m_aggregatedEmails.begin()->periodicTaskID;
        lk.unlock();
        TimerManager::instance()->joinAndDeleteTimer( taskID );
        lk.relock();
        if( m_aggregatedEmails.begin()->periodicTaskID == taskID )  //task has not been removed in sendAggregationEmail while we were waiting
            m_aggregatedEmails.erase( m_aggregatedEmails.begin() );
    }
}

void QnMServerBusinessRuleProcessor::onRemoveResource(const QnResourcePtr &resource)
{
    qnServerDb->removeLogForRes(resource->getId());
}

bool QnMServerBusinessRuleProcessor::executeActionInternal(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    bool result = QnBusinessRuleProcessor::executeActionInternal(action, res);
    if (!result) {
        switch(action->actionType())
        {
        case QnBusiness::SendMailAction:
            return sendMail( action.dynamicCast<QnSendMailBusinessAction>() );
        case QnBusiness::BookmarkAction:
            result = executeBookmarkAction(action, res);
            break;
        case QnBusiness::CameraOutputAction:
        case QnBusiness::CameraOutputOnceAction:
            result = triggerCameraOutput(action.dynamicCast<QnCameraOutputBusinessAction>(), res);
            break;
        case QnBusiness::CameraRecordingAction:
            result = executeRecordingAction(action.dynamicCast<QnRecordingBusinessAction>(), res);
            break;
        case QnBusiness::PanicRecordingAction:
            result = executePanicAction(action.dynamicCast<QnPanicBusinessAction>());
            break;
        default:
            break;
        }
    }
    
    if (result)
        qnServerDb->saveActionToDB(action, res);

    return result;
}

bool QnMServerBusinessRuleProcessor::executePanicAction(const QnPanicBusinessActionPtr& action)
{
    const QnResourcePtr& mediaServerRes = qnResPool->getResourceById(serverGuid());
    QnMediaServerResource* mediaServer = dynamic_cast<QnMediaServerResource*> (mediaServerRes.data());
    if (!mediaServer)
        return false;
    if (mediaServer->getPanicMode() == Qn::PM_User)
        return true; // ignore panic business action if panic mode turn on by user
    
    Qn::PanicMode val = Qn::PM_None;
    if (action->getToggleState() == QnBusiness::ActiveState)
        val =  Qn::PM_BusinessEvents;
    mediaServer->setPanicMode(val);
    propertyDictionary->saveParams(mediaServer->getId());
    return true;
}

bool QnMServerBusinessRuleProcessor::executeRecordingAction(const QnRecordingBusinessActionPtr& action, const QnResourcePtr& res)
{
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = res.dynamicCast<QnSecurityCamResource>();
    //Q_ASSERT(camera);
    bool rez = false;
    if (camera) {
        // todo: if camera is offline function return false. Need some tries on timer event
        if (action->getToggleState() == QnBusiness::ActiveState)
            rez = qnRecordingManager->startForcedRecording(
                camera,
                action->getStreamQuality(), action->getFps(), 
                0, /* Record-before setup is forbidden */
                action->getRecordAfter(), 
                action->getRecordDuration());
        else
            rez = qnRecordingManager->stopForcedRecording(camera);
    }
    return rez;
}

bool QnMServerBusinessRuleProcessor::executeBookmarkAction(const QnAbstractBusinessActionPtr &action, const QnResourcePtr &resource) {
    Q_ASSERT(action);
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return false;

    int fixedDurationMs = action->getParams().durationMs;

    auto runningKey = guidFromArbitraryData(action->getBusinessRuleId().toRfc4122() + camera->getId().toRfc4122());

    qint64 startTimeMs = action->getRuntimeParams().eventTimestampUsec / 1000;
    qint64 endTimeMs = startTimeMs;

    if (fixedDurationMs <= 0) 
    {
        // bookmark as an prolonged action
        if (action->getToggleState() == QnBusiness::ActiveState) {
            m_runningBookmarkActions[runningKey] = startTimeMs;
            return true;
        }

        if (!m_runningBookmarkActions.contains(runningKey))
            return false;

        startTimeMs = m_runningBookmarkActions.take(runningKey);
    }

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.startTimeMs = startTimeMs;
    bookmark.durationMs = fixedDurationMs > 0 ? fixedDurationMs : endTimeMs - startTimeMs;
    bookmark.cameraId = camera->getUniqueId();
    bookmark.name = QnBusinessStringsHelper::eventAtResource(action->getRuntimeParams(), true);
    bookmark.description = QnBusinessStringsHelper::eventDetails(action->getRuntimeParams(), lit("\n"));
    bookmark.tags = action->getParams().tags.split(L',', QString::SkipEmptyParts).toSet();

    return qnServerDb->addBookmark(bookmark);
}


QnUuid QnMServerBusinessRuleProcessor::getGuid() const {
    return serverGuid();
}

bool QnMServerBusinessRuleProcessor::triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, const QnResourcePtr& resource )
{
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
    //if( relayOutputId.isEmpty() )
    //{
    //    NX_LOG( lit("Received BA_CameraOutput action without required parameter relayOutputID. Ignoring..."), cl_logWARNING );
    //    return false;
    //}

    //bool instant = action->actionType() == QnBusiness::CameraOutputOnceAction;

    //int autoResetTimeout = instant
    //        ? ( action->getRelayAutoResetTimeout() ? action->getRelayAutoResetTimeout() : 30*1000)
    //        : qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    int autoResetTimeout = qMax(action->getRelayAutoResetTimeout(), 0); //truncating negative values to avoid glitches
    bool on = action->getToggleState() != QnBusiness::InactiveState;

    return securityCam->setRelayOutputState(
                relayOutputId,
                on,
                autoResetTimeout );
}

QByteArray QnMServerBusinessRuleProcessor::getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize)
{
    const QnResourcePtr& cameraRes = qnResPool->getResourceById(id);
    QSharedPointer<CLVideoDecoderOutput> frame = QnGetImageHelper::getImage(cameraRes.dynamicCast<QnVirtualCameraResource>(), timestampUsec, dstSize);
    return frame ? QnGetImageHelper::encodeImage(frame, "jpg") : QByteArray();
}

bool QnMServerBusinessRuleProcessor::sendMailInternal( const QnSendMailBusinessActionPtr& action, int aggregatedResCount )
{
    Q_ASSERT( action );

    QStringList log;
    QStringList recipients;
    for (const QnUserResourcePtr &user: qnResPool->getResources<QnUserResource>(action->getResources())) {
        QString email = user->getEmail();
        log << QString(QLatin1String("%1 <%2>")).arg(user->getName()).arg(user->getEmail());
        if (!email.isEmpty() && QnEmailAddress::isValid(email))
            recipients << email;
    }

    QStringList additional = action->getParams().emailAddress.split(QLatin1Char(';'), QString::SkipEmptyParts);
    for(const QString &email: additional) {
        log << email;
        QString trimmed = email.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (QnEmailAddress::isValid(trimmed))
            recipients << email;
    }

    if( recipients.isEmpty() )
    {
        NX_LOG( lit("Action SendMail (rule %1) missing valid recipients. Ignoring...").arg(action->getBusinessRuleId().toString()), cl_logWARNING );
        NX_LOG( lit("All recipients: ") + log.join(QLatin1String("; ")), cl_logWARNING );
        return false;
    }

    NX_LOG( lit("Processing action SendMail. Sending mail to %1").
        arg(recipients.join(QLatin1String("; "))), cl_logDEBUG1 );

    QtConcurrent::run(std::bind(&QnMServerBusinessRuleProcessor::sendEmailAsync, this, action, recipients, aggregatedResCount));

    /*
     * This action instance is not used anymore but storing into the Events Log db.
     * Therefore we are storing all used emails in order to not recalculate them in
     * the event log processing methods. --rvasilenko
     */
    action->getParams().emailAddress = formatEmailList(recipients);
    return true;
}

void QnMServerBusinessRuleProcessor::sendEmailAsync(QnSendMailBusinessActionPtr action, QStringList recipients, int aggregatedResCount)
{
    QnEmailAttachmentList attachments;
    QVariantHash contextMap = eventDescriptionMap(action, action->aggregationInfo(), attachments, true);
    QnEmailAttachmentData attachmentData(action->getRuntimeParams().eventType);

    QnEmailSettings emailSettings = QnGlobalSettings::instance()->emailSettings();

    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpProductLogo, lit(":/skin/email_attachments/productLogo.png"), tpImageMimeType)));
    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(attachmentData.imageName, attachmentData.imagePath, tpImageMimeType)));
    contextMap[tpProductLogoFilename] = lit("cid:") + tpProductLogo;
    contextMap[tpEventLogoFilename] = lit("cid:") + attachmentData.imageName;
    contextMap[tpCompanyName] = QnAppInfo::organizationName();
    contextMap[tpCompanyUrl] = QnAppInfo::companyUrl();
    contextMap[tpSupportLink] = QnEmailAddress::isValid(emailSettings.supportEmail)
        ? lit("mailto:%1").arg(emailSettings.supportEmail)
        : emailSettings.supportEmail;
    contextMap[tpSupportLinkText] = emailSettings.supportEmail;
    contextMap[tpSystemName] = emailSettings.signature;

    contextMap[tpCaption] = action->getRuntimeParams().caption;
    contextMap[tpDescription] = action->getRuntimeParams().description;
    contextMap[tpSource] = action->getRuntimeParams().resourceName;

    QString messageBody = renderTemplateFromFile(attachmentData.templatePath, contextMap);

    ec2::ApiEmailData data(
        recipients,
        aggregatedResCount > 1
        ? QnBusinessStringsHelper::eventAtResources(action->getRuntimeParams(), aggregatedResCount)
        : QnBusinessStringsHelper::eventAtResource(action->getRuntimeParams(), true),
        messageBody,
        emailSettings.timeout,
        attachments
        );

    if (!m_emailManager->sendEmail(data))
    {
        QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::EmailSendError));
        broadcastBusinessAction(action);
        NX_LOG(lit("Error processing action SendMail."), cl_logWARNING);
    }
}

bool QnMServerBusinessRuleProcessor::sendMail(const QnSendMailBusinessActionPtr& action )
{
    //QnMutexLocker lk( &m_mutex );  m_mutex is locked down the stack

    //aggregating by recipients and eventtype
    if( action->getRuntimeParams().eventType != QnBusiness::CameraDisconnectEvent &&
        action->getRuntimeParams().eventType != QnBusiness::NetworkIssueEvent )
    {
        return sendMailInternal( action, 1 );  //currently, aggregating only cameraDisconnected and networkIssue events
    }

    QStringList recipients;
    for (const QnUserResourcePtr &user: qnResPool->getResources<QnUserResource>(action->getResources())) {
        QString email = user->getEmail();
        if (!email.isEmpty() && QnEmailAddress::isValid(email))
            recipients << email;
    }

    SendEmailAggregationKey aggregationKey( action->getRuntimeParams().eventType, recipients.join(';') );
    SendEmailAggregationData& aggregatedData = m_aggregatedEmails[aggregationKey];

    QnBusinessAggregationInfo aggregationInfo = aggregatedData.action
        ? aggregatedData.action->aggregationInfo()  //adding event source (camera) to the existing aggregation info
        : QnBusinessAggregationInfo();              //creating new aggregation info

    if( !aggregatedData.action )
    {
        aggregatedData.action = QnSendMailBusinessActionPtr( new QnSendMailBusinessAction( *action ) );
        using namespace std::placeholders;
        aggregatedData.periodicTaskID = TimerManager::instance()->addTimer(
            std::bind(&QnMServerBusinessRuleProcessor::sendAggregationEmail, this, aggregationKey),
            emailAggregationPeriodMS );
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

QVariantHash QnMServerBusinessRuleProcessor::eventDescriptionMap(const QnAbstractBusinessActionPtr& action, 
                                                                 const QnBusinessAggregationInfo &aggregationInfo, 
                                                                 QnEmailAttachmentList& attachments, 
                                                                 bool useIp)
{
    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    QVariantHash contextMap;

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    contextMap[tpEvent] = QnBusinessStringsHelper::eventName(eventType);
    contextMap[tpSource] = getFullResourceName(QnBusinessStringsHelper::eventSource(params), useIp);
    if (eventType == QnBusiness::CameraMotionEvent) 
    {
        auto camRes = qnResPool->getResourceById<QnVirtualCameraResource>( action->getRuntimeParams().eventResourceId);
        qnCameraHistoryPool->updateCameraHistorySync(camRes);

        contextMap[tpUrlInt] = QnBusinessStringsHelper::urlForCamera(params.eventResourceId, params.eventTimestampUsec, false);
        contextMap[tpUrlExt] = QnBusinessStringsHelper::urlForCamera(params.eventResourceId, params.eventTimestampUsec, true);

        QByteArray screenshotData = getEventScreenshotEncoded(action->getRuntimeParams().eventResourceId, action->getRuntimeParams().eventTimestampUsec, SCREENSHOT_SIZE);
        if (!screenshotData.isNull()) {
            QBuffer screenshotStream(&screenshotData);
            attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpScreenshot, screenshotStream, lit("image/jpeg"))));
            contextMap[tpScreenshotFilename] = lit("cid:") + tpScreenshot;
        }

    }
    else if (eventType == QnBusiness::UserDefinedEvent) 
    {
        auto metadata = action->getRuntimeParams().metadata;
        if (!metadata.cameraRefs.empty()) 
        {
            QVariantList cameras;
            int screenshotNum = 1;
            for (const QnUuid& cameraId: metadata.cameraRefs)
            {
                if (QnVirtualCameraResourcePtr camRes = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId))
                {
                    QVariantMap camera;

                    camera[QLatin1String("name")] = getFullResourceName(camRes, useIp);

                    qnCameraHistoryPool->updateCameraHistorySync(camRes);
                    camera[tpUrlInt] = QnBusinessStringsHelper::urlForCamera(cameraId, params.eventTimestampUsec, false);
                    camera[tpUrlExt] = QnBusinessStringsHelper::urlForCamera(cameraId, params.eventTimestampUsec, true);

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
    }

    contextMap[tpAggregated] = aggregatedEventDetailsMap(action, aggregationInfo, useIp);

    return contextMap;
}

QString QnMServerBusinessRuleProcessor::formatEmailList(const QStringList &value) const
{
    QString result;
    for (int i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            result.append(L' ');
        result.append(QString(QLatin1String("%1")).arg(value[i].trimmed()));
    }
    return result;
}

QVariantList QnMServerBusinessRuleProcessor::aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
                                                                const QnBusinessAggregationInfo& aggregationInfo,
                                                                bool useIp) 
{
    QVariantList result;
    if (aggregationInfo.isEmpty()) {
        result << eventDetailsMap(action, QnInfoDetail(action->getRuntimeParams(), action->getAggregationCount()), useIp);
    }

    for (const QnInfoDetail& detail: aggregationInfo.toList()) {
        result << eventDetailsMap(action, detail, useIp);
    }
    return result;
}

QVariantList QnMServerBusinessRuleProcessor::aggregatedEventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QList<QnInfoDetail>& aggregationDetailList,
    bool useIp )
{
    QVariantList result;
    for (const QnInfoDetail& detail: aggregationDetailList)
        result << eventDetailsMap(action, detail, useIp);
    return result;
}


QVariantHash QnMServerBusinessRuleProcessor::eventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QnInfoDetail& aggregationData,
    bool useIp,
    bool addSubAggregationData )
{
    using namespace QnBusiness;

    const QnBusinessEventParameters& params = aggregationData.runtimeParams();
    const int aggregationCount = aggregationData.count();

    QVariantHash detailsMap;

    if( addSubAggregationData )
    {
        const QnBusinessAggregationInfo& subAggregationData = aggregationData.subAggregationData();
        detailsMap[tpAggregated] = !subAggregationData.isEmpty()
            ? aggregatedEventDetailsMap(action, subAggregationData, useIp)
            : (QVariantList() << eventDetailsMap(action, aggregationData, useIp, false));
    }

    detailsMap[tpTimestamp] = QnBusinessStringsHelper::eventTimestampShort(params, aggregationCount);

    switch (params.eventType) {
    case CameraDisconnectEvent: {
        detailsMap[tpSource] = getFullResourceName(QnBusinessStringsHelper::eventSource(params), useIp);
        break;
                                }

    case CameraInputEvent: {
        detailsMap[tpInputPort] = params.inputPortId;
        break;
                           }

    case NetworkIssueEvent:
        {
            detailsMap[tpSource] = getFullResourceName(QnBusinessStringsHelper::eventSource(params), useIp);
            detailsMap[tpReason] = QnBusinessStringsHelper::eventReason(params);
            break;
        }
    case StorageFailureEvent:
    case ServerFailureEvent: 
    case LicenseIssueEvent:
    case BackupFinishedEvent:
        {
            detailsMap[tpReason] = QnBusinessStringsHelper::eventReason(params);
            break;
        }
    case CameraIpConflictEvent: {
        detailsMap[lit("cameraConflictAddress")] = params.caption;
        QVariantList conflicts;
        int n = 0;
        foreach (const QString& mac, params.description.split(QnConflictBusinessEvent::Delimiter)) {
            QVariantHash conflict;
            conflict[lit("number")] = ++n;
            conflict[lit("mac")] = mac;
            conflicts << conflict;
        }
        detailsMap[lit("cameraConflicts")] = conflicts;

        break;
                                }
    case ServerConflictEvent: {
        QnCameraConflictList conflicts;
        conflicts.sourceServer = params.caption;
        conflicts.decode(params.description);

        QVariantList conflictsList;
        int n = 0;
        for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr) {
            const QString &server = itr.key();
            foreach (const QString &camera, conflicts.camerasByServer[server]) {
                QVariantHash conflict;
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
