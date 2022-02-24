// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API PasswordHashes
{
    static PasswordHashes calculateHashes(
        const QString& username, const QString& password, bool isLdap);

    QString realm;
    QByteArray passwordHash;
    QByteArray passwordDigest;
    QByteArray cryptSha512Hash;

    bool operator==(const PasswordHashes& other) const = default;
};

struct NX_VMS_COMMON_API PasswordData: PasswordHashes
{
    bool hasPassword() const;

    QString password;

    bool operator==(const PasswordData& other) const = default;
};
#define PasswordData_Fields (password)(realm)(passwordHash)(passwordDigest)(cryptSha512Hash)

struct CurrentPasswordData
{
    QString currentPassword;
};
#define CurrentPasswordData_Fields (currentPassword)

struct CameraPasswordData
{
    QString cameraId;
    QString user;
    QString password;
};
#define CameraPasswordData_Fields (cameraId)(user)(password)

QN_FUSION_DECLARE_FUNCTIONS(PasswordData, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CameraPasswordData, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CurrentPasswordData, (json), NX_VMS_COMMON_API)
