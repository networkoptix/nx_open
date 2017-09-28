#include "media_server_process.h"
#include <test_support/utils.h>
#include "utils/common/util.h"
#include <core/resource/user_resource.h>
#include "core/resource_management/resource_pool.h"
#include <server_query_processor.h>
#include <transaction/transaction.h>
#include <nx/utils/std/future.h>
#include <test_support/mediaserver_launcher.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#include <future>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>


TEST(ExecActionAccessRightsTest, main) //< Crash on QnDbManager nullptr
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    auto createUser = [&launcher](const QString& name, Qn::GlobalPermission permissions)
    {
        QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setRawPermissions(permissions);
        launcher.commonModule()->resourcePool()->addResource(user);
    };

    auto findUserByName = [&launcher](const QString& name)
    {
        for (const auto& user: launcher.commonModule()->resourcePool()->getResources<QnUserResource>())
            if (user->getName() == name)
                return user;
        return QnUserResourcePtr();
    };

    auto executeTransaction = [&launcher](const Qn::UserAccessData& userAccess)
    {
        ec2::QnTransaction<ec2::ApiBusinessActionData> actionTran(
            ec2::ApiCommand::execAction,
            launcher.commonModule()->moduleGUID());
        nx::utils::promise<ec2::ErrorCode> resultPromise;
        nx::utils::future<ec2::ErrorCode> resultFuture = resultPromise.get_future();

        ec2::QnJsonTransactionSerializer jsonTranSerializer;
        ec2::QnUbjsonTransactionSerializer ubjsonTranSerializer;
        ec2::TransactionMessageBusAdapter messageBus(
            nullptr/*QnDbManager*/,
            Qn::PT_Server,
            launcher.commonModule(),
            &jsonTranSerializer,
            &ubjsonTranSerializer);
        messageBus.init(ec2::MessageBusType::LegacyMode);

        ec2::ServerQueryProcessorAccess access(nullptr/*QnDbManager*/, &messageBus);
        access.getAccess(userAccess).processUpdateAsync(
                actionTran,
                [&resultPromise](ec2::ErrorCode ecode)
                {
                    resultPromise.set_value(ecode);
                });

        return resultFuture.get();
    };

    createUser(lit("Vasya"), Qn::GlobalUserInputPermission);
    createUser(lit("Admin"), Qn::GlobalAdminPermission);
    createUser(lit("Petya"), Qn::GlobalLiveViewerPermissionSet);

    auto vasya = findUserByName("Vasya");
    auto admin = findUserByName("Admin");
    auto petya = findUserByName("Petya");

    ASSERT_TRUE((bool)vasya);
    ASSERT_TRUE((bool)admin);
    ASSERT_TRUE((bool)petya);

    Qn::UserAccessData vasyaAccess(vasya->getId());
    Qn::UserAccessData adminAccess(admin->getId());
    Qn::UserAccessData petyaAccess(petya->getId());

    ec2::ErrorCode ecode = executeTransaction(vasyaAccess);
    ASSERT_NE(ecode, ec2::ErrorCode::forbidden);

    ecode = executeTransaction(adminAccess);
    ASSERT_NE(ecode, ec2::ErrorCode::forbidden);

    ecode = executeTransaction(petyaAccess);
    ASSERT_EQ(ecode, ec2::ErrorCode::forbidden);
}
