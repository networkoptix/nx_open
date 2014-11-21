#include "routing_information_rest_handler.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/router.h"

namespace {
    QVariantMap routePointToVariant(const QnRoutePoint &point) {
        QVariantMap result;
        result["id"] = point.peerId.toString();
        result["host"] = point.host;
        result["port"] = point.port;
        return result;
    }

    QVariantList routeToVariant(const QnRoute &route) {
        QVariantList list;

        for (const QnRoutePoint &point: route.points)
            list.append(routePointToVariant(point));

        return list;
    }

    QVariant getConnections(const QnUuid &target) {
        QVariantMap connectionsMap;

        QMultiHash<QnUuid, QnRouter::Endpoint> connections = QnRouter::instance()->connections();
        for (const QnUuid &id: connections.keys()) {
            if (!target.isNull() && target != id)
                continue;

            QVariantList connectionsList;
            for (const QnRouter::Endpoint &endpoint: connections.values(id)) {
                QVariantMap connection;
                connection["id"] = endpoint.id.toString();
                connection["host"] = endpoint.host;
                connection["port"] = endpoint.port;
                connectionsList.append(connection);
            }
            connectionsMap[id.toString()] = connectionsList;
        }

        return connectionsMap;
    }

    QVariant getCachedRoutes(const QnUuid &target) {
        QHash<QnUuid, QnRoute> routes = QnRouter::instance()->routes();
        QList<QnUuid> targets = target.isNull() ? routes.uniqueKeys() : (QList<QnUuid>() << target);

        QVariantMap routesMap;

        for (const QnUuid &id: targets)
            routesMap[id.toString()] = routeToVariant(routes.value(id));

        return routesMap;
    }

    QUrl makeUrl(const QString &host, int port = -1) {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(host);
        url.setPort(port);
        return url;
    }

    QVariant getRules(const QnUuid &target) {
        QVariantMap rules;
        for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers()) {
            QnUuid id = server->getId();
            if (!target.isNull() && target != id)
                continue;

            int port = server->getPort();
            QSet<QUrl> ignoredUrls = QSet<QUrl>::fromList(server->getIgnoredUrls());
            QList<QUrl> used;
            QList<QUrl> ignored;

            for (const QHostAddress &address: server->getNetAddrList()) {
                QUrl url = makeUrl(address.toString());
                QUrl explicitUrl = makeUrl(address.toString(), port);
                if (ignoredUrls.contains(url) || ignoredUrls.contains(explicitUrl))
                    ignored.append(explicitUrl);
                else
                    used.append(explicitUrl);
            }

            for (const QUrl &url: server->getAdditionalUrls()) {
                if (ignoredUrls.contains(url))
                    ignored.append(url);
                else
                    used.append(url);
            }

            QVariantList usedList;
            for (const QUrl &url: used)
                usedList.append(url);

            QVariantList ignoredList;
            for (const QUrl &url: ignored)
                ignoredList.append(url);

            QVariantMap map;
            map[lit("used")] = usedList;
            map[lit("ignored")] = ignoredList;

            rules[id.toString()] = map;
        }
        return rules;
    }
}

int QnRoutingInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *) {
    QString command = path;
    command.remove(lit("api/routingInformation/"));
    if (command.startsWith(QLatin1Char('/')))
        command = command.mid(1);

    QnUuid target = QnUuid::fromStringSafe(params.value(lit("target")));

    if (command == lit("routeTo")) {
        QnRoute route;

        if (target.isNull()) {
            // suppose it to be host:port
            QStringList targetParams = params.value(lit("target")).split(QLatin1Char(':'));
            if (targetParams.size() != 2)
                return nx_http::StatusCode::badRequest;

            QString host = targetParams.first();
            quint16 port = targetParams.last().toUShort();

            if (host.isEmpty() || port == 0)
                return nx_http::StatusCode::badRequest;

            target = QnRouter::instance()->whoIs(host, port);
            route = QnRouter::instance()->routeTo(host, port);
        } else {
            route = QnRouter::instance()->routeTo(target);
        }

        if (route.isValid())
            result.setReply(QJsonValue::fromVariant(routeToVariant(route)));
        else
            result.setError(QnJsonRestResult::CantProcessRequest, lit("NO_ROUTE"));

        return nx_http::StatusCode::ok;
    } else if (command == lit("connections")) {
        result.setReply(QJsonValue::fromVariant(getConnections(target)));
        return nx_http::StatusCode::ok;
    } else if (command == lit("cachedRoutes")) {
        result.setReply(QJsonValue::fromVariant(getCachedRoutes(target)));
        return nx_http::StatusCode::ok;
    } else if (command == lit("rules")) {
        result.setReply(QJsonValue::fromVariant(getRules(target)));
        return nx_http::StatusCode::ok;
    }

    return nx_http::StatusCode::notFound;
}
