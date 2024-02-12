// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_params.h"

#include <QtCore/QUrlQuery>

#include <api/helpers/camera_id_helper.h>
#include <api/helpers/layout_id_helper.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/vms/api/data/stored_file_data.h>
#include <nx/vms/common/system_context.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class T>
bool deserialize(const nx::network::rest::Params& value, const QString& key, T* target)
{
    auto pos = value.findValue(key);
    if (!pos)
        return false;

    if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
        return nx::reflect::fromString(pos->toStdString(), target);
    else
        return QnLexical::deserialize(*pos, target);
}

template<class T>
void serialize(const T& value, const QString& key, QUrlQuery* target)
{
    QString serialized;
    if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
        serialized = QString::fromStdString(nx::reflect::toString(value));
    else
        serialized = QnLexical::serialized(value);

    target->addQueryItem(key, serialized);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& command,
    const nx::network::rest::Params& params,
    QString* value)
{
    NX_ASSERT(command == "getHelp");
    return deserialize(params, "group", value);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& command,
    const nx::network::rest::Params& params,
    nx::vms::api::StoredFilePath* value)
{
    NX_ASSERT(command != "getHelp");
    return deserialize(params, "folder", &(value->path)); //< nx::vms::api::StoredFilePath
}

void toUrlParams(const nx::vms::api::StoredFilePath& name, QUrlQuery* query)
{
    serialize(name.path, "folder", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    QByteArray* value)
{
    QString tmp;
    bool result = deserialize(params, "id", &tmp);
    *value = tmp.toUtf8();
    return result;
}

void toUrlParams(const QByteArray& id, QUrlQuery* query)
{
    serialize(QString::fromLatin1(id), "id", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QnCameraDataExQuery* query)
{
    // Semantics of the returned value is quite strange. We must parse both params anyway.
    parseHttpRequestParams(systemContext, command, params, &query->id);
    deserialize(params, "showDesktopCameras", &query->showDesktopCameras);
    return true;
}

void toUrlParams(const QnCameraDataExQuery& filter, QUrlQuery* query)
{
    serialize(filter.id, "id", query);
    serialize(filter.showDesktopCameras, "showDesktopCameras", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    nx::Uuid* id)
{
    return deserialize(params, "id", id);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    QnCameraUuid* id)
{
    QString stringValue;
    const bool result = deserialize(params, "id", &stringValue);
    if (result)
    {
        static const nx::Uuid kNonExistingUuid("{11111111-1111-1111-1111-111111111111}");
        *id = nx::camera_id_helper::flexibleIdToId(
            systemContext->resourcePool(), stringValue);
        if (id->isNull())
            *id = kNonExistingUuid; //< Turn on data filtering anyway.
    }
    return result;
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    QnLayoutUuid* id)
{
    QString stringValue;
    const bool result = deserialize(params, "id", &stringValue);
    if (result)
    {
        static const nx::Uuid kNonExistingUuid("{11111111-1111-1111-1111-111111111111}");
        *id = nx::layout_id_helper::flexibleIdToId(
            systemContext->resourcePool(), stringValue);
        if (id->isNull())
            *id = kNonExistingUuid; //< Turn on data filtering anyway.
    }
    return result;
}

void toUrlParams(const nx::Uuid& id, QUrlQuery* query)
{
    serialize(id, "id", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    nx::vms::api::StorageParentId* id)
{
    // TODO: Consider renaming this parameter to parentId. Note that this is a breaking change that
    // affects all API functions which are registered with ParentId input data.
    return deserialize(params, "id", id);
}

void toUrlParams(const nx::vms::api::StorageParentId& id, QUrlQuery* query)
{
    serialize(id, "id", query);
}

bool parseHttpRequestParams(
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    Qn::SerializationFormat* format)
{
    return deserialize(params, "format", format);
}

void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery* query)
{
    serialize(format, "format", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString& /*command*/,
    const nx::network::rest::Params& params,
    ApiTranLogFilter* tranLogFilter)
{
    bool b1 = deserialize(params, "cloud_only", &tranLogFilter->cloudOnly);
    bool b2 = deserialize(params, "remove_only", &tranLogFilter->removeOnly);
    return b1 || b2;
}

void toUrlParams(const ApiTranLogFilter& tranLogFilter, QUrlQuery* query)
{
    serialize(tranLogFilter.cloudOnly, "cloud_only", query);
    serialize(tranLogFilter.removeOnly, "remove_only", query);
}

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* /*systemContext*/,
    const QString&,
    const nx::network::rest::Params& /*params*/,
    std::nullptr_t*)
{
    return true;
}

void toUrlParams(const std::nullptr_t&, QUrlQuery*)
{
}

} // namespace ec2
