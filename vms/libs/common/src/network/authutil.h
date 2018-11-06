#pragma once

#include <QtCore/QMap>
#include <QtCore/QByteArray>
#include <QStringList>
#include <nx/fusion/model_functions_fwd.h>

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray& authData, char delimiter);

class HttpAuthenticationClientContext;

/**
 * Calculate HA1 (see rfc 2617).
 */
QByteArray createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm);

 QByteArray createHttpQueryAuthParam(
     const QString& userName,
     const QByteArray& digest,
     const QByteArray& method,
     QByteArray nonce);

 QByteArray createHttpQueryAuthParam(
     const QString& userName,
     const QString& password,
     const QString& realm,
     const QByteArray& method,
     QByteArray nonce);

struct NonceReply
{
    QString nonce;
    QString realm;
};
#define NonceReply_Fields (nonce)(realm)

QN_FUSION_DECLARE_FUNCTIONS(NonceReply, (json))
