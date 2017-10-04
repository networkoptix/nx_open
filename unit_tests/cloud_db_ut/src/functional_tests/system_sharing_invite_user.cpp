#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/test_support/utils.h>

#include "email_manager_mocked.h"
#include "test_email_manager.h"
#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class SystemSharingInvitingUser:
    public CdbFunctionalTest
{
public:
    SystemSharingInvitingUser():
        m_testEmailManager(std::bind(
            &SystemSharingInvitingUser::onNotification, this, std::placeholders::_1))
    {
        m_factoryFuncBak = EMailManagerFactory::setFactory(
            [this](const conf::Settings& /*settings*/)
            {
                return std::make_unique<EmailManagerStub>(&m_testEmailManager);
            });

        NX_GTEST_ASSERT_TRUE(startAndWaitUntilStarted());
    }

    ~SystemSharingInvitingUser()
    {
        EMailManagerFactory::setFactory(std::move(m_factoryFuncBak));
    }

protected:
    void givenNotActivatedAccountCreatedByInvite()
    {
        m_ownerAccount = addActivatedAccount2();
        ignoreReceivedNotification();

        m_inviteeEmail = generateRandomEmailAddress();

        m_systems.push_back(addRandomSystemToAccount(m_ownerAccount));
        shareSystemEx(
            m_ownerAccount,
            m_systems.back(),
            m_inviteeEmail,
            api::SystemAccessRole::cloudAdmin);
        popReceivedInviteNotification();
    }

    void whenInvitedAccountToANewSystem()
    {
        m_systems.push_back(addRandomSystemToAccount(m_ownerAccount));
        shareSystemEx(
            m_ownerAccount,
            m_systems.back(),
            m_inviteeEmail,
            api::SystemAccessRole::cloudAdmin);
    }

    void thenInviteToANewSystemNotificationMustBeReceived()
    {
        auto notification = popReceivedInviteNotification();
        ASSERT_EQ(NotificationType::systemInvite, notification->type);
        ASSERT_EQ(m_systems.back().id, notification->message.system_id);
        ASSERT_EQ(m_inviteeEmail, notification->user_email);
    }

    void ignoreReceivedNotification()
    {
        ASSERT_FALSE(m_notifications.empty());
        m_notifications.pop_front();
    }

    std::unique_ptr<InviteUserNotification> popReceivedInviteNotification()
    {
        EXPECT_FALSE(m_notifications.empty());

        auto notification = std::move(m_notifications.front());
        EXPECT_NE(nullptr, notification);
        m_notifications.pop_front();
        return notification;
    }

private:
    TestEmailManager m_testEmailManager;
    EMailManagerFactory::FactoryFunc m_factoryFuncBak;
    AccountWithPassword m_ownerAccount;
    std::string m_inviteeEmail;
    std::deque<std::unique_ptr<InviteUserNotification>> m_notifications;
    std::deque<api::SystemData> m_systems;

    void onNotification(const AbstractNotification& notification)
    {
        const auto* inviteNotificationPtr =
            dynamic_cast<const InviteUserNotification*>(&notification);
        if (inviteNotificationPtr)
        {
            m_notifications.push_back(
                std::make_unique<InviteUserNotification>(*inviteNotificationPtr));
        }
        else
        {
            m_notifications.push_back(nullptr);
        }
    }
};

TEST_F(SystemSharingInvitingUser, inviting_not_active_account_to_another_system)
{
    givenNotActivatedAccountCreatedByInvite();
    whenInvitedAccountToANewSystem();
    thenInviteToANewSystemNotificationMustBeReceived();
}

TEST_F(SystemSharingInvitingUser, basic_scenario)
{
    const auto account1 = addActivatedAccount2();
    ignoreReceivedNotification();

    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1.password, &system1));

    const std::string newAccountEmail = generateRandomEmailAddress();
    std::string newAccountPassword = "new_password";
    const auto newAccountAccessRoleInSystem1 = api::SystemAccessRole::cloudAdmin;

    shareSystemEx(account1, system1, newAccountEmail, newAccountAccessRoleInSystem1);
    auto inviteNotification = popReceivedInviteNotification();

    std::vector<api::SystemSharingEx> sharings;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystemSharings(account1.email, account1.password, &sharings));
    ASSERT_EQ(2U, sharings.size());

    ASSERT_EQ(
        api::SystemAccessRole::owner,
        accountAccessRoleForSystem(sharings, account1.email, system1.id));
    ASSERT_EQ(
        api::SystemAccessRole::cloudAdmin,
        accountAccessRoleForSystem(sharings, newAccountEmail, system1.id));

    // Confirmation code has format: base64(tmp_password:email).
    const auto tmpPasswordAndEmail =
        QByteArray::fromBase64(QByteArray::fromRawData(
            inviteNotification->message.code.data(),
            inviteNotification->message.code.size()));
    const std::string tmpPassword =
        tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

    // Setting new password.
    api::AccountUpdateData update;
    update.passwordHa1 = nx_http::calcHa1(
        newAccountEmail.c_str(),
        moduleInfo().realm.c_str(),
        newAccountPassword.c_str()).constData();
    ASSERT_EQ(
        api::ResultCode::ok,
        updateAccount(newAccountEmail, tmpPassword, update));

    api::AccountData newAccount;
    ASSERT_EQ(
        api::ResultCode::ok,
        getAccount(newAccountEmail, newAccountPassword, &newAccount));
    ASSERT_EQ(api::AccountStatus::activated, newAccount.statusCode);

    // Checking new account access to system1.
    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(newAccountEmail, newAccountPassword, &systems));
    ASSERT_EQ(1U, systems.size());
    ASSERT_EQ(system1.id, systems[0].id);
    ASSERT_EQ(newAccountAccessRoleInSystem1, systems[0].accessRole);
}

} // namespace test
} // namespace cdb
} // namespace nx
