// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_multiserver_request_data.h"

#include <api/helpers/camera_id_helper.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/string.h>

namespace {

static const QString kSortOrderParam = "sortOrder";
static const QString kLimitParam = "limit";

static constexpr int kInvalidStartTime = -1;

} // namespace

QnEventLogMultiserverRequestData::QnEventLogMultiserverRequestData()
{
    format = Qn::SerializationFormat::ubjson;
}

QnEventLogMultiserverRequestData::QnEventLogMultiserverRequestData(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
    :
    QnEventLogMultiserverRequestData()
{
    loadFromParams(resourcePool, params);
}

void QnEventLogMultiserverRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    filter.loadFromParams(resourcePool, params);

    order = params.contains(kSortOrderParam)
        ? nx::reflect::fromString<Qt::SortOrder>(params.value(kSortOrderParam).toStdString())
        : kDefaultOrder;

    limit = params.contains(kLimitParam)
        ? qMax(0, params.value(kLimitParam).toInt())
        : kNoLimit;
}

nx::network::rest::Params QnEventLogMultiserverRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.unite(filter.toParams());

    if (order != kDefaultOrder)
        result.insert(kSortOrderParam, nx::reflect::toString(order));

    if (limit != kNoLimit)
        result.insert(kLimitParam, QString::number(limit));

    return result;
}

bool QnEventLogMultiserverRequestData::isValid() const
{
    return QnMultiserverRequestData::isValid() && filter.isValid(nullptr);
}
