#ifndef API_REVERSE_CONNECTION_DATA_H
#define API_REVERSE_CONNECTION_DATA_H

#include "api_data.h"

#include <QByteArray>
#include <QString>


namespace ec2 {

    //! Request to open @var count proxy connections to @var target server.
    struct ApiReverseConnectionData: ApiData
    {
        QnUuid targetServer;
        int securityCode;
        int socketCount;
    };

    #define ApiReverseConnectionData_Fields (targetServer)(securityCode)(socketCount)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiReverseConnectionData);

#endif // API_REVERSE_CONNECTION_DATA_H
