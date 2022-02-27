// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QByteArray>
#include <QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray& authData, char delimiter);

/**
 * Calculate HA1 (see rfc 2617).
 */
QByteArray createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm);

 NX_VMS_COMMON_API QByteArray createHttpQueryAuthParam(
     const QString& userName,
     const QByteArray& digest,
     const nx::network::http::Method& method,
     QByteArray nonce);

 NX_VMS_COMMON_API QByteArray createHttpQueryAuthParam(
     const QString& userName,
     const QString& password,
     const QString& realm,
     const nx::network::http::Method& method,
     QByteArray nonce);

struct NonceReply
{
    QString nonce;
    QString realm;
};
#define NonceReply_Fields (nonce)(realm)

QN_FUSION_DECLARE_FUNCTIONS(NonceReply, (json), NX_VMS_COMMON_API)
