#include "change_admin_password_rest_handler.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "api/app_server_connection.h"

namespace HttpStatusCode {
    enum Code {
        ok = 200,
        badRequest = 400
    };
}

int QnChangeAdminPasswordRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_UNUSED(result)

    QByteArray hash = params.value("hash").toLatin1();
    QByteArray digest = params.value("digest").toLatin1();
    if (hash.isEmpty() || hash.count('$') != 2 || digest.isEmpty())
        return HttpStatusCode::badRequest;

    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::user)) {
        QnUserResourcePtr user = resource.staticCast<QnUserResource>();
        if (user->getName() == lit("admin")) {
            if (user->getHash() == hash && user->getDigest() == digest)
                return HttpStatusCode::ok;

            user->setHash(hash);
            user->setDigest(digest);
            QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(user, this, [](int, ec2::ErrorCode) { return; });

            return HttpStatusCode::ok;
        }
    }

    return HttpStatusCode::badRequest;
}

int QnChangeAdminPasswordRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}

QString QnChangeAdminPasswordRestHandler::description() const {
    return
        "Changes the admin's password<br>"
        "Request format: GET /api/changeAdminPassword?hash=<hash>&digest=<digets><br>";
}
