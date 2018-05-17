#pragma once

#include "api_data.h"

#include <QByteArray>
#include <QString>


namespace ec2 {

/** Request to open count proxy connections to @var target server. */
struct ApiReverseConnectionData: nx::vms::api::Data
{
    QnUuid targetServer;
    int socketCount = 0;
};
#define ApiReverseConnectionData_Fields (targetServer)(socketCount)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiReverseConnectionData);
