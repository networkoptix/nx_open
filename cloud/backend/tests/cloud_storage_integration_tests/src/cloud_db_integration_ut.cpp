#include "test_fixture.h"

namespace nx::cloud::storage::service::test {

using namespace nx::cloud::storage::service::api;

class CloudDbIntegration: public TestFixture
{
protected:
	TestContext givenCloudAccountWithStorage()
	{
		auto testContext = initializeTest();
		addBucket(&testContext);
		addStorage(&testContext);
		return testContext;
	}

	TestContext givenCloudSystemWithStorage()
	{
		auto cloudAccount = givenCloudAccountWithStorage();
		addSystem(&cloudAccount);
		return cloudAccount;
	}

	TestContext givenCloudStorage()
	{
		return initializeTest();
	}

	TestContext givenCloudStorageWithoutSystem()
	{
		return givenCloudAccountWithStorage();
	}

	TestContext givenCloudSystemWithSystemCredentials()
	{
		auto testContext = initializeTestWithSystemCredentials();
		addBucket(&testContext);
		addStorage(&testContext);
		addSystem(&testContext);
		return testContext;
	}

	std::unique_ptr<Client> givenMediaserverWithSystemCredentials(const TestContext& testContext)
	{
		return makeStorageServiceClient(
			testContext.storageServiceUrl,
			testContext.system.id,
			testContext.system.authKey);
	}

	void shareSystem(
		const TestContext& grantor,
		const TestContext& receiver,
		cloud::db::api::SystemAccessRole accessLevel)
	{
		cloudDb().shareSystem(
			grantor.cloudDbAccount,
			grantor.system.id,
			receiver.cloudDbAccount.email,
			accessLevel);
	}

	void whenReadStorage(TestContext* cloudAccount, const Storage& storage)
	{
		readStorage(cloudAccount->storageServiceClient.get(), storage.id);
	}

	void whenRemoveStorage(TestContext* cloudAccount, const Storage& storage)
	{
		removeStorage(cloudAccount->storageServiceClient.get(), storage.id);
	}

	void whenRequestMediaContentCredentials(TestContext* cloudAccount, const Storage& storage)
	{
		getCredentials(cloudAccount->storageServiceClient.get(), storage.id);
	}

	void whenMediaserverRequestsMediaContentCredentials(
		const std::unique_ptr<Client>& mediaserver,
		const Storage& storage)
	{
		getCredentials(mediaserver.get(), storage.id);
	}

	void thenReadStorageResponseIs(ResultCode resultCode)
	{
		auto [result, storage] = waitForReadStorageResponse();
		ASSERT_EQ(resultCode, result.resultCode);
	}

	void thenRemoveStorageResponseIs(ResultCode resultCode)
	{
		auto result = waitForRemoveStorageResponse();
		ASSERT_EQ(resultCode, result.resultCode);
	}

	void thenRequestMediaContentCredentialsResponseIs(
		ResultCode resultCode,
		api::StorageCredentials* outStorageCredentials = nullptr)
	{
		auto [result, storageCredentials] = waitForGetCredentialsResponse();
		ASSERT_EQ(resultCode, result.resultCode);
		if (outStorageCredentials)
			*outStorageCredentials = std::move(storageCredentials);
	}
};

TEST_F(CloudDbIntegration, storage_owner_can_read_storage)
{
	auto account = givenCloudAccountWithStorage();

	whenReadStorage(&account, account.storage);

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, storage_owner_can_remove_storage_if_system_is_not_added)
{
	auto account = givenCloudStorageWithoutSystem();

	whenRemoveStorage(&account, account.storage);

	thenRemoveStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, non_owner_cannot_remove_storage)
{
	auto accountWithStorage = givenCloudAccountWithStorage();
	auto otherAccount = givenCloudStorage();

	whenRemoveStorage(&otherAccount, accountWithStorage.storage);

	thenRemoveStorageResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, user_can_access_storage_content_of_a_shared_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudStorage();
	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::viewer);

	whenReadStorage(&otherAccount, accountWithSystem.storage);

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_read_storage_if_shared_level_is_disabled)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudStorage();

	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::disabled);

	whenReadStorage(&otherAccount, accountWithSystem.storage);

	thenReadStorageResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, storage_owner_can_request_media_content_credentials)
{
	auto account = givenCloudAccountWithStorage();

	whenRequestMediaContentCredentials(&account, account.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_can_request_media_content_credentials_of_shared_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudStorage();

	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::viewer);

	whenRequestMediaContentCredentials(&otherAccount, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_media_content_credentials_if_system_shared_level_is_disabled)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudStorage();

	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::disabled);

	whenRequestMediaContentCredentials(&otherAccount, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, mediaserver_can_access_content_on_cloud_storage)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserverWithSystemCredentials(accountWithSystem);

	whenMediaserverRequestsMediaContentCredentials(mediaserver, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::ok);
}

} // namespace nx::cloud::storage::service::test