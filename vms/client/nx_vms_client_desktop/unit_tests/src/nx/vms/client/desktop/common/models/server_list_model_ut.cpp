// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QObject>
#include <QtQml/QQmlComponent>
#include <QtTest/QAbstractItemModelTester>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/current_system_servers.h>
#include <nx/vms/client/desktop/system_administration/models/server_list_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

using TestModel = ServerListModel;

class ServerListModelTest: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        QQmlComponent component(appContext()->qmlEngine());
        component.setData("import nx.vms.client.desktop; ServerListModel {}", {});
        const auto model = qobject_cast<ServerListModel*>(component.create());
        m_testModel.reset(model);
        m_modelTester.reset(new QAbstractItemModelTester(m_testModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));
    }

    virtual void TearDown() override
    {
        m_modelTester.reset();
        m_testModel.reset();
        resourcePool()->clear();
    }

    QnMediaServerResourcePtr registerNewServer(const QString& name)
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource);
        server->setIdUnsafe(nx::Uuid::createUuid());
        server->setName(name);
        resourcePool()->addNewResources({server});
        return server;
    }

public:
    std::unique_ptr<TestModel> m_testModel;
    std::unique_ptr<QAbstractItemModelTester> m_modelTester;
    std::unique_ptr<CurrentSystemServers> m_servers;
};

// ------------------------------------------------------------------------------------------------

TEST_F(ServerListModelTest, rowCount1)
{
    ASSERT_EQ(m_testModel->rowCount({}), 1);
}

TEST_F(ServerListModelTest, rowCount2)
{
    const auto server1 = registerNewServer("Server 1");
    ASSERT_EQ(m_testModel->rowCount({}), 2);

    const auto server2 = registerNewServer("Server 2");
    ASSERT_EQ(m_testModel->rowCount({}), 3);

    resourcePool()->removeResource(server1);
    ASSERT_EQ(m_testModel->rowCount({}), 2);

    resourcePool()->removeResource(server2);
    ASSERT_EQ(m_testModel->rowCount({}), 1);
}

TEST_F(ServerListModelTest, signalEmission)
{
    QStringList signalLog;
    QObject::connect(m_testModel.get(), &QAbstractItemModel::rowsAboutToBeInserted,
        [this, &signalLog](const QModelIndex& /*parent*/, int first, int last)
        {
            signalLog << nx::format(
                "rowsAboutToBeInserted(%1, %2) |%3", first, last, m_testModel->rowCount());
        });

    QObject::connect(m_testModel.get(), &QAbstractItemModel::rowsInserted,
        [this, &signalLog](const QModelIndex& /*parent*/, int first, int last)
        {
            signalLog <<
                nx::format("rowsInserted(%1, %2) |%3", first, last, m_testModel->rowCount());
        });

    QObject::connect(m_testModel.get(), &QAbstractItemModel::rowsAboutToBeRemoved,
        [this, &signalLog](const QModelIndex& /*parent*/, int first, int last)
        {
            signalLog << nx::format(
                "rowsAboutToBeRemoved(%1, %2) |%3", first, last, m_testModel->rowCount());
        });

    QObject::connect(m_testModel.get(), &QAbstractItemModel::rowsRemoved,
        [this, &signalLog](const QModelIndex& /*parent*/, int first, int last)
        {
            signalLog <<
                nx::format("rowsRemoved(%1, %2) |%3", first, last, m_testModel->rowCount());
        });

    QObject::connect(m_testModel.get(), &QAbstractItemModel::dataChanged,
        [&signalLog](const QModelIndex& topLeft, const QModelIndex& bottomRight,
            const QList<int>& /*roles*/)
        {
            signalLog << nx::format("dataChanged(%1, %2)", topLeft.row(), bottomRight.row());
        });

    QObject::connect(m_testModel.get(), &ServerListModel::serverRemoved,
        [&signalLog]()
        {
            signalLog << "serverRemoved()";
        });

    const auto server1 = registerNewServer("Server A1");
    ASSERT_EQ(signalLog, QStringList({"rowsAboutToBeInserted(1, 1) |1", "rowsInserted(1, 1) |2"}));
    signalLog.clear();

    const auto server2 = registerNewServer("Server A2");
    ASSERT_EQ(signalLog, QStringList({"rowsAboutToBeInserted(2, 2) |2", "rowsInserted(2, 2) |3"}));
    signalLog.clear();

    const auto server3 = registerNewServer("Server A3");
    ASSERT_EQ(signalLog, QStringList({"rowsAboutToBeInserted(3, 3) |3", "rowsInserted(3, 3) |4"}));
    signalLog.clear();

    resourcePool()->removeResources({server2});
    ASSERT_EQ(signalLog,
        QStringList({"rowsAboutToBeRemoved(2, 2) |4", "rowsRemoved(2, 2) |3", "serverRemoved()"}));
    signalLog.clear();

    server3->setName("Server B3");
    ASSERT_EQ(signalLog, QStringList({"dataChanged(2, 2)"}));
    signalLog.clear();

    resourcePool()->removeResources({server3});
    ASSERT_EQ(signalLog,
        QStringList({"rowsAboutToBeRemoved(2, 2) |3", "rowsRemoved(2, 2) |2", "serverRemoved()"}));
    signalLog.clear();

    resourcePool()->removeResources({server1});
    ASSERT_EQ(signalLog,
        QStringList({"rowsAboutToBeRemoved(1, 1) |2", "rowsRemoved(1, 1) |1", "serverRemoved()"}));
}

TEST_F(ServerListModelTest, data)
{
    const auto server = registerNewServer("Server");
    const auto index = m_testModel->index(1);
    ASSERT_EQ(index.data(), server->getName());
    ASSERT_EQ(index.data(ServerListModel::idRole), QVariant::fromValue(server->getId()));
}

TEST_F(ServerListModelTest, indexOf)
{
    const auto server1 = registerNewServer("Server 01");
    const auto server2 = registerNewServer("Server 02");
    ASSERT_EQ(m_testModel->indexOf(nx::Uuid::createUuid()), -1);
    ASSERT_EQ(m_testModel->indexOf(nx::Uuid()), 0);
    ASSERT_EQ(m_testModel->indexOf(server1->getId()), 1);
    ASSERT_EQ(m_testModel->indexOf(server2->getId()), 2);
}

TEST_F(ServerListModelTest, serverIdAt)
{
    const auto server1 = registerNewServer("Server 1");
    const auto server2 = registerNewServer("Server 2");
    ASSERT_EQ(m_testModel->serverIdAt(0), nx::Uuid());
    ASSERT_EQ(m_testModel->serverIdAt(1), server1->getId());
    ASSERT_EQ(m_testModel->serverIdAt(2), server2->getId());
    ASSERT_EQ(m_testModel->serverIdAt(3), nx::Uuid());
}

TEST_F(ServerListModelTest, serverName)
{
    const auto server1 = registerNewServer("Server A");
    const auto server2 = registerNewServer("Server B");
    ASSERT_EQ(m_testModel->serverName(0), "Auto");
    ASSERT_EQ(m_testModel->serverName(1), server1->getName());
    ASSERT_EQ(m_testModel->serverName(2), server2->getName());
    ASSERT_EQ(m_testModel->serverName(3), "");
}

} // namespace test
} // namespace nx::vms::client::desktop
