#pragma once

#include "data.h"

#include <QtCore/QByteArray>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DatabaseDumpData: Data
{
    /**%apidoc Binary database dump, encoded in Base64. */
    QByteArray data;
};
#define DatabaseDumpData_Fields (data)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DatabaseDumpData);
