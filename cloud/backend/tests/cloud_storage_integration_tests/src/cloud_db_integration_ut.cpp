#include "test_fixture.h"

namespace nx::cloud::storage::service::test {

using namespace nx::cloud::storage::service::api;

class CloudDbIntegration: public TestFixture
{
protected:
	void givenCloudStorage()
	{
		m_testContext = initializeTest();
	}

	void givenCloudStorageWithoutSystem()
	{
		m_testContext = initializeTest(InitializeFlags::bucket | InitializeFlags::storage);
	}

	void givenSharedCloudDbSystem(
		nx::cloud::db::api::SystemAccessRole systemAccessRole =
		    nx::cloud::db::api::SystemAccessRole::liveViewer)
	{
		givenCloudStorage();

		auto cloudDbAccount = cloudDb().addActivatedAccount2();

		cloudDb().shareSystem(
			m_testContext.cloudDbAccount,
			m_testContext.system.id,
			cloudDbAccount.email,
			systemAccessRole);

		m_testContext2.cloudDbAccount = std::move(cloudDbAccount);
		m_testContext2.storageServiceClient = makeStorageServiceClient(
			storageService().httpUrl(),
			m_testContext2.cloudDbAccount);
	}

	void givenSecondCloudDbAccount()
	{
		m_testContext2.cloudDbAccount = cloudDb().addActivatedAccount2();
		m_testContext2.storageServiceClient = makeStorageServiceClient(
			storageService().httpUrl(),
			m_testContext2.cloudDbAccount);
	}

	void whenSecondCloudDBAccountTriesToRemoveStorageOwnedByFirstAccount()
	{
		removeStorage(m_testContext2.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenSecondUserReadsStorageFromFirst()
	{
		readStorage(
			m_testContext2.storageServiceClient.get(),
			m_testContext.storage.id);
	}

	void whenSecondUserRequestsCredentialsForFirstUserStorage()
	{
		getCredentials(m_testContext2.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenReadStorage()
	{
		readStorage(m_testContext.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenRemoveStorage()
	{
		removeStorage(m_testContext.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenRequestCredentials()
	{
		getCredentials(m_testContext.storageServiceClient.get(), m_testContext.storage.id);
	}

	void thenReadStorageResponseIs(ResultCode resultCode)
	{
		auto [result, storage] = waitForReadStorageResponse();
		ASSERT_EQ(resultCode, result.resultCode);
		if (result.ok())
		{
			ASSERT_EQ(storage.id, m_testContext.storage.id);
		}
	}

	void thenRemoveStorageResponseIs(ResultCode resultCode)
	{
		auto result = waitForRemoveStorageResponse();
		ASSERT_EQ(resultCode, result.resultCode);
	}

	void thenRequestCredentialsResponseIs(ResultCode resultCode)
	{
		auto [result, storageCredentials] = waitForGetCredentialsResponse();
		ASSERT_EQ(resultCode, result.resultCode);
		(void) storageCredentials;
	}

private:
	TestContext m_testContext;
	TestContext m_testContext2;
};

TEST_F(CloudDbIntegration, storage_owner_can_read_storage)
{
	givenCloudStorage();

	whenReadStorage();

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, storage_owner_can_remove_storage_if_system_is_not_added)
{
	givenCloudStorageWithoutSystem();

	whenRemoveStorage();

	thenRemoveStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, non_owner_cannot_remove_storage)
{
	givenCloudStorage();
	givenSecondCloudDbAccount();

	whenSecondCloudDBAccountTriesToRemoveStorageOwnedByFirstAccount();

	thenRemoveStorageResponseIs(ResultCode::unauthorized);
}

TEST_F(CloudDbIntegration, user_can_read_storage_if_system_is_shared_with_them)
{
	givenSharedCloudDbSystem();

	whenSecondUserReadsStorageFromFirst();

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_read_storage_if_shared_level_is_disabled)
{
	givenSharedCloudDbSystem(nx::cloud::db::api::SystemAccessRole::disabled);

	whenSecondUserReadsStorageFromFirst();

	thenReadStorageResponseIs(ResultCode::unauthorized);
}

TEST_F(CloudDbIntegration, storage_owner_can_request_credentials)
{
	givenCloudStorage();

	whenRequestCredentials();

	thenRequestCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_can_request_credentials_if_system_is_shared_with_them)
{
	givenSharedCloudDbSystem();

	whenSecondUserRequestsCredentialsForFirstUserStorage();

	thenRequestCredentialsResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_denied_credentials_request_if_system_shared_level_is_disabled)
{
	givenSharedCloudDbSystem(nx::cloud::db::api::SystemAccessRole::disabled);

	whenSecondUserRequestsCredentialsForFirstUserStorage();

	thenRequestCredentialsResponseIs(ResultCode::unauthorized);
}

} // namespace nx::cloud::storage::service::test