// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json.h>
#include <nx/vms/api/data_tests.h>
#include <nx/vms/api/data/media_server_data.h>

namespace nx::vms::api {

NX_VMS_API_PRINT_TO(StorageData);
NX_VMS_API_PRINT_TO(MediaServerData);
NX_VMS_API_PRINT_TO(MediaServerDataEx);

} // namespace nx::vms::api

namespace nx::vms::api::test {

StorageData storageData(QString path = "/mnt")
{
    StorageData storage;
    storage.id = nx::Uuid::createUuid();
    storage.name = path;
    storage.url = path;
    storage.spaceLimit = 77777;
    storage.usedForWriting = true;
    storage.storageType = "local";
    storage.addParams = {{"param", "value"}};
    storage.isBackup = false;
    return storage;
}

MediaServerData serverData()
{
    MediaServerData server;
    server.id = nx::Uuid::createUuid();
    server.name = "server";
    server.networkAddresses = "123.45.67.89";
    server.url = "http://" + server.networkAddresses;
    server.flags = SF_HasPublicIP;
    server.version = "3.2.1";
    server.systemInfo = "windows";
    server.authKey = "XYZ";
    return server;
}

MediaServerDataEx serverDataEx()
{
    MediaServerDataEx server;
    server.id = nx::Uuid::createUuid();
    server.name = "some server";
    server.networkAddresses = "123.45.67.89";
    server.url = "http://" + server.networkAddresses;
    server.version = "2.4.0";
    server.systemInfo = "arm";
    server.authKey = "DFG";
    server.maxCameras = 100;
    server.allowAutoRedundancy = true;
    server.status = ResourceStatus::online;
    server.addParams = {{"param1", "value1"}, {"param2", "value2"}};
    server.storages.push_back(storageData("/storage_1"));
    server.storages.push_back(storageData("/storage_2"));
    return server;
}

BackupBitrateBytesPerSecond backupBitrateData()
{
    BackupBitrateBytesPerSecond data;

    // Default values could be omitted by serializer. Therefore we use all
    // combinations of default and non-default key and value pairs for testing.
    data.insert({DayOfWeek::monday, 0}, 0);
    data.insert({DayOfWeek::monday, 1}, 0);
    data.insert({DayOfWeek::tuesday, 0}, 0);
    data.insert({DayOfWeek::tuesday, 1}, 0);
    data.insert({DayOfWeek::monday, 0}, 1);
    data.insert({DayOfWeek::monday, 1}, 1);
    data.insert({DayOfWeek::tuesday, 0}, 1);
    data.insert({DayOfWeek::tuesday, 1}, 1);

    return data;
}

NX_VMS_API_DATA_TEST(StorageData, storageData,
    (spaceLimit = 33333)
    (isBackup = true)
    (addParams.emplace_back("hello", "world"))
)

NX_VMS_API_DATA_TEST(MediaServerData, serverData,
    (networkAddresses = "127.0.0.1")
    (version = "4.3.2")
    (systemInfo = "linux")
    (authKey = "ABC")
)

NX_VMS_API_DATA_TEST(MediaServerDataEx, serverData,
    (networkAddresses = "127.0.0.1")
    (version = "4.3.2")
    (maxCameras = 101)
    (status = ResourceStatus::online)
    (addParams.emplace_back("hello", "world"))
    (storages.push_back(storageData("/storage_3")))
)

NX_VMS_API_DATA_TEST(MediaServerDataEx, serverDataEx,
    (networkAddresses = "127.0.0.1")
    (version = "4.3.2")
    (systemInfo = "windows")
    (status = ResourceStatus::offline)
    (addParams.clear())
    (storages.clear())
)

TEST(Reflect, BackupBitrateBytesPerSecond)
{
    const auto data = backupBitrateData();

    const auto [fusionToReflect, success] =
        nx::reflect::json::deserialize<BackupBitrateBytesPerSecond>(
            QJson::serialized(data).data());

    const auto reflectToFusion =
        QJson::deserialized<BackupBitrateBytesPerSecond>(
            nx::reflect::json::serialize(data));

    ASSERT_TRUE(success);
    ASSERT_EQ(fusionToReflect, data);
    ASSERT_EQ(reflectToFusion, data);
}

} // namespace nx::vms::api::test
