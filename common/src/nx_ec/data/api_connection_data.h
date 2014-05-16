#ifndef EC2_CONNECTION_DATA_H
#define EC2_CONNECTION_DATA_H

#include "api_data.h"

#include <QByteArray>
#include <QString>


namespace ec2
{
    //!Parameters of connect request
    struct ApiLoginData: ApiData 
    {
        QString login;
        QByteArray passwordHash;
    };

#define ApiLoginData_Fields (login)(passwordHash)
}

#endif  //EC2_CONNECTION_DATA_H
