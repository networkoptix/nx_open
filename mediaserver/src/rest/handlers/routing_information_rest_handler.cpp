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

namespace {
    QByteArray routeToHtml(const QnUuid &target, const QnRoute &route) {
        QByteArray result;

        result.append("<b>Route to ");
        result.append(target.toByteArray());
        result.append("</b><br/>\r\n");

        for (const QnRoutePoint &point: route.points) {
            result.append(point.peerId.toByteArray());
            result.append(" ");
            result.append(point.host.toUtf8());
            result.append(":");
            result.append(QString::number(point.port).toUtf8());
            result.append("<br/>\r\n");
        }

        return result;
    }
}



int QnRoutingInformationRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType, const QnRestConnectionProcessor*) 
{
    contentType = "text/html";

    QString command = path;
    command.remove(lit("api/routingInformation/"));
    if (command.startsWith(QLatin1Char('/')))
        command = command.mid(1);

    if (command == lit("routeTo")) {
        QnUuid target(QUuid(params.value(lit("target"))));
        if (target.isNull()) {
            // suppose it to be host:port
            QStringList targetParams = params.value(lit("target")).split(QLatin1Char(':'));
            if (targetParams.size() != 2)
                return HttpCode::BadRequest;

            QString host = targetParams.first();
            quint16 port = targetParams.last().toUShort();

            if (host.isEmpty() || port == 0)
                return HttpCode::BadRequest;

            target = QnRouter::instance()->whoIs(host, port);
        }

        QnRoute route = QnRouter::instance()->routeTo(target);

        result.append("<html><body>\r\n");

        if (route.isValid()) {
            result.append(routeToHtml(target, route));
        } else {
            result.append("Could not found a route to ");
            result.append(params.value(lit("target")).toUtf8());
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
    } else if (command == lit("routes")) {
        QnUuid target = QnUuid(params.value(lit("target")));
        QHash<QnUuid, QnRouteList> routes = QnRouter::instance()->routes();

        result.append("<html><body>\r\n");

        QList<QnUuid> targets = target.isNull() ? routes.uniqueKeys() : (QList<QnUuid>() << target);
        for (const QnUuid &id: targets) {
            for (const QnRoute &route: routes.value(id)) {
                result.append(routeToHtml(id, route));
                result.append("<br/>\r\n");
            }
        }

        if (!target.isNull() && routes.count(target) == 0) {
            result.append("There are no available routes to ");
            result.append(params.value(lit("target")));
            result.append("\r\n");
        } else if (routes.isEmpty()) {
            result.append("There are no available routes on this server.\r\n");
        }

        result.append("</body></html>\r\n");

        return HttpCode::Ok;
    }

    return HttpCode::NotFound;
}

int QnRoutingInformationRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray &srcBodyContentType, QByteArray &result, 
                                                 QByteArray &resultContentType, const QnRestConnectionProcessor* owner) 
{
    Q_UNUSED(body)
    Q_UNUSED(srcBodyContentType)

    return executeGet(path, params, result, resultContentType, owner);
}
