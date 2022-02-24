// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec2_router.h"

#include <QDataStream>

#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

namespace {

const auto kFakeHandler = [](int /*handle*/, ec2::ErrorCode errorCode)
{
    qDebug() << toString(errorCode);
};

}

namespace nx::vms::rules {

Ec2Router::Ec2Router(QnCommonModule* common): m_common(common)
{
}

Ec2Router::~Ec2Router()
{
}

void Ec2Router::init(const QnUuid& id)
{
    m_id = id;

    if (!m_connected)
    {
        connect(
            m_common->messageProcessor(),
            &QnCommonMessageProcessor::vmsEventReceived,
            this,
            &Ec2Router::onEventReceived);
        m_connected = true;
    }
}

void Ec2Router::routeEvent(
        const EventData& eventData,
        const QSet<QnUuid>& triggeredRules,
        const QSet<QnUuid>& affectedResources)
{
    QSet<QnUuid> peers;
    for (const auto& id: affectedResources)
    {
        const auto& ptr = m_common->resourcePool()->getResourceById(id);
        if (!ptr)
            continue;

        const auto& server = ptr->getParentId();
        if (server.isNull())
            continue; //< TODO: #spanasenko Check all resource types...

        peers += server;
    }
    //TODO: #spanasenko Add client peers.

    for (const auto& ruleId: triggeredRules)
    {
        nx::vms::api::rules::EventInfo info;
        info.id = ruleId;
        for (auto it = eventData.begin(); it != eventData.end(); ++it)
        {
            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << it.value();
            info.props[it.key()] = data.toBase64(); //< TODO: #spanasenko Refactor.
        }

        m_common->ec2Connection()->getVmsRulesManager(Qn::kSystemAccess)->broadcastEvent(
            info, kFakeHandler);
    }
}

void Ec2Router::onEventReceived(const nx::vms::api::rules::EventInfo& eventInfo)
{
    const auto& ruleId = eventInfo.id;
    EventData eventData;
    for (auto it = eventInfo.props.begin(); it != eventInfo.props.end(); ++it)
    {
        auto data = QByteArray::fromBase64(it.value().toLatin1());
        QDataStream ds(&data, QIODevice::ReadOnly);
        ds >> eventData[it.key()];
    }
    emit eventReceived(ruleId, eventData);
}

} // namespace nx::vms::rules
