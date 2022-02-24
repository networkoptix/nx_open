// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/router.h>

class QnCommonModule;

namespace nx::vms::rules {

class Ec2Router: public Router
{
    Q_OBJECT

public:
    Ec2Router(QnCommonModule* common);
    virtual ~Ec2Router();

    virtual void init(const QnUuid& id) override;

    virtual void routeEvent(
        const EventData& eventData,
        const QSet<QnUuid>& triggeredRules,
        const QSet<QnUuid>& affectedResources) override;

    void onEventReceived(const nx::vms::api::rules::EventInfo& eventInfo);

private:
    QnUuid m_id;
    bool m_connected = false;
    QnCommonModule* m_common;
};

} // namespace nx::vms::rules