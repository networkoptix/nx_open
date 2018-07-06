#include <gtest/gtest.h>

#include <nx/cloud/cdb/managers/notification.h>

#include "functional_tests/email_manager_mocked.h"
#include "functional_tests/test_setup.h"

namespace nx {
namespace cdb {
namespace test {

namespace {

class NotificationGeneralFieldsValidation:
    public CdbFunctionalTest
{
public:
    NotificationGeneralFieldsValidation()
    {
        init();
    }

    ~NotificationGeneralFieldsValidation()
    {
        fini();
    }

    void havingCreatedAccount()
    {
        m_account = addActivatedAccount2();
        addExpectedNotification(NotificationType::activateAccount, m_account);
    }

    void havingSharedSystem()
    {
        m_account = addActivatedAccount2();
        addExpectedNotification(NotificationType::activateAccount, m_account);

        const auto account2 = addActivatedAccount2();
        addExpectedNotification(NotificationType::activateAccount, account2);

        const auto system = addRandomSystemToAccount(m_account);

        shareSystemEx(m_account, system, account2, api::SystemAccessRole::advancedViewer);
        addExpectedNotification(NotificationType::systemShared, account2);
    }

    void havingRequestedAccountPasswordRestoration()
    {
        m_account = addActivatedAccount2();
        addExpectedNotification(NotificationType::activateAccount, m_account);

        std::string confirmationCode;
        ASSERT_EQ(api::ResultCode::ok, resetAccountPassword(m_account.email, &confirmationCode));
        addExpectedNotification(NotificationType::restorePassword, m_account);
    }

    void havingInvitedUser()
    {
        m_account = addActivatedAccount2();
        addExpectedNotification(NotificationType::activateAccount, m_account);

        const auto system = addRandomSystemToAccount(m_account);

        api::AccountData inviteeAccount;
        inviteeAccount.email = generateRandomEmailAddress();
        inviteeAccount.customization = m_account.customization;
        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(
                m_account, system.id,
                inviteeAccount.email, api::SystemAccessRole::cloudAdmin));
        addExpectedNotification(NotificationType::systemInvite, inviteeAccount);
    }

protected:
    void expectingReceivedNotificationCountToBe(std::size_t desiredNotificationCount)
    {
        ASSERT_EQ(desiredNotificationCount, m_notifications.size());
    }

    void expectingReceivedNotificationsAreValid()
    {
        for (const auto& notification: m_notifications)
            validateNotification(notification);
    }

    void addExpectedNotification(
        NotificationType notificationType,
        const api::AccountData& addressee)
    {
        BasicNotification basicNotification;
        basicNotification.type = notificationType;
        basicNotification.customization = addressee.customization;
        basicNotification.user_email = addressee.email;
        m_expectedNotifications.emplace(notificationType, std::move(basicNotification));
    }

private:
    TestEmailManager m_notificationSender;
    AccountWithPassword m_account;
    std::vector<nx::cdb::BasicNotification> m_notifications;
    std::multimap<NotificationType, BasicNotification> m_expectedNotifications;

    void init()
    {
        using namespace std::placeholders;

        m_notificationSender.setOnReceivedNotification(
            std::bind(&NotificationGeneralFieldsValidation::onReceivedNotification, this, _1));

        EMailManagerFactory::setFactory(
            [this](const conf::Settings& /*settings*/)
            {
                return std::make_unique<EmailManagerStub>(&m_notificationSender);
            });

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void fini()
    {
        EMailManagerFactory::setFactory(nullptr);
    }

    void onReceivedNotification(const nx::cdb::AbstractNotification& notification)
    {
        const auto jsonString = notification.serializeToJson();
        bool deserializationSuccessful = false;
        auto notificationData =
            QJson::deserialized<nx::cdb::BasicNotification>(
                jsonString,
                nx::cdb::BasicNotification(),
                &deserializationSuccessful);
        //validateNotification(notificationData);
        m_notifications.push_back(std::move(notificationData));
    }

    void validateNotification(const nx::cdb::BasicNotification& notification)
    {
        const auto range = m_expectedNotifications.equal_range(notification.type);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (notification == it->second)
                return;
        }

        ASSERT_TRUE(false);
    }
};

} // namespace

TEST_F(NotificationGeneralFieldsValidation, activate_account)
{
    havingCreatedAccount();
    expectingReceivedNotificationsAreValid();
}

TEST_F(NotificationGeneralFieldsValidation, system_shared)
{
    havingSharedSystem();
    expectingReceivedNotificationsAreValid();
}

TEST_F(NotificationGeneralFieldsValidation, restore_password)
{
    havingRequestedAccountPasswordRestoration();
    expectingReceivedNotificationsAreValid();
}

TEST_F(NotificationGeneralFieldsValidation, invite_user_to_system)
{
    havingInvitedUser();
    expectingReceivedNotificationsAreValid();
}

} // namespace test
} // namespace cdb
} // namespace nx
