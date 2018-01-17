#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/event/event_fwd.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>

#include <nx/utils/uuid.h>

struct QnEventLogFilterData
{
    QnTimePeriod period;
    QnSecurityCamResourceList cameras;
    nx::vms::event::EventType eventType = nx::vms::event::undefinedEvent;
    QnUuid eventSubtype;
    nx::vms::event::ActionType actionType = nx::vms::event::undefinedAction;
    QnUuid ruleId;

    void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;

    bool isValid(QString* errorString) const;
};

struct QnEventLogRequestData
{
    QnEventLogFilterData filter;
    Qn::SerializationFormat format = Qn::SerializationFormat::JsonFormat;

    void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;

    bool isValid(QString* errorString) const;
};
