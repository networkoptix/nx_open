// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>
#include <recording/time_period.h>

namespace nx::network::rest { class Params; }

struct NX_VMS_COMMON_API QnEventLogFilterData
{
    QnTimePeriod period;
    QnVirtualCameraResourceList cameras;
    std::vector<nx::vms::api::EventType> eventTypeList;
    QString eventSubtype;
    nx::vms::api::ActionType actionType = nx::vms::api::ActionType::undefinedAction;
    QnUuid ruleId;
    QString text;
    bool eventsOnly = false;

    void loadFromParams(QnResourcePool* resourcePool, const nx::network::rest::Params& params);
    nx::network::rest::Params toParams() const;

    bool isValid(QString* errorString) const;
};

struct NX_VMS_COMMON_API QnEventLogRequestData
{
    QnEventLogFilterData filter;
    Qn::SerializationFormat format = Qn::SerializationFormat::json;

    void loadFromParams(QnResourcePool* resourcePool, const nx::network::rest::Params& params);
    nx::network::rest::Params toParams() const;

    bool isValid(QString* errorString) const;
};
