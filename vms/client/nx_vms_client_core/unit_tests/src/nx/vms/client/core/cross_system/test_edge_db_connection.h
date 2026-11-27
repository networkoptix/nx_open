// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/reflect/json.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/vms/client/core/cross_system/edge_db_data_loader.h>
#include <nx/vms/client/core/network/edge_db_connection.h>

namespace nx::vms::client::core::test {

inline QString edgeDbPath(const nx::Uuid& organizationId, const QString& collection)
{
    return QString("/edgeDb/v4/organizations/%1/%2")
        .arg(organizationId.toSimpleString(), collection);
}

inline nx::network::http::Response edgeDbResponse(
    nx::network::http::StatusCode::Value code, const QByteArray& body)
{
    nx::network::http::Response result;
    result.statusLine.version = nx::network::http::http_1_1;
    result.statusLine.statusCode = code;
    result.statusLine.reasonPhrase = nx::network::http::StatusCode::toString(code);
    result.messageBody = body;
    return result;
}

inline nx::network::http::Response edgeDbError(
    nx::network::http::StatusCode::Value code, const QString& errorId, const QString& message)
{
    const QJsonObject error{{"errorId", errorId}, {"errorString", message}};
    return edgeDbResponse(code, QJsonDocument(error).toJson(QJsonDocument::Compact));
}

inline nx::network::http::Response edgeDbJson(const QJsonValue& value)
{
    const auto document =
        value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject());
    return edgeDbResponse(
        nx::network::http::StatusCode::ok, document.toJson(QJsonDocument::Compact));
}

/** Answers requests with the configured handler and responses. */
class TestEdgeDbConnection: public EdgeDbConnection
{
public:
    struct Request
    {
        QString path;
        nx::network::rest::Params params;
    };

    /** Response or network error. */
    using Reply = std::expected<nx::network::http::Response, SystemError::ErrorCode>;

    using Handler = std::function<Reply(const Request&)>;

    /** Answers requests which have no explicit response. Unknown requests get 404 otherwise. */
    Handler handler;

    /** Explicit response by path and `_next` cursor, overrides the handler. */
    std::map<std::pair<QString, QString>, Reply> responses;

    std::vector<Request> requests;

    /** Canceled requests still get their replies, like the real ones may. */
    std::vector<rest::Handle> canceled;

    /** When set, responses are kept until `replyAll()` is called. */
    bool deferReplies = false;

    void replyAll()
    {
        auto replies = std::move(m_pendingReplies);
        for (auto& reply: replies)
            reply();
    }

    virtual void cancel(rest::Handle handle) override { canceled.push_back(handle); }

protected:
    rest::Handle sendGet(const QString& path,
        const nx::network::rest::Params& params,
        RawCallback callback) override
    {
        const Request request{path, params};
        requests.push_back(request);

        Reply result = edgeDbResponse(nx::network::http::StatusCode::notFound, {});
        if (const auto it = responses.find({path, params.value("_next")}); it != responses.end())
            result = it->second;
        else if (handler)
            result = handler(request);

        const auto handle = ++m_lastHandle;
        auto reply = [callback, handle, result]()
        {
            if (result)
                callback(handle, SystemError::noError, *result);
            else
                callback(handle, result.error(), std::nullopt);
        };

        if (deferReplies)
            m_pendingReplies.push_back(std::move(reply));
        else
            reply();

        return handle;
    }

private:
    rest::Handle m_lastHandle = 0;
    std::vector<std::function<void()>> m_pendingReplies;
};

/**
 * In-memory edgeDb of one Organization. Answers list and permissions requests, and
 * rejects requests violating the service read limits the same way the service does.
 */
class FakeEdgeDb
{
public:
    static constexpr int kListFieldLimit = 10;

    nx::Uuid organizationId;
    EdgeDbDataLoader::PermissionsMap permissions;
    std::map<QString, std::vector<QJsonObject>> collections;
    int pageSize = 2;

    template<typename Model>
    void add(const QString& collection, const WithSite<Model>& record)
    {
        auto object = QJsonDocument::fromJson(
            QByteArray::fromStdString(nx::reflect::json::serialize(record)))
                          .object();

        // edgeDb keeps record ids without braces.
        object["id"] = record.id.toSimpleString();
        collections[collection].push_back(object);
    }

    TestEdgeDbConnection::Handler handler() const
    {
        return [this](const TestEdgeDbConnection::Request& request)
        {
            return handle(request);
        };
    }

private:
    TestEdgeDbConnection::Reply handle(const TestEdgeDbConnection::Request& request) const
    {
        using namespace nx::network::http;

        const auto prefix = edgeDbPath(organizationId, "");
        if (!request.path.startsWith(prefix))
            return edgeDbResponse(StatusCode::notFound, {});

        const QString collection = request.path.mid(prefix.size());
        if (collection == "permissions")
        {
            return edgeDbResponse(StatusCode::ok,
                QByteArray::fromStdString(nx::reflect::json::serialize(permissions)));
        }

        const auto it = collections.find(collection);
        const auto& records = it != collections.end() ? it->second : kNoRecords;
        const auto fields = request.params.value("_with").split(',', Qt::SkipEmptyParts);

        return list(records, request.params.value("_next").toInt(), fields);
    }

    nx::network::http::Response list(
        const std::vector<QJsonObject>& records, int start, const QStringList& fields) const
    {
        using namespace nx::network::http;

        if (fields.isEmpty())
            return edgeDbError(StatusCode::badRequest, "invalidParameter", "_with is required");
        if (fields.size() > kListFieldLimit)
            return edgeDbError(StatusCode::badRequest, "invalidParameter", "Too many fields");
        if (fields.contains("parameters"))
            return edgeDbError(StatusCode::badRequest, "invalidParameter", "Bad field");

        QJsonArray results;
        const int end = std::min<int>(start + pageSize, (int) records.size());
        for (int i = start; i < end; ++i)
            results.append(project(records[i], fields));

        QJsonObject page{{"results", results}};
        if (end < (int) records.size())
            page["_next"] = QString::number(end);

        return edgeDbJson(page);
    }

    static QJsonObject project(const QJsonObject& record, const QStringList& fields)
    {
        QJsonObject result;
        for (const auto& field: fields)
        {
            const auto parts = field.split('.');
            if (parts.size() == 1)
            {
                if (record.contains(field))
                    result[field] = record[field];
                continue;
            }

            const auto inner = record[parts[0]].toObject();
            if (!inner.contains(parts[1]))
                continue;

            auto projected = result[parts[0]].toObject();
            projected[parts[1]] = inner[parts[1]];
            result[parts[0]] = projected;
        }
        return result;
    }

private:
    inline static const std::vector<QJsonObject> kNoRecords;
};

/** Processes events until the condition is met or the timeout expires. */
inline bool waitFor(std::function<bool()> condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    // Let possible extra signals arrive.
    QCoreApplication::processEvents();
    return condition();
}

} // namespace nx::vms::client::core::test
