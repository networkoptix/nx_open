// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/api/rules/rules_fwd.h>

#include "rules_fwd.h"

class QnCommonMessageProcessor;

namespace nx::vms::rules {

/**
 * Abstract class that provides interface for event router.
 * This router exists in a single copy as a part of every Engine instance
 * and is responsible for event delivery between different Engine instances,
 * however it was split out into a separate class for testing purposes.
 */
class NX_VMS_RULES_API Router: public QObject
{
    Q_OBJECT

public:
    using RuleList = std::vector<ConstRulePtr>;

public:
    Router();
    virtual ~Router();

    virtual void init(const QnCommonMessageProcessor* processor) = 0;

    virtual nx::Uuid peerId() const = 0;

    virtual void routeEvent(const EventPtr& event, const RuleList& triggeredRules) = 0;

    virtual void routeAction(const ActionPtr& action) = 0;

    // Default implementation emits the corresponding signal.
    virtual void receiveAction(const ActionPtr& action);

signals:
    void eventReceived(const EventPtr& event, const RuleList& triggeredRules);
    void actionReceived(const nx::vms::rules::ActionPtr& action);
};

} // namespace nx::vms::rules
