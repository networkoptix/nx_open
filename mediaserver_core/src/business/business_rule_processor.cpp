#include "business_rule_processor.h"

#include <QtCore/QList>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtConcurrent>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/global_settings.h>

#include <business/business_action_factory.h>
#include <business/business_event_rule.h>
#include <business/actions/system_health_business_action.h>
#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include "mustache/mustache_helper.h"

#include <utils/common/synctime.h>
#include <utils/common/log.h>
#include <utils/common/app_info.h>
#include <utils/common/timermanager.h>
#include <utils/email/email.h>
#include <utils/email/email_manager_impl.h>

#include "business/business_strings_helper.h"
#include "nx_ec/data/api_email_data.h"
#include "common/common_module.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
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

QnBusinessRuleProcessor* QnBusinessRuleProcessor::m_instance = 0;

QnBusinessRuleProcessor::QnBusinessRuleProcessor():
        m_emailManager(new EmailManagerImpl())
{
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDelivered, this, &QnBusinessRuleProcessor::at_actionDelivered);
    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionDeliveryFail, this, &QnBusinessRuleProcessor::at_actionDeliveryFailed);

    connect(qnResPool, &QnResourcePool::resourceAdded,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, true ); });
    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, [this](const QnResourcePtr& resource) { toggleInputPortMonitoring( resource, false ); });

    connect(qnBusinessMessageBus, &QnBusinessMessageBus::actionReceived,
        this, static_cast<void (QnBusinessRuleProcessor::*)(const QnAbstractBusinessActionPtr&)>(&QnBusinessRuleProcessor::executeAction));

    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleChanged,
            this, &QnBusinessRuleProcessor::at_businessRuleChanged);
    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleDeleted,
            this, &QnBusinessRuleProcessor::at_businessRuleDeleted);
    connect(QnCommonMessageProcessor::instance(),       &QnCommonMessageProcessor::businessRuleReset,
            this, &QnBusinessRuleProcessor::at_businessRuleReset);

    connect(&m_timer, &QTimer::timeout, this, &QnBusinessRuleProcessor::at_timer);
    m_timer.start(1000);
    start();
}

QnBusinessRuleProcessor::~QnBusinessRuleProcessor()
{
    quit();
    wait();

    QMutexLocker lk( &m_mutex );
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

QnMediaServerResourcePtr QnBusinessRuleProcessor::getDestMServer(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (action->actionType() == QnBusiness::SendMailAction) {
        // looking for server with public IP address
        const QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
        if (!mServer || (mServer->getServerFlags() & Qn::SF_HasPublicIP))
            return QnMediaServerResourcePtr(); // do not proxy
        for (const QnMediaServerResourcePtr& mServer: qnResPool->getAllServers())
        {
            if ((mServer->getServerFlags() & Qn::SF_HasPublicIP) && mServer->getStatus() == Qn::Online)
                return mServer;
        }
        return QnMediaServerResourcePtr();
    }

    if (!res)
        return QnMediaServerResourcePtr();

    if (action->actionType() == QnBusiness::DiagnosticsAction)
        return QnMediaServerResourcePtr(); // no need transfer to other mServer. Execute action here.
    if (!res)
        return QnMediaServerResourcePtr(); // can not find routeTo resource
    return qnResPool->getResourceById<QnMediaServerResource>(res->getParentId());
}

bool QnBusinessRuleProcessor::needProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    const QnMediaServerResourcePtr routeToServer = getDestMServer(action, res);
    return routeToServer && !action->isReceivedFromRemoteHost() && routeToServer->getId() != getGuid();
}

void QnBusinessRuleProcessor::doProxyAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    const QnMediaServerResourcePtr routeToServer = getDestMServer(action, res);
    if (routeToServer) 
    {
        // todo: it is better to use action.clone here
        ec2::ApiBusinessActionData actionData;
        QnAbstractBusinessActionPtr actionToSend;

        ec2::fromResourceToApi(action, actionData);
        if (res) {
            actionData.resourceIds.clear();
            actionData.resourceIds.push_back(res->getId());
        }
        ec2::fromApiToResource(actionData, actionToSend, qnResPool);

        qnBusinessMessageBus->deliveryBusinessAction(actionToSend, routeToServer->getId());
    }
}

void QnBusinessRuleProcessor::executeAction(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (needProxyAction(action, res))
        doProxyAction(action, res);
    else
        executeActionInternal(action, res);
}

void QnBusinessRuleProcessor::executeAction(const QnAbstractBusinessActionPtr& action)
{
    QnNetworkResourceList resList = qnResPool->getResources<QnNetworkResource>(action->getResources());
    if (resList.isEmpty()) {
        executeAction(action, QnResourcePtr());
    }
    else {
        for(const QnResourcePtr& res: resList)
            executeAction(action, res);
    }
}

bool QnBusinessRuleProcessor::executeActionInternal(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res)
{
    if (QnBusiness::hasToggleState(action->actionType()))
    {
        // check for duplicate actions. For example: camera start recording by 2 different events e.t.c
        QString actionKey = action->getExternalUniqKey();
        if (res)
            actionKey += QString(L'_') + res->getUniqueId();

        if (action->getToggleState() == QnBusiness::ActiveState)
        {
            if (++m_actionInProgress[actionKey] > 1)
                return true; // ignore duplicated start
        }
        else if (action->getToggleState() == QnBusiness::InactiveState)
        {
            if (--m_actionInProgress[actionKey] > 0)
                return true; // ignore duplicated stop
            m_actionInProgress.remove(actionKey);
        }
    }

    switch( action->actionType() )
    {
    case QnBusiness::SendMailAction:
        return sendMail( action.dynamicCast<QnSendMailBusinessAction>() );

    case QnBusiness::DiagnosticsAction:
        return true;

    case QnBusiness::ShowPopupAction:
    case QnBusiness::PlaySoundOnceAction:
    case QnBusiness::PlaySoundAction:
    case QnBusiness::SayTextAction:
        return broadcastBusinessAction(action);

    default:
        break;
    }

    return false;
}

class QnBusinessRuleProcessorInstanceDeleter
{
public:
    ~QnBusinessRuleProcessorInstanceDeleter()
    {
        QnBusinessRuleProcessor::fini();
    }
};

static QnBusinessRuleProcessorInstanceDeleter qnBusinessRuleProcessorInstanceDeleter;

QnBusinessRuleProcessor* QnBusinessRuleProcessor::instance()
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(m_instance, Q_FUNC_INFO, "QnBusinessRuleProcessor::init must be called first!");
    return m_instance;
}

void QnBusinessRuleProcessor::init(QnBusinessRuleProcessor* instance)
{
    // this call is not thread safe! You should init from main thread e.t.c
    Q_ASSERT_X(!m_instance, Q_FUNC_INFO, "QnBusinessRuleProcessor::init must be called once!");
    m_instance = instance;
}

void QnBusinessRuleProcessor::fini()
{
    delete m_instance;
    m_instance = NULL;
}

void QnBusinessRuleProcessor::addBusinessRule(const QnBusinessEventRulePtr& value)
{
    at_businessRuleChanged(value);
}

void QnBusinessRuleProcessor::processBusinessEvent(const QnAbstractBusinessEventPtr& bEvent)
{
    QMutexLocker lock(&m_mutex);

    QnAbstractBusinessActionList actions = matchActions(bEvent);
    for(const QnAbstractBusinessActionPtr& action: actions)
    {
        executeAction(action);
    }
}

bool QnBusinessRuleProcessor::containResource(const QnResourceList& resList, const QnUuid& resId) const
{
    for (int i = 0; i < resList.size(); ++i)
    {
        if (resList.at(i) && resList.at(i)->getId() == resId)
            return true;
    }
    return false;
}

bool QnBusinessRuleProcessor::checkRuleCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule) const
{
    if (!bEvent->checkCondition(rule->eventState(), rule->eventParams()))
        return false;
    if (!rule->isScheduleMatchTime(qnSyncTime->currentDateTime()))
        return false;
    return true;
}

QString QnBusinessRuleProcessor::formatEmailList(const QStringList &value) {
    QString result;
    for (int i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            result.append(L' ');
        result.append(QString(QLatin1String("%1")).arg(value[i].trimmed()));
    }
    return result;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processToggleAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    QnAbstractBusinessActionPtr action;

    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];

    if (bEvent->getToggleState() == QnBusiness::InactiveState && !runtimeRule.resources.isEmpty())
        return QnAbstractBusinessActionPtr(); // ignore 'Off' event if some event resources still running

    if (!runtimeRule.isActionRunning.isEmpty())
    {
        // Toggle event repeated with some interval with state 'on'.
        // if toggled action is used and condition is no longer valid - stop action
        // Or toggle event goes to 'off'. stop action
        if (!condOK || bEvent->getToggleState() == QnBusiness::InactiveState)
            action = QnBusinessActionFactory::instantiateAction(rule, bEvent, QnBusiness::InactiveState);
        else
            return QnAbstractBusinessActionPtr(); // ignore repeating 'On' event
    }
    else if (condOK)
        action = QnBusinessActionFactory::instantiateAction(rule, bEvent);

    bool isActionRunning = action && action->getToggleState() == QnBusiness::ActiveState;
    if (isActionRunning)
        runtimeRule.isActionRunning.insert(QnUuid());
    else
        runtimeRule.isActionRunning.clear();

    return action;
}

QnAbstractBusinessActionPtr QnBusinessRuleProcessor::processInstantAction(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool condOK = checkRuleCondition(bEvent, rule);
    RunningRuleMap::iterator itr = m_rulesInProgress.find(rule->getUniqueId());
    if (itr != m_rulesInProgress.end())
    {
        // instant action connected to continue event. update stats to prevent multiple action for repeation 'on' event state
        RunningRuleInfo& runtimeRule = itr.value();
        QnUuid resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnUuid();

        if (condOK && bEvent->getToggleState() == QnBusiness::ActiveState) {
            if (runtimeRule.isActionRunning.contains(resId))
                return QnAbstractBusinessActionPtr(); // action by rule already was ran. Allow separate action for each resource of source event but ingore repeated 'on' state
            else
                runtimeRule.isActionRunning.insert(resId);
        }
        else
            runtimeRule.isActionRunning.remove(resId);
    }

    if (!condOK)
        return QnAbstractBusinessActionPtr();

    if (rule->eventState() != QnBusiness::UndefinedState && rule->eventState() != bEvent->getToggleState())
        return QnAbstractBusinessActionPtr();


    if (rule->aggregationPeriod() == 0)
        return QnBusinessActionFactory::instantiateAction(rule, bEvent);

    QString eventKey = rule->getUniqueId();
    if (bEvent->getResource())
        eventKey += bEvent->getResource()->getUniqueId();

    QnProcessorAggregationInfo& aggInfo = m_aggregateActions[eventKey];
    if (!aggInfo.initialized())
        aggInfo.init(bEvent, rule);

    aggInfo.append(bEvent->getRuntimeParams());

    if (aggInfo.isExpired())
    {
        QnAbstractBusinessActionPtr result = QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                                        aggInfo.event(),
                                                                                        aggInfo.info());
        aggInfo.reset();
        return result;
    }

    return QnAbstractBusinessActionPtr();
}

void QnBusinessRuleProcessor::at_timer()
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, QnProcessorAggregationInfo>::iterator itr = m_aggregateActions.begin();
    while (itr != m_aggregateActions.end())
    {
        QnProcessorAggregationInfo& aggInfo = itr.value();
        if (aggInfo.totalCount() > 0 && aggInfo.isExpired())
        {
            executeAction(QnBusinessActionFactory::instantiateAction(aggInfo.rule(),
                                                                     aggInfo.event(),
                                                                     aggInfo.info()));
            aggInfo.reset();
        }
        ++itr;
    }
}

bool QnBusinessRuleProcessor::checkEventCondition(const QnAbstractBusinessEventPtr& bEvent, const QnBusinessEventRulePtr& rule)
{
    bool resOK = !bEvent->getResource() || rule->eventResources().isEmpty() || rule->eventResources().contains(bEvent->getResource()->getId());
//    auto resList = rule->eventResourceObjects<QnResource>();
//    bool resOK = !bEvent->getResource() || resList.isEmpty() || resList.contains(bEvent->getResource());
    if (!resOK)
        return false;


    if (!QnBusiness::hasToggleState(bEvent->getEventType()))
        return true;
    
    // for continue event put information to m_eventsInProgress
    QnUuid resId = bEvent->getResource() ? bEvent->getResource()->getId() : QnUuid();
    RunningRuleInfo& runtimeRule = m_rulesInProgress[rule->getUniqueId()];
    if (bEvent->getToggleState() == QnBusiness::ActiveState)
        runtimeRule.resources[resId] = bEvent;
    else 
        runtimeRule.resources.remove(resId);

    return true;
}

QnAbstractBusinessActionList QnBusinessRuleProcessor::matchActions(const QnAbstractBusinessEventPtr& bEvent)
{
    QnAbstractBusinessActionList result;
    for(const QnBusinessEventRulePtr& rule: m_rules)    
    {
        if (rule->isDisabled() || rule->eventType() != bEvent->getEventType())
            continue;
        bool condOK = checkEventCondition(bEvent, rule);
        if (condOK)
        {
            QnAbstractBusinessActionPtr action;

            if (QnBusiness::hasToggleState(rule->actionType()))
                action = processToggleAction(bEvent, rule);
            else
                action = processInstantAction(bEvent, rule);

            if (action)
                result << action;

            RunningRuleMap::Iterator itr = m_rulesInProgress.find(rule->getUniqueId());
            if (itr != m_rulesInProgress.end() && itr->resources.isEmpty())
                m_rulesInProgress.erase(itr); // clear running info if event is finished (all running event resources actually finished)
        }
    }
    return result;
}

void QnBusinessRuleProcessor::at_actionDelivered(const QnAbstractBusinessActionPtr& action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

void QnBusinessRuleProcessor::at_actionDeliveryFailed(const QnAbstractBusinessActionPtr& action)
{
    Q_UNUSED(action)
    //TODO: #vasilenko implement me
}

QByteArray QnBusinessRuleProcessor::getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const
{
    Q_UNUSED(id);
    Q_UNUSED(timestampUsec);
    Q_UNUSED(dstSize);

    return QByteArray();
}

static const unsigned int MS_PER_SEC = 1000;
static const unsigned int emailAggregationPeriodMS = 30 * MS_PER_SEC;

bool QnBusinessRuleProcessor::sendMail(const QnSendMailBusinessActionPtr& action )
{
    //QMutexLocker lk( &m_mutex );  m_mutex is locked down the stack

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
    if( !aggregatedData.action )
    {
        aggregatedData.action = QnSendMailBusinessActionPtr( new QnSendMailBusinessAction( *action ) );
        using namespace std::placeholders;
        aggregatedData.periodicTaskID = TimerManager::instance()->addTimer(
            std::bind(&QnBusinessRuleProcessor::sendAggregationEmail, this, aggregationKey),
            emailAggregationPeriodMS );
    }

    ++aggregatedData.eventCount;

    //adding event source (camera) to the aggregation info
    QnBusinessAggregationInfo aggregationInfo = aggregatedData.action->aggregationInfo();
    aggregationInfo.append( action->getRuntimeParams(), action->aggregationInfo() );
    aggregatedData.action->setAggregationInfo( aggregationInfo );

    return true;
}

void QnBusinessRuleProcessor::sendAggregationEmail( const SendEmailAggregationKey& aggregationKey )
{
    QMutexLocker lk( &m_mutex );

    auto aggregatedActionIter = m_aggregatedEmails.find(aggregationKey);
    if( aggregatedActionIter == m_aggregatedEmails.end() )
        return;

    if( !sendMailInternal( aggregatedActionIter->action, aggregatedActionIter->eventCount ) )
    {
        NX_LOG( lit("Failed to send aggregated email"), cl_logDEBUG1 );
    }

    m_aggregatedEmails.erase( aggregatedActionIter );
}

QVariantList QnBusinessRuleProcessor::aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
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

QVariantList QnBusinessRuleProcessor::aggregatedEventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QList<QnInfoDetail>& aggregationDetailList,
    bool useIp )
{
    QVariantList result;
    for (const QnInfoDetail& detail: aggregationDetailList)
        result << eventDetailsMap(action, detail, useIp);
    return result;
}


QVariantHash QnBusinessRuleProcessor::eventDetailsMap(
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
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent: 
    case LicenseIssueEvent:
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

QVariantHash QnBusinessRuleProcessor::eventDescriptionMap(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo &aggregationInfo, QnEmailAttachmentList& attachments, bool useIp)
{
    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    QVariantHash contextMap;

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    contextMap[tpEvent] = QnBusinessStringsHelper::eventName(eventType);
    contextMap[tpSource] = getFullResourceName(QnBusinessStringsHelper::eventSource(params), useIp);
    if (eventType == QnBusiness::CameraMotionEvent) {
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
                if (QnResourcePtr res = qnResPool->getResourceById(cameraId))
                {
                    QVariantMap camera;

                    camera[QLatin1String("name")] = getFullResourceName(res, useIp);
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

bool QnBusinessRuleProcessor::sendMailInternal( const QnSendMailBusinessActionPtr& action, int aggregatedResCount )
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
    QtConcurrent::run(std::bind(&QnBusinessRuleProcessor::sendEmailAsync, this, data));

    /*
     * This action instance is not used anymore but storing into the Events Log db.
     * Therefore we are storing all used emails in order to not recalculate them in
     * the event log processing methods. --rvasilenko
     */
    action->getParams().emailAddress = formatEmailList(recipients);
    return true;
}

void QnBusinessRuleProcessor::sendEmailAsync(const ec2::ApiEmailData& data)
{
    if (!m_emailManager->sendEmail(data))
    {
        QnAbstractBusinessActionPtr action(new QnSystemHealthBusinessAction(QnSystemHealth::EmailSendError));
        broadcastBusinessAction(action);
        NX_LOG(lit("Error processing action SendMail."), cl_logWARNING);
    }
}

void QnBusinessRuleProcessor::at_broadcastBusinessActionFinished( int handle, ec2::ErrorCode errorCode )
{
    if (errorCode == ec2::ErrorCode::ok)
        return;

    qWarning() << "error delivering broadcast action message #" << handle << "error:" << ec2::toString(errorCode);
}

bool QnBusinessRuleProcessor::broadcastBusinessAction(const QnAbstractBusinessActionPtr& action)
{
    QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->broadcastBusinessAction(
        action, this, &QnBusinessRuleProcessor::at_broadcastBusinessActionFinished );
    return true;
}

void QnBusinessRuleProcessor::at_businessRuleChanged_i(const QnBusinessEventRulePtr& bRule)
{
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == bRule->id())
        {
            if( !m_rules[i]->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            if( !bRule->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( bRule, true );
            terminateRunningRule(m_rules[i]);
            m_rules[i] = bRule;
            return;
        }
    }

    //adding new rule
    m_rules << bRule;
    if( !bRule->isDisabled() )
        notifyResourcesAboutEventIfNeccessary( bRule, true );
}

void QnBusinessRuleProcessor::at_businessRuleChanged(const QnBusinessEventRulePtr& bRule)
{
    QMutexLocker lock(&m_mutex);
    at_businessRuleChanged_i(bRule);
}

void QnBusinessRuleProcessor::at_businessRuleReset(const QnBusinessEventRuleList& rules)
{
    QMutexLocker lock(&m_mutex);

    // Remove all rules
    for (int i = 0; i < m_rules.size(); ++i)
    {
        if( !m_rules[i]->isDisabled() )
            notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
        terminateRunningRule(m_rules[i]);
    }
    m_rules.clear();

    for(const QnBusinessEventRulePtr& rule: rules) {
        at_businessRuleChanged_i(rule);
    }
}

void QnBusinessRuleProcessor::toggleInputPortMonitoring(const QnResourcePtr& resource, bool toggle)
{
    QMutexLocker lock(&m_mutex);

    QnVirtualCameraResourcePtr camResource = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camResource)
        return;

    for( const QnBusinessEventRulePtr& rule: m_rules )
    {
        if( rule->isDisabled() )
            continue;

        if( rule->eventType() == QnBusiness::CameraInputEvent)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(rule->eventResources());
            if( resList.isEmpty() ||            //listening all cameras
                resList.contains(camResource) )
            {
                if( toggle )
                    camResource->inputPortListenerAttached();
                else
                    camResource->inputPortListenerDetached();
            }
        }
    }
}

void QnBusinessRuleProcessor::terminateRunningRule(const QnBusinessEventRulePtr& rule)
{
    QString ruleId = rule->getUniqueId();
    RunningRuleInfo runtimeRule = m_rulesInProgress.value(ruleId);
    bool isToggledAction = QnBusiness::hasToggleState(rule->actionType()); // We decided to terminate continues actions only if rule is changed
    if (!runtimeRule.isActionRunning.isEmpty() && !runtimeRule.resources.isEmpty() && isToggledAction)
    {
        for(const QnUuid& resId: runtimeRule.isActionRunning)
        {
            // terminate all actions. If instant action, terminate all resources on which it was started
            QnAbstractBusinessEventPtr bEvent;
            if (!resId.isNull())
                bEvent = runtimeRule.resources.value(resId);
            else
                bEvent = runtimeRule.resources.begin().value(); // for continues action resourceID is not specified and only one record is used
            if (bEvent) {
                QnAbstractBusinessActionPtr action = QnBusinessActionFactory::instantiateAction(rule, bEvent, QnBusiness::InactiveState);
                if (action)
                    executeAction(action);
            }
        }
    }
    m_rulesInProgress.remove(ruleId);

    QString aggKey = rule->getUniqueId();
    QMap<QString, QnProcessorAggregationInfo>::iterator itr = m_aggregateActions.lowerBound(aggKey);
    while (itr != m_aggregateActions.end())
    {
        itr = m_aggregateActions.erase(itr);
        if (itr == m_aggregateActions.end() || !itr.key().startsWith(aggKey))
            break;
    }
}

void QnBusinessRuleProcessor::at_businessRuleDeleted(QnUuid id)
{
    QMutexLocker lock(&m_mutex);

    for (int i = 0; i < m_rules.size(); ++i)
    {
        if (m_rules[i]->id() == id)
        {
            if( !m_rules[i]->isDisabled() )
                notifyResourcesAboutEventIfNeccessary( m_rules[i], false );
            terminateRunningRule(m_rules[i]);
            m_rules.removeAt(i);
            break;
        }
    }
}

void QnBusinessRuleProcessor::notifyResourcesAboutEventIfNeccessary( const QnBusinessEventRulePtr& businessRule, bool isRuleAdded )
{
    //notifying resources to start input monitoring
    {
        if( businessRule->eventType() == QnBusiness::CameraInputEvent)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(businessRule->eventResources());
            if (resList.isEmpty())
                resList = qnResPool->getAllCameras(QnResourcePtr(), true);

            for(const QnVirtualCameraResourcePtr &camera: resList)
            {
                if( isRuleAdded )
                    camera->inputPortListenerAttached();
                else
                    camera->inputPortListenerDetached();
            }
        }
    }

    //notifying resources about recording action
    {
        if( businessRule->actionType() == QnBusiness::CameraRecordingAction)
        {
            QnVirtualCameraResourceList resList = qnResPool->getResources<QnVirtualCameraResource>(businessRule->actionResources());
            for(const QnVirtualCameraResourcePtr &camera: resList)
            {
                if( isRuleAdded )
                    camera->recordingEventAttached();
                else
                    camera->recordingEventDetached();
            }
        }
    }
}
