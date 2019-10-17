#pragma once

#include <gtest/gtest.h>

#include <api/test_api_requests.h>
#include <core/resource/storage_resource.h>
#include <nx/mediaserver/camera_mock.h>
#include <nx/utils/std/algorithm.h>
#include <rest/server/json_rest_result.h>

namespace nx::vms::server::test {

class ServerForTests: public MediaServerLauncher
{
public:
    ServerForTests();

    /** Creates a server, merged to this server. */
    std::unique_ptr<ServerForTests> addServer();

    /** Creates a camera mock on this server. */
    QnSharedResourcePointer<resource::test::CameraMock> addCamera(std::optional<int> id = {});

    /** Creates a storage resource */
    QnStorageResourcePtr addStorage(const QString& name);

    template<typename T>
    T get(const QString& api)
    {
        using namespace nx::test;
        QnJsonRestResult result;
        [&](){ NX_TEST_API_GET(this, api, &result); }();
        EXPECT_EQ(result.error, QnJsonRestResult::NoError);
        return result.deserialized<T>();
    }

    template<typename T>
    auto getFlat(const QString& api) { return nx::utils::flat_map(get<T>(api)); }

public:
    const QString id; //< For debug.
};

} // namespace nx::vms::server::test
