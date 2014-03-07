/**********************************************************
* 12 feb 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_RUNTIME_INFO_H
#define EC2_RUNTIME_INFO_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx_ec/binary_serialization_helper.h>


namespace ec2
{
    class QnRuntimeInfo
    {
    public:
        QString publicIp;
        QString systemName;
        QByteArray sessionKey;
        bool allowCameraChanges;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS( QnRuntimeInfo, (publicIp)(systemName)(sessionKey)(allowCameraChanges) )
}

#endif  //EC2_RUNTIME_INFO_H
