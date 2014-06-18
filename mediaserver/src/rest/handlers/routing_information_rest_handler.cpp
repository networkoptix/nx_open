#include "routing_information_rest_handler.h"

#include "utils/network/router.h"

//TODO: #dklychkov collect http codes in one place and stop copying enums
namespace HttpCode {
    enum {
        Ok = 200,
        BadRequest = 400,
        NotFound = 404
    };
}



int QnRoutingInformationRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(contentType)

    QString command = path;
    command.remove(lit("api/routingInformation/"));
    if (command.startsWith(QLatin1Char('/')))
        command = command.mid(1);

    if (command == lit("routeTo")) {
        QnRoute route;

        QnId target = QnId(params.value(lit("target")));
        if (!target.isNull()) {
            route = QnRouter::instance()->routeTo(target);
        } else {
            // suppose it to be host:port
            QStringList targetParams = params.value(lit("target")).split(QLatin1Char(':'));
            if (targetParams.size() != 2)
                return HttpCode::BadRequest;

            QString host = targetParams.first();
            quint16 port = targetParams.last().toUShort();

            if (host.isEmpty() || port == 0)
                return HttpCode::BadRequest;

            route = QnRouter::instance()->routeTo(host, port);
        }

        result.append("<html><body>\r\n");

        if (route.isValid()) {
            result.append("<b>Route to ");
            result.append(target.toByteArray());
            result.append("</b><br/>\r\n");

            foreach (const QnRoutePoint &point, route.points) {
                result.append(point.peerId.toByteArray());
                result.append(" ");
                result.append(point.host.toUtf8());
                result.append(":");
                result.append(QString::number(point.port).toUtf8());
                result.append("<br/>\r\n");
            }
        } else {
            result.append("Could not found a route to ");
            result.append(target.toByteArray());
            result.append("\r\n");
        }

        result.append("</body></html>\r\n");

        return HttpCode::Ok;
    } else if (command == lit("connections")) {
        result.append("<html><body>\r\n");
        auto connections = QnRouter::instance()->connections();
        for (auto it = connections.begin(); it != connections.end(); ++it) {
            result.append(it.key().toByteArray());
            result.append(" -> ");
            result.append(it->id.toByteArray());
            result.append(" ");
            result.append(it->host);
            result.append(":");
            result.append(QString::number(it->port).toUtf8());
            result.append("<br/>\r\n");
        }

        if (connections.isEmpty())
            result.append("There are no available connections on this server.\r\n");

        result.append("</body></html>\r\n");

        return HttpCode::Ok;
    }

    return HttpCode::NotFound;
}

int QnRoutingInformationRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}

QString QnRoutingInformationRestHandler::description() const {
    // this is for our support
    return QString();
}
