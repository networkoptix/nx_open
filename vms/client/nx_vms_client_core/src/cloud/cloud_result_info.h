#pragma once

#include <QtCore/QCoreApplication>

#include <nx/cloud/db/api/result_code.h>


class QnCloudResultInfo
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultInfo)

public:
    QnCloudResultInfo(nx::cloud::db::api::ResultCode code);
    operator QString() const;

    static QString toString(nx::cloud::db::api::ResultCode code);

private:
    QString m_text;
};
