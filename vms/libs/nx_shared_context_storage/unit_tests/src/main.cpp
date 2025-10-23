// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/sdk/helpers/string.h>
#include <nx/shared_context_storage/shared_context_storage.h>
#include <nx/shared_context_storage/utils.h>

namespace nx::shared_context_storage::test {

class UtilityProvider: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider>
{
public:
    UtilityProvider() {}

    virtual int64_t vmsSystemTimeSinceEpochMs() const override { return 0; }
    virtual const nx::sdk::IString* getHomeDir() const override { return new nx::sdk::String(); }
    virtual const char* serverId() const override { return ""; }
    virtual const char* cloudToken() const override { return ""; }
    virtual nx::sdk::IString* cloudOrgId() const override { return new nx::sdk::String(); }
    virtual nx::sdk::IString* cloudSystemId() const override { return new nx::sdk::String(); }
    virtual nx::sdk::IString* cloudAuthKey() const override { return new nx::sdk::String(); }
    virtual nx::sdk::IString* supportedVectorizationModels() const override
    {
        return new nx::sdk::String();
    }

    virtual nx::sdk::IString* getDataDir() const override { return new nx::sdk::String(); }
    virtual nx::sdk::IString* getSharedContextValue(
        const char* /*id*/, const char* /*name*/) const override
    {
        return new nx::sdk::String("[\"1\", \"2\", \"3\"]");
    }

    virtual void subscribeForCloudTokenUpdate(
        nx::sdk::ICloudTokenSubscriber* /*subscriber*/) override {}

    virtual const nx::sdk::IString* getServerSdkVersion() const override
    {
        return new nx::sdk::String(nx::sdk::sdkVersion());
    }

    virtual void doSendHttpRequest(
        HttpDomainName /*requestDomain*/,
        const char* /*url*/,
        const char* /*httpMethod*/,
        const char* /*mimeType*/,
        const char* /*requestBody*/,
        IHttpRequestCompletionHandler* /*callback*/) const override
    {
    }
};

TEST(SharedContextStorageTest, writeAndReadData)
{
    SharedContextStorage storage = SharedContextStorage();

    storage.writeData("id1", "string1", std::string("value1"));
    storage.writeData("id1", "string2", std::string("value2"));
    storage.writeData("id2", "vector", std::vector<int>{1, 2, 3});
    storage.writeData("id2", "int", 1);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Read existing values, preserving their types.

    auto stringValue1 = storage.readData<std::string>("id1", "string1");
    ASSERT_TRUE(stringValue1.has_value());
    ASSERT_EQ(stringValue1.value(), "value1");

    auto stringValue2 = storage.readData<std::string>("id1", "string2");
    ASSERT_TRUE(stringValue2.has_value());
    ASSERT_EQ(stringValue2.value(), "value2");

    auto vecValue = storage.readData<std::vector<int>>("id2", "vector");
    ASSERT_TRUE(vecValue.has_value());
    ASSERT_EQ(vecValue.value(), (std::vector<int>{1, 2, 3}));

    auto intValue = storage.readData<int>("id2", "int");
    ASSERT_TRUE(intValue.has_value());
    ASSERT_EQ(intValue.value(), 1);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Read a non-existing value

    auto missingValue = storage.readData<std::string>("id1", "unknown");
    ASSERT_FALSE(missingValue.has_value());
    ASSERT_EQ(
        missingValue.error(),
        "Shared context storage has no value for id \"id1\", name \"unknown\"");

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Read stored value as a raw string

    auto rawVectorValue = storage.readData<std::string>("id2", "vector");
    ASSERT_TRUE(rawVectorValue.has_value());
    ASSERT_EQ(rawVectorValue.value(), "[1,2,3]");

    auto rawIntValue = storage.readData<std::string>("id2", "int");
    ASSERT_TRUE(rawIntValue.has_value());
    ASSERT_EQ(rawIntValue.value(), "1");


}

TEST(SharedContextStoreTest, deleteData)
{
    SharedContextStorage storage = SharedContextStorage();

    // Write some data
    storage.writeData("id1", "key1", std::string("value1"));
    storage.writeData("id1", "key2", std::string("value2"));

    // Ensure data exists
    ASSERT_TRUE(storage.readData<std::string>("id1", "key1").has_value());
    ASSERT_TRUE(storage.readData<std::string>("id1", "key2").has_value());

    // Delete one key
    storage.deleteData("id1", "key1");

    // Verify deletion
    ASSERT_FALSE(storage.readData<std::string>("id1", "key1").has_value());

    // Ensure other data is unaffected
    ASSERT_TRUE(storage.readData<std::string>("id1", "key2").has_value());
}

TEST(SharedContextStorageTest, readDataUsingUtilityProvider)
{
    nx::sdk::Ptr<nx::sdk::IUtilityProvider> utilityProvider(new UtilityProvider());

    auto vecValueOpt = Utils::readDataUsingUtilityProvider<std::vector<std::string>>(
        utilityProvider, "any_id", "any_name");

    ASSERT_TRUE(vecValueOpt.has_value());
    const auto vecValue = vecValueOpt.value();
    ASSERT_EQ(vecValue, (std::vector<std::string>{"1", "2", "3"}));
}

} // namespace nx::shared_context_storage
