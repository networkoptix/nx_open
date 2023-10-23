// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/media_server_data.h>
#include <nx_ec/data/api_fwd.h>
#include <utils/common/id.h>

#include "request_type_wrappers.h"

class QUrlQuery;

namespace Qn { enum class SerializationFormat: int; }
namespace nx::network::rest { class Params; }
namespace nx::vms::common { class SystemContext; }

namespace nx::vms::api {

struct StoredFilePath;

} // namespace nx::vms::api

namespace ec2 {

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QString* value);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    nx::vms::api::StoredFilePath* value);

void toUrlParams(const nx::vms::api::StoredFilePath& id, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QnUuid* id);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QnCameraUuid* id);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QnLayoutUuid* id);

void toUrlParams(const QnUuid& id, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    nx::vms::api::StorageParentId* id);

void toUrlParams(const nx::vms::api::StorageParentId& id, QUrlQuery* query);

bool parseHttpRequestParams(
    const QString& command,
    const nx::network::rest::Params& params,
    Qn::SerializationFormat* format);

void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    ApiTranLogFilter* tranLogFilter);

void toUrlParams(const ApiTranLogFilter&, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    std::nullptr_t*);

void toUrlParams(const std::nullptr_t&, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QByteArray* value);

void toUrlParams(const QByteArray& filter, QUrlQuery* query);

bool parseHttpRequestParams(
    nx::vms::common::SystemContext* systemContext,
    const QString& command,
    const nx::network::rest::Params& params,
    QnCameraDataExQuery* query);

void toUrlParams(const QnCameraDataExQuery& filter, QUrlQuery* query);

} // namespace ec2
