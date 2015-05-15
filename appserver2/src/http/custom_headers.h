/**********************************************************
* 22 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef CUSTOM_HEADERS_H
#define CUSTOM_HEADERS_H

#include <QByteArray>


namespace nx_ec
{
    static const QByteArray EC2_SYSTEM_NAME_HEADER_NAME = "X-Nx-EC-SYSTEM-NAME";
    static const QByteArray EC2_CONNECTION_STATE_HEADER_NAME = "X-Nx-EC-CONNECTION-STATE";
    static const QByteArray EC2_CONNECTION_DIRECTION_HEADER_NAME = "X-Nx-Connection-Direction";
    static const QByteArray EC2_CONNECTION_GUID_HEADER_NAME = "X-Nx-Connection-Guid";
    static const QByteArray EC2_GUID_HEADER_NAME = "X-guid";
    static const QByteArray EC2_SERVER_GUID_HEADER_NAME = "X-server-guid";
    static const QByteArray EC2_RUNTIME_GUID_HEADER_NAME = "X-runtime-guid";
    static const QByteArray EC2_SYSTEM_IDENTITY_HEADER_NAME = "X-system-identity-time";
    //!Name of Http header holding ec2 proto version
    static const QByteArray EC2_PROTO_VERSION_HEADER_NAME = "X-Nx-EC-PROTO-VERSION";
    static const QByteArray BASE64_ENCODING_REQUIRED_HEADER_NAME = "X-Nx-base64-encoding-required";
}

#endif  //CUSTOM_HEADERS_H
