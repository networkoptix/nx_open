
#ifndef EC2_CONNECTION_DATA_H
#define EC2_CONNECTION_DATA_H

#include "api_data.h"

#include <api/model/connection_info.h>
#include <utils/common/request_param.h>

#include "../binary_serialization_helper.h"


namespace ec2
{
    //!Response to connect request
    struct ConnectionInfo
    :
        public ApiData
    {
        QString brand;
        QString version;
        //QList<QnCompatibilityItem> compatibilityItems;
        int proxyPort;
        QString ecsGuid;
        QString publicIp;

        ConnectionInfo()
        :
            proxyPort( 0 )
        {
        }

        void copy( QnConnectionInfo* const connectionInfo ) const
        {
            connectionInfo->brand = brand;
            connectionInfo->version = QnSoftwareVersion(version);
            connectionInfo->proxyPort = proxyPort;
            connectionInfo->ecsGuid = ecsGuid;
            connectionInfo->publicIp = publicIp;
        }

        QN_DECLARE_STRUCT_SERIALIZATORS();
    };

    //!Parameters of connect request
    struct LoginInfo
    :
        public ApiData
    {
    };

    inline void parseHttpRequestParams( const QnRequestParamList& params, ec2::LoginInfo* loginInfo )
    {
        //TODO/IMPL
    }
}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ConnectionInfo, (brand)(version)(proxyPort)(ecsGuid)(publicIp) )

#endif  //EC2_CONNECTION_DATA_H
