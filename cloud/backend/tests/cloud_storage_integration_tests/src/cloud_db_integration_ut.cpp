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

	void givenSharedCloudDbSystem(nx::cloud::db::api::SystemAccessRole systemAccessRole)
	{
		givenCloudStorage();

		auto cloudDbAccount = cloudDb().addActivatedAccount2();

		cloudDb().shareSystem(
			m_testContext.cloudDbAccount,
			m_testContext.system.id,
			cloudDbAccount.email,
			systemAccessRole);

		m_testContext2.cloudDbAccount = std::move(cloudDbAccount);
	}

	void givenSecondCloudDbAccount()
	{
		m_testContext2.cloudDbAccount = cloudDb().addActivatedAccount2();
	}

	void whenSecondCloudDBAccountTriesToRemoveStorageOwnedByFirstAccount()
	{
		m_testContext2.storageServiceClient = makeStorageServiceClient(
			storageService().httpUrl(),
			m_testContext2.cloudDbAccount);

		removeStorage(m_testContext2.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenSecondCloudStorageUserReadsStorageFromFirst()
	{
		m_testContext2.storageServiceClient = makeStorageServiceClient(
			storageService().httpUrl(),
			m_testContext2.cloudDbAccount);

		readStorage(
			m_testContext2.storageServiceClient.get(),
			m_testContext.storage.id);
	}

	void whenReadStorage()
	{
		readStorage(m_testContext.storageServiceClient.get(), m_testContext.storage.id);
	}

	void whenRemoveStorage()
	{
		removeStorage(m_testContext.storageServiceClient.get(), m_testContext.storage.id);
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
	givenSharedCloudDbSystem(nx::cloud::db::api::SystemAccessRole::liveViewer);

	whenSecondCloudStorageUserReadsStorageFromFirst();

	thenReadStorageResponseIs(ResultCode::ok);
}

TEST_F(CloudDbIntegration, user_is_denied_read_storage_if_shared_level_is_disabled)
{
	givenSharedCloudDbSystem(nx::cloud::db::api::SystemAccessRole::disabled);

	whenSecondCloudStorageUserReadsStorageFromFirst();

	thenReadStorageResponseIs(ResultCode::unauthorized);
}

} // namespace nx::cloud::storage::service::test