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

	TestContext givenCloudAccount()
	{
		return initializeTest();
	}

	TestContext givenCloudStorageWithoutSystem()
	{
		return givenCloudAccountWithStorage();
	}

	std::unique_ptr<Client> givenMediaserver(const TestContext& testContext)
	{
		return makeStorageServiceClient(
			testContext.storageServiceUrl,
			testContext.system.id,
			testContext.system.authKey);
	}

	TestContext givenCloudAccountWithSystemCredentials()
	{
		return initializeTestWithSystemCredentials();
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

	void whenAddStorage(const std::unique_ptr<api::Client>& client)
	{
		addStorage(client, {1000, "us-east-1"});
	}

	void whenReadStorage(TestContext* cloudAccount, const Storage& storage)
	{
		readStorage(cloudAccount->storageServiceClient, storage.id);
	}

	void whenRemoveStorage(const std::unique_ptr<Client>& client, const Storage& storage)
	{
		removeStorage(client, storage.id);
	}

	void whenRemoveStorage(TestContext* cloudAccount, const Storage& storage)
	{
		removeStorage(cloudAccount->storageServiceClient, storage.id);
	}

	void whenAddSystem(
		const std::unique_ptr<api::Client>& client,
		const Storage& storage,
		const cloud::db::api::SystemData& system)
	{
		addSystem(client, storage.id, {system.id});
	}

	void whenRemoveSystem(
		const std::unique_ptr<api::Client>& client,
		const Storage& storage,
		const cloud::db::api::SystemData& system)
	{
		removeSystem(client, storage.id, system.id);
	}

	void whenRequestMediaContentCredentials(
		const std::unique_ptr<Client>& client,
		const Storage& storage)
	{
		requestMediaContentCredentials(client, storage.id);
	}

	void whenRequestMediaContentCredentials(TestContext* cloudAccount, const Storage& storage)
	{
		requestMediaContentCredentials(cloudAccount->storageServiceClient, storage.id);
	}

	void thenAddStorageResponseIs(ResultCode resultCode)
	{
		auto [result, storage] = waitForAddStorageResponse();
		ASSERT_EQ(resultCode, result.resultCode);
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

	void thenAddSystemResponseIs(ResultCode resultCode)
	{
		auto [result, system] = waitForAddSystemResponse();
		ASSERT_EQ(resultCode, result.resultCode);
		(void) system;
	}

	void thenRemoveSystemResponseIs(ResultCode resultCode)
	{
		auto result = waitForRemoveSystemResponse();
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
	auto otherAccount = givenCloudAccount();

	whenRemoveStorage(&otherAccount, accountWithStorage.storage);

	thenRemoveStorageResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, user_can_access_storage_content_of_a_shared_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudAccount();
	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::viewer);

	whenReadStorage(&otherAccount, accountWithSystem.storage);

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_read_storage_if_shared_level_is_disabled)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudAccount();

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
	auto otherAccount = givenCloudAccount();

	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::viewer);

	whenRequestMediaContentCredentials(&otherAccount, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_media_content_credentials_if_system_shared_level_is_disabled)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto otherAccount = givenCloudAccount();

	shareSystem(accountWithSystem, otherAccount, cloud::db::api::SystemAccessRole::disabled);

	whenRequestMediaContentCredentials(&otherAccount, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, mediaserver_can_access_content_of_shared_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserver(accountWithSystem);

	whenRequestMediaContentCredentials(mediaserver, accountWithSystem.storage);

	thenRequestMediaContentCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, mediaserver_cannot_add_storage)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserver(accountWithSystem);

	whenAddStorage(mediaserver);

	thenAddStorageResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, mediaserver_cannot_remove_storage)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserver(accountWithSystem);

	whenRemoveStorage(mediaserver, accountWithSystem.storage);

	thenRemoveStorageResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, mediaserver_cannot_add_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserver(accountWithSystem);

	whenAddSystem(mediaserver, accountWithSystem.storage, accountWithSystem.system);

	thenAddSystemResponseIs(ResultCode::forbidden);
}

TEST_F(CloudDbIntegration, mediaserver_cannot_remove_system)
{
	auto accountWithSystem = givenCloudSystemWithStorage();
	auto mediaserver = givenMediaserver(accountWithSystem);

	whenRemoveSystem(mediaserver, accountWithSystem.storage, accountWithSystem.system);

	thenRemoveSystemResponseIs(ResultCode::forbidden);
}

} // namespace nx::cloud::storage::service::test
