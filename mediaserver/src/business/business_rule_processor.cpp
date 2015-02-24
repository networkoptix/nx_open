#include "business_rule_processor.h"

#include <QtCore/QList>
#include <QtCore/QBuffer>
#include <QtGui/QImage>

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
#include "mustache/partial_info.h"

#include <utils/common/synctime.h>
#include <utils/common/email.h>
#include <utils/common/log.h>
#include "business/business_strings_helper.h"
#include <utils/common/app_info.h>
#include <utils/common/timermanager.h>

#include "nx_ec/data/api_email_data.h"
#include "common/common_module.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "email_manager_impl.h"
#include <QtConcurrent>

namespace {
    const QString tpProductLogoFilename(lit("productLogoFilename"));
    const QString tpEventLogoFilename(lit("eventLogoFilename"));
    const QString tpProductLogo(lit("productLogo.png"));
    const QString tpCompanyName(lit("companyName"));
    const QString tpCompanyUrl(lit("companyUrl"));
    const QString tpSupportEmail(lit("supportEmail"));
    const QString tpSystemName(lit("systemName"));
    const QString tpImageMimeType(lit("image/png"));
    const QString tpScreenshotFilename(lit("screenshot"));
    const QString tpScreenshot(lit("screenshot.jpeg"));
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
        const QnResourcePtr& mServerRes = qnResPool->getResourceById(qnCommon->moduleGUID());
        const QnMediaServerResource* mServer = dynamic_cast<const QnMediaServerResource*>(mServerRes.data());
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
    return qnResPool->getResourceById(res->getParentId()).dynamicCast<QnMediaServerResource>();
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
    QnResourceList resList = action->getResourceObjects().filtered<QnNetworkResource>();
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

QByteArray QnBusinessRuleProcessor::getEventScreenshotEncoded(const QnBusinessEventParameters& params, QSize dstSize) const
{
    Q_UNUSED(params);
    Q_UNUSED(dstSize);

    return QByteArray();
}

static const unsigned int MS_PER_SEC = 1000;
static const unsigned int emailAggregationPeriodMS = 30 * MS_PER_SEC;

bool QnBusinessRuleProcessor::sendMail(const QnSendMailBusinessActionPtr& action )
{
    //QMutexLocker lk( &m_mutex );  m_mutex is locked down the stack

    //aggregating by recipients and eventtype
    if( action->getRuntimeParams().getEventType() != QnBusiness::CameraDisconnectEvent &&
        action->getRuntimeParams().getEventType() != QnBusiness::NetworkIssueEvent )
    {
        return sendMailInternal( action, 1 );  //currently, aggregating only cameraDisconnected and networkIssue events
    }

    QStringList recipients;
    for (const QnUserResourcePtr &user: action->getResourceObjects().filtered<QnUserResource>()) {
        QString email = user->getEmail();
        if (!email.isEmpty() && QnEmailAddress::isValid(email))
            recipients << email;
    }

    SendEmailAggregationKey aggregationKey( action->getRuntimeParams().getEventType(), recipients.join(';') );
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

bool QnBusinessRuleProcessor::sendMailInternal( const QnSendMailBusinessActionPtr& action, int aggregatedResCount )
{
    Q_ASSERT( action );

    QStringList log;
    QStringList recipients;
    for (const QnUserResourcePtr &user: action->getResourceObjects().filtered<QnUserResource>()) {
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


    QVariantHash contextMap = QnBusinessStringsHelper::eventDescriptionMap(action, action->aggregationInfo(), true);
    QnPartialInfo partialInfo(action->getRuntimeParams().getEventType());

    assert(!partialInfo.attrName.isEmpty());
//    contextMap[partialInfo.attrName] = lit("true");

    QnEmailSettings emailSettings = QnGlobalSettings::instance()->emailSettings();

    QnEmailAttachmentList attachments;
    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpProductLogo, lit(":/skin/email_attachments/productLogo.png"), tpImageMimeType)));
    attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(partialInfo.eventLogoFilename, lit(":/skin/email_attachments/") + partialInfo.eventLogoFilename, tpImageMimeType)));
    contextMap[tpProductLogoFilename] = lit("cid:") + tpProductLogo;
    contextMap[tpEventLogoFilename] = lit("cid:") + partialInfo.eventLogoFilename;
    contextMap[tpCompanyName] = QnAppInfo::organizationName();
    contextMap[tpCompanyUrl] = QnAppInfo::companyUrl();
    contextMap[tpSupportEmail] = emailSettings.supportEmail;
    contextMap[tpSystemName] = emailSettings.signature;
    attachments.append(partialInfo.attachments);

    QByteArray screenshotData = this->getEventScreenshotEncoded(action->getRuntimeParams(), QSize(640, 480));
    if (!screenshotData.isNull()) {
        QBuffer screenshotStream(&screenshotData);
        attachments.append(QnEmailAttachmentPtr(new QnEmailAttachment(tpScreenshot, screenshotStream, lit("image/jpeg"))));
        contextMap[tpScreenshotFilename] = lit("cid:") + tpScreenshot;
    }

    QString messageBody = renderTemplateFromFile(lit(":/email_templates"), partialInfo.attrName + lit(".mustache"), contextMap);

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
};

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

    QnSecurityCamResource* camResource = dynamic_cast<QnSecurityCamResource*>(resource.data());
    if( camResource == nullptr )
        return;

    for( const QnBusinessEventRulePtr& rule: m_rules )
    {
        if( rule->isDisabled() )
            continue;

        if( rule->eventType() == QnBusiness::CameraInputEvent)
        {
            QnVirtualCameraResourceList resList = rule->eventResourceObjects().filtered<QnVirtualCameraResource>();
            if( resList.isEmpty() ||            //listening all cameras
                resList.contains(resource.staticCast<QnVirtualCameraResource>()) )
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
            QnVirtualCameraResourceList resList = businessRule->eventResourceObjects().filtered<QnVirtualCameraResource>();
            if (resList.isEmpty())
                resList = qnResPool->getAllCameras(QnResourcePtr(), true);

            for(const QnVirtualCameraResourcePtr camera: resList)
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
        QnVirtualCameraResourceList resList = businessRule->actionResourceObjects().filtered<QnVirtualCameraResource>();
        if( businessRule->actionType() == QnBusiness::CameraRecordingAction)
        {
            for(const QnVirtualCameraResourcePtr camera: resList)
            {
                if( isRuleAdded )
                    camera->recordingEventAttached();
                else
                    camera->recordingEventDetached();
            }
        }
    }
}
