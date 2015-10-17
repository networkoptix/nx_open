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

    QnUuid ruleId = action->getBusinessRuleId();

    if (action->getToggleState() == QnBusiness::ActiveState) {
        m_runningBookmarkActions[ruleId] = QDateTime::currentMSecsSinceEpoch();
        return true;
    }

    if (!m_runningBookmarkActions.contains(ruleId))
        return false;

    qint64 startTime = m_runningBookmarkActions.take(ruleId);
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.startTimeMs = startTime;
    bookmark.durationMs = endTime - startTime;
    bookmark.cameraId = camera->getUniqueId();

    bookmark.name = "Auto-Generated Bookmark";
    bookmark.description = QString("Rule %1\nFrom %2 to %3")
        .arg(ruleId.toString())
        .arg(QDateTime::fromMSecsSinceEpoch(startTime).toString())
        .arg(QDateTime::fromMSecsSinceEpoch(endTime).toString());

    return qnServerDb->addOrUpdateCameraBookmark(bookmark);
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
    QtConcurrent::run(std::bind(&QnMServerBusinessRuleProcessor::sendEmailAsync, this, data));

    /*
     * This action instance is not used anymore but storing into the Events Log db.
     * Therefore we are storing all used emails in order to not recalculate them in
     * the event log processing methods. --rvasilenko
     */
    action->getParams().emailAddress = formatEmailList(recipients);
    return true;
}

void QnMServerBusinessRuleProcessor::sendEmailAsync(const ec2::ApiEmailData& data)
{
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

QVariantHash QnMServerBusinessRuleProcessor::eventDescriptionMap(const QnAbstractBusinessActionPtr& action, 
                                                                 const QnBusinessAggregationInfo &aggregationInfo, 
                                                                 QnEmailAttachmentList& attachments, 
                                                                 bool useIp)
QString QnMServerBusinessRuleProcessor::formatEmailList(const QStringList &value) const
{

        {
            detailsMap[tpSource] = getFullResourceName(QnBusinessStringsHelper::eventSource(params), useIp);
            detailsMap[tpReason] = QnBusinessStringsHelper::eventReason(params);
            break;
        }
    case StorageFailureEvent:
} 
