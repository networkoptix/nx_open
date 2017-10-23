#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <utils/common/request_param.h>

struct PasswordData
{
    PasswordData();
    PasswordData(const QnRequestParams& params);

    static PasswordData calculateHashes(const QString& username, const QString& password, bool isLdap);

    bool hasPassword() const;

    QString password;
    QString realm;
    QByteArray passwordHash;
    QByteArray passwordDigest;
    QByteArray cryptSha512Hash;
};

#define PasswordData_Fields (password)(realm)(passwordHash)(passwordDigest)(cryptSha512Hash)

struct CameraPasswordData
{
    QString cameraId;
    QString user;
    QString password;
};
#define CameraPasswordData_Fields (cameraId)(user)(password)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (PasswordData)(CameraPasswordData), (json));
