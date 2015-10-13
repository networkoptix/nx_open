#include <database/server_db.h>

    qnServerDb->removeLogForRes(resource->getId());
        qnServerDb->saveActionToDB(action, res);
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

QByteArray QnMServerBusinessRuleProcessor::getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const 
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
    if( !aggregatedData.action )
    {
        aggregatedData.action = QnSendMailBusinessActionPtr( new QnSendMailBusinessAction( *action ) );
        using namespace std::placeholders;
        aggregatedData.periodicTaskID = TimerManager::instance()->addTimer(
            std::bind(&QnMServerBusinessRuleProcessor::sendAggregationEmail, this, aggregationKey),
            emailAggregationPeriodMS );
    }

    ++aggregatedData.eventCount;

    //adding event source (camera) to the aggregation info
    QnBusinessAggregationInfo aggregationInfo = aggregatedData.action->aggregationInfo();
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

QVariantHash QnMServerBusinessRuleProcessor::eventDescriptionMap(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo &aggregationInfo, QnEmailAttachmentList& attachments, bool useIp)
QString QnMServerBusinessRuleProcessor::formatEmailList(const QStringList &value) {
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
