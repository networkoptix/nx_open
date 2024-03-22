// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/rest/handler.h>
#include <nx/network/rest/handler_pool.h>
#include <nx/network/rest/path_router.h>

namespace nx::network::rest::test {

static const QString kIdParamName = "id";

struct MockHandler: Handler
{
    virtual void modifyPathRouteResultOrThrow(PathRouter::Result* result) const override
    {
        const auto id = result->pathParams.findValue(kIdParamName);
        if (id && *id == "*")
        {
            result->validationPath.replace(NX_FMT("{%1}", kIdParamName), "*");
            result->pathParams.remove(kIdParamName);
        }
    }
};

static PathRouter pathRouter;

static void addHandler(const QString& path)
{
    pathRouter.addHandler(path, std::make_unique<MockHandler>());
}

enum Type { api, jsonRpc, typeCount};
struct Test
{
    QString regPath;
    struct Data
    {
        QString testPath[typeCount];
        std::string expectedPath;
        Params params;
    };
    std::vector<Data> dataList;
};

static const std::vector<Test> kTests = {
    {
        "rest/^v[1-9]/devices/:id?", {
        {
            {"rest/v1/devices/id", "rest.v1.devices"},
            "/rest/v1/devices/{id}", {{"id", "id"}}
        },
        {
            {"rest/v1/devices", "rest.v1.devices"},
            "/rest/v1/devices", {}
        }}
    },
    {
        "rest/^v[1-9]/servers/:id?", {
        {
            {"rest/v1/servers/id", "rest.v1.servers"},
            "/rest/v1/servers/{id}", {{"id", "id"}}
        },
        {
            {"rest/v1/servers", "rest.v1.servers"},
            "/rest/v1/servers", {}
        }}
    },
    {
        "rest/^v[3-9]/servers/:serverId/events", {
        {
            {"rest/v3/servers/*/events", "rest.v3.servers.events"},
            "/rest/v3/servers/{serverId}/events", {{"serverId", "*"}}
        }}
    },
    {
        "rest/^v[1-9]/devices/:deviceId/bookmarks/:id?", {
        {
            {"rest/v1/devices/id1/bookmarks/id2", "rest.v1.devices.bookmarks"},
            "/rest/v1/devices/{deviceId}/bookmarks/{id}", {{"deviceId", "id1"}, {"id", "id2"}}
        },
        {
            {"rest/v1/devices/id/bookmarks", "rest.v1.devices.bookmarks"},
            "/rest/v1/devices/{deviceId}/bookmarks", {{"deviceId", "id"}}
        }}
    },
    {
        "rest/^v[1-9]/devices/*/bookmarks/*/tags", {
        {
            {"rest/v1/devices/*/bookmarks/*/tags", "rest.v1.devices.bookmarks.tags"},
            "/rest/v1/devices/*/bookmarks/*/tags", {}
        }}
    },
    {
        "rest/^v[1-9]/devices/*/searches/:id?", {
        {
            {"rest/v1/devices/*/searches/id", "rest.v1.devices.searches"},
            "/rest/v1/devices/*/searches/{id}", {{"id", "id"}}
        },
        {
            {"rest/v1/devices/*/searches", "rest.v1.devices.searches"},
            "/rest/v1/devices/*/searches", {}
        }}
    },
    {
        "rest/^v[1-9]/devices/:id/status", {
        {
            {"rest/v1/devices/id/status", "rest.v1.devices.status"},
            "/rest/v1/devices/{id}/status", {{"id", "id"}}
        },
        {
            {"rest/v1/devices/*/status", "rest.v1.devices.status"},
            "/rest/v1/devices/*/status", {}
        }}
    },
    {
        "rest/^v[1-9]/storedFiles/:path+", {
        {
            {"rest/v1/storedFiles", "rest.v1.storedFiles"},
            "/rest/v1/storedFiles", {}
        },
        {
            {"rest/v1/storedFiles/some/path.value", "rest.v1.storedFiles"},
            "/rest/v1/storedFiles/{path}", {{"path", "some/path.value"}}
        }}
    },
    {
        "rest/^v[1-9]/servers/:serverId/backupPositions/:deviceId?/:_reset?", {
        {
            {"rest/v1/servers/serverId/backupPositions", "rest.v1.servers.backupPositions"},
            "/rest/v1/servers/{serverId}/backupPositions", {{"serverId", "serverId"}}
        },
        {
            {"rest/v1/servers/serverId/backupPositions/deviceId", "rest.v1.servers.backupPositions"},
            "/rest/v1/servers/{serverId}/backupPositions/{deviceId}",
                {{"serverId", "serverId"}, {"deviceId", "deviceId"}}
        },
        {
            {"rest/v1/servers/serverId/backupPositions/deviceId/reset", "rest.v1.servers.backupPositions"},
            "/rest/v1/servers/{serverId}/backupPositions/{deviceId}/{_reset}",
                {{"serverId", "serverId"}, {"deviceId", "deviceId"}, {"_reset", "reset"}}
        }}
    },
};

struct PathRouterTest: public ::testing::Test
{
    static void SetUpTestSuite()
    {
        for (const auto& test: kTests)
            addHandler(test.regPath);
    }
};

TEST_F(PathRouterTest, Route)
{
    for (const auto& test: kTests)
    {
        for (const auto& data: test.dataList)
        {
            for (int type = api; type < typeCount; ++type)
            {
                std::unique_ptr<::testing::ScopedTrace> trace;
                switch (type)
                {
                    case api:
                        trace =
                            std::make_unique<::testing::ScopedTrace>(__FILE__, __LINE__, "api");
                        break;
                    case jsonRpc:
                        trace = std::make_unique<::testing::ScopedTrace>(
                            __FILE__, __LINE__, "jsonRpc");
                        break;
                    case typeCount:
                        break;
                }
                nx::network::http::Request httpRequest;
                auto path = data.testPath[type];
                if (type == jsonRpc)
                    path.replace('.', '/');
                httpRequest.requestLine.url.setPath(path);
                Request request(&httpRequest, kSystemSession, {}, {}, {}, type == jsonRpc);
                if (type == jsonRpc)
                {
                    request.content = {nx::network::http::header::ContentType::kJson,
                        QJson::serialized(data.params.toJson())};
                }
                ASSERT_TRUE(pathRouter.findHandlerOrThrow(&request));
                ASSERT_EQ(request.decodedPath().toStdString(), data.expectedPath)
                    << request.pathParams().toString().toStdString();
                ASSERT_EQ(request.pathParams(), data.params);
            }
        }
    }
}

TEST_F(PathRouterTest, Overlap)
{
    auto replaceVersionWithRegex = &PathRouter::replaceVersionWithRegex;

    pathRouter.addHandler(
        replaceVersionWithRegex("rest/v1/test"), std::make_unique<MockHandler>());
    pathRouter.addHandler(
        replaceVersionWithRegex("rest/v{1-2}/test"), std::make_unique<MockHandler>());
    pathRouter.addHandler(
        replaceVersionWithRegex("rest/v{2-}/test"), std::make_unique<MockHandler>());

    {
        nx::network::http::Request request;
        request.requestLine.url.setPath("/rest/v1/test");
        std::vector<std::pair<QString, QString>> expected;
        expected.push_back(std::make_pair<QString, QString>("/rest/v1/test", "/v1/test"));
        ASSERT_EQ(pathRouter.findOverlaps(Request(&request, kSystemSession)), expected);
    }

    {
        nx::network::http::Request request;
        request.requestLine.url.setPath("/rest/v2/test");
        std::vector<std::pair<QString, QString>> expected;
        expected.push_back(std::make_pair<QString, QString>("/rest/v2/test", "/v2/test"));
        ASSERT_EQ(pathRouter.findOverlaps(Request(&request, kSystemSession)), expected);
    }
}

} // namespace nx::network::rest::test
