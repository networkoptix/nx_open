#pragma once
#include <QtCore>
#include <QtNetwork>
#include <utils/common/model_functions_fwd.h>

struct QnCredentials{

    QString user;
    QString password;

    QAuthenticator toAuthenticator() const;

};
#define QnCredentials_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(QnCredentials, (json)(ubjson)(xml)(csv_record))
Q_DECLARE_METATYPE(QnCredentials)



