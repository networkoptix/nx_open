#pragma once

#include <QtCore/QCoreApplication>

#include <cdb/result_code.h>


class QnCloudResultInfo
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultInfo)

public:
    QnCloudResultInfo(nx::cdb::api::ResultCode code);
    operator QString() const;

    static QString toString(nx::cdb::api::ResultCode code);

private:
    QString m_text;
};
