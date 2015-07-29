/**********************************************************
* 22 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef CUSTOM_HEADERS_H
#define CUSTOM_HEADERS_H

#include <QByteArray>


namespace Qn
{
    static const QByteArray GUID_HEADER_NAME = "X-guid";
    static const QByteArray SERVER_GUID_HEADER_NAME = "X-server-guid";
    static const QByteArray CAMERA_GUID_HEADER_NAME = "X-camera-guid";
    static const QByteArray VIDEOWALL_GUID_HEADER_NAME = "X-NetworkOptix-VideoWall";
    static const QByteArray PROXY_TTL_HEADER_NAME = "X-proxy-ttl";
    static const QByteArray CUSTOM_USERNAME_HEADER_NAME = "X-Nx-User-Name";
    static const QByteArray CAMERA_UNIQUE_ID_HEADER_NAME = "physicalId";

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
    static const QByteArray EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME = "X-Nx-base64-encoding-required";
}

#endif  //CUSTOM_HEADERS_H
