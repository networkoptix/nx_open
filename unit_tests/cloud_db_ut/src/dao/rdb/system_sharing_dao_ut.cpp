#include <gtest/gtest.h>

#include <dao/rdb/system_sharing_data_object.h>

#include "base_persistent_data_test.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {
namespace test {

using BasePersistentDataTest = cdb::test::BasePersistentDataTest;

class SystemSharingDataObject:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    SystemSharingDataObject()
    {
        m_ownerAccount = insertRandomAccount();
        m_system = insertRandomSystem(m_ownerAccount);
        m_userAccount = insertRandomAccount();
    }

protected:
    void insertSystemSharing()
    {
        insertOrReplaceSystemSharing(api::SystemAccessRole::cloudAdmin);
    }

    void updateSystemSharing()
    {
        insertOrReplaceSystemSharing(api::SystemAccessRole::advancedViewer);
    }

    void verifySharingRecordHasBeenReplaced()
    {
        using namespace std::placeholders;

        std::deque<api::SystemSharingEx> sharings;
        const auto dbResult = executeSelectQuerySync(
            std::bind(&dao::rdb::SystemSharingDataObject::fetchAllUserSharings, systemSharingDao(), _1, _2),
            &sharings);
        ASSERT_EQ(db::DBResult::ok, dbResult);

        ASSERT_EQ(1, sharings.size());
        ASSERT_EQ(api::SystemAccessRole::advancedViewer, sharings[0].accessRole);
    }

private:
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AccountData m_userAccount;

    void insertOrReplaceSystemSharing(api::SystemAccessRole accessRole)
    {
        api::SystemSharingEx sharing;
        sharing.accountEmail = m_userAccount.email;
        sharing.systemId = m_system.id;
        sharing.accountId = m_ownerAccount.id;
        sharing.accessRole = accessRole;
        BasePersistentDataTest::insertSystemSharing(sharing);
    }
};

TEST_F(SystemSharingDataObject, system_sharing_update_replaces_record)
{
    insertSystemSharing();
    updateSystemSharing();
    verifySharingRecordHasBeenReplaced();
}

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
