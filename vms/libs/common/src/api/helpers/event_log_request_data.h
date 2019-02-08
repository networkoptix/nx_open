#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

#include <nx/vms/event/event_fwd.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>

#include <nx/utils/uuid.h>

struct QnEventLogFilterData
{
    QnTimePeriod period;
    QnVirtualCameraResourceList cameras;
    nx::vms::api::EventType eventType = nx::vms::api::EventType::undefinedEvent;
    QString eventSubtype;
    nx::vms::api::ActionType actionType = nx::vms::api::ActionType::undefinedAction;
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
