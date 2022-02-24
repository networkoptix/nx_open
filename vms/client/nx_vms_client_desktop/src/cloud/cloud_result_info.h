// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/cloud/db/api/result_code.h>

class NX_VMS_CLIENT_DESKTOP_API QnCloudResultInfo
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultInfo)

public:
    QnCloudResultInfo(nx::cloud::db::api::ResultCode code);
    operator QString() const;

    static QString toString(nx::cloud::db::api::ResultCode code);

private:
    QString m_text;
};
