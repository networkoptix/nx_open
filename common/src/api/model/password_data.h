#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <utils/common/request_param.h>

struct PasswordData
{
    PasswordData();
    PasswordData(const QnRequestParams& params);

    bool hasPassword() const;

    QString password;
    QByteArray realm;
    QByteArray passwordHash;
    QByteArray passwordDigest;
    QByteArray cryptSha512Hash;
};

#define PasswordData_Fields (password)(realm)(passwordHash)(passwordDigest)(cryptSha512Hash)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (PasswordData), (json));

