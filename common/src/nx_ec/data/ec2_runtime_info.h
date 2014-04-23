/**********************************************************
* 12 feb 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_RUNTIME_INFO_H
#define EC2_RUNTIME_INFO_H

#include <QtCore/QByteArray>
#include <QtCore/QString>


namespace ec2
{
    class QnRuntimeInfo
    {
    public:
        QString publicIp;
        QByteArray sessionKey;
        bool allowCameraChanges;
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS( QnRuntimeInfo, (publicIp)(sessionKey)(allowCameraChanges) )
    QN_FUSION_DECLARE_FUNCTIONS(QnRuntimeInfo, (binary))
}

#endif  //EC2_RUNTIME_INFO_H
