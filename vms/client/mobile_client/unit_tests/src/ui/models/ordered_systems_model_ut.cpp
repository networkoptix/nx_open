// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QVector>

#include <gtest/gtest.h>

#include <nx/utils/math/math.h>
#include <ui/models/ordered_systems_model.h>
#include <ui/models/systems_model.h>

namespace nx::vms::client::core {
namespace test {

namespace {

// Dummy structure that represents a single system.
struct SystemInfo
{
    QString name;
    QString systemId;
    QString localId;
    QString address;
    bool isNew;
    bool isCloud;
    bool isOnline;

    bool operator==(const SystemInfo& other) const
    {
        return other.name == name &&
            other.systemId == systemId &&
            other.localId == localId&&
            other.address == address &&
            other.isNew == isNew &&
            other.isCloud == isCloud &&
            other.isOnline == isOnline;
    }
};

void PrintTo(const SystemInfo& val, ::std::ostream* os)
{
    *os << val.name.toStdString() << "[" << val.systemId.toStdString() << "]";
}

// A mock class for QnSystemsModel that implements Qt list model.
class MockSystemsModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

public:
    MockSystemsModel(QObject* parent = nullptr) : base_type(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        EXPECT_FALSE(parent.isValid());
        return m_systems.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || !qBetween<int>(QnSystemsModel::FirstRoleId, role, QnSystemsModel::RolesCount))
            return QVariant();

        const auto row = index.row();
        if (!qBetween(0, row, rowCount()))
            return QVariant();

        switch(role)
        {
            case Qt::DisplayRole:
            case QnSystemsModel::SearchRoleId:
                // In QnSystemsModel SearchRoleId is returned similart to this:
                //   return QString("%0  %1 ").arg(m_systems[row].name, m_systems[row].address);
                // but for test purposes we return just an address.
                return m_systems[row].address;
            case QnSystemsModel::SystemNameRoleId:
                return m_systems[row].name;
            case QnSystemsModel::SystemIdRoleId:
                return m_systems[row].systemId;
            case QnSystemsModel::LocalIdRoleId:
                return m_systems[row].localId;
            case QnSystemsModel::IsFactorySystemRoleId:
                return m_systems[row].isNew;
            case QnSystemsModel::IsCloudSystemRoleId:
                return m_systems[row].isCloud;
            case QnSystemsModel::IsOnlineRoleId:
                return m_systems[row].isOnline;
            default:
                return QVariant();
        }
    }

    void addSystem(const SystemInfo& info)
    {
        beginInsertRows(QModelIndex(), m_systems.size(), m_systems.size());
        m_systems.append(info);
        endInsertRows();
    }

    int getRowBySystemId(const QString& systemId)
    {
        for (int i = 0; i < m_systems.size(); i++)
        {
            if (m_systems[i].systemId == systemId)
                return i;
        }
        return -1;
    }

    void updateSystem(const SystemInfo& info)
    {
        updateSystemAt(getRowBySystemId(info.systemId), info);
    }

    void updateSystemAt(int row, const SystemInfo& info)
    {
        SystemInfo prevInfo = m_systems[row];

        QVector<int> roles;
        if (prevInfo.name != info.name) roles << QnSystemsModel::SystemNameRoleId;
        if (prevInfo.systemId != info.systemId) roles << QnSystemsModel::SystemIdRoleId;
        if (prevInfo.localId != info.localId) roles << QnSystemsModel::LocalIdRoleId;
        if (prevInfo.address != info.address) roles << QnSystemsModel::SearchRoleId;
        if (prevInfo.isNew != info.isNew) roles << QnSystemsModel::IsFactorySystemRoleId;
        if (prevInfo.isCloud != info.isCloud) roles << QnSystemsModel::IsCloudSystemRoleId;
        if (prevInfo.isOnline != info.isOnline) roles << QnSystemsModel::IsOnlineRoleId;

        if (roles.isEmpty())
            return;

        m_systems[row] = info;

        auto index = this->index(row, 0);
        emit dataChanged(index, index, roles);
    }

private:
    QVector<SystemInfo> m_systems;
};

// Stores current weight info.
class MockSystemsWeightsManager
{
public:
    QnWeightsDataHash weights() const { return m_weights; }

    qreal unknownSystemsWeight() const { return 0.0; }

    void setWeight(const nx::Uuid& localSystemId, qreal weight, qint64 lastConnectedMs = 0)
    {
        const bool isRealConnection = true;
        const nx::vms::client::core::WeightData data{
            localSystemId, weight, lastConnectedMs, isRealConnection};
        m_weights.insert(localSystemId.toSimpleString(), data);
    }

private:
    QnWeightsDataHash m_weights;
};

// This is the actual class we are testing. It inherits QnSystemSortFilterListModel with additional
// ability to forget a single system. QnOrderedSystemsModel (which is used for welcome screen tiles)
// also inherits QnSystemSortFilterListModel but adds connections to QnSystemsWeightsManager and
// QnForgottenSystemsManager.
class SystemSortFilterListModel: public QnSystemSortFilterListModel
{
    using base_type = QnSystemSortFilterListModel;

public:

    SystemSortFilterListModel(QObject* parent = nullptr) : base_type(parent) {}

    virtual bool isForgotten(const QString& id) const override
    {
        return id == m_forgottenId;
    }

    void setForgottenId(const QString& id)
    {
        m_forgottenId = id;
        forceUpdate();
    }

    // Helper function for gathering model data as vector.
    QVector<SystemInfo> systems()
    {
        QVector<SystemInfo> result;

        for (int i = 0; i < rowCount(); i++)
        {
            const auto mi = index(i, 0);

            QString name = mi.data(QnSystemsModel::SystemNameRoleId).toString();
            QString systemId = mi.data(QnSystemsModel::SystemIdRoleId).toString();
            QString localId = mi.data(QnSystemsModel::LocalIdRoleId).toString();
            QString address = mi.data(QnSystemsModel::SearchRoleId).toString();
            bool isNew = mi.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
            bool isCloud = mi.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
            bool isOnline = mi.data(QnSystemsModel::IsOnlineRoleId).toBool();

            result.append({name, systemId, localId, address, isNew, isCloud, isOnline});
        }

        return result;
    }
private:
    QString m_forgottenId;
};

// All necessary models.
class OrderedSystemsModelTest: public testing::Test
{
protected:
    void SetUp() override
    {
        weightsManager = new MockSystemsWeightsManager();
        systemsModel = new MockSystemsModel();
        model = new SystemSortFilterListModel();

        model->setSourceModel(systemsModel);
    }

    void TearDown() override
    {
        delete model;
        delete systemsModel;
        delete weightsManager;
    }

    MockSystemsWeightsManager* weightsManager;
    MockSystemsModel* systemsModel;
    SystemSortFilterListModel* model;
};

// All necessary models and initial list of systems.
class OrderedSystemsModelDataTest: public OrderedSystemsModelTest
{
    using base_type = OrderedSystemsModelTest;

protected:
    void SetUp() override
    {
        base_type::SetUp();

        uuid1 = nx::Uuid::createUuid();
        uuid2 = nx::Uuid::createUuid();

        //                    name     systemId                 localId  address      isNew  isCloud isConnectable
        system0 = SystemInfo {"Name",  "0005",                  "00011", "127.0.0.1", false, false,  true};
        system1 = SystemInfo {"Name1", uuid1.toSimpleString(),  "00012", "1.2.3.4",   false, true,   true};
        system2 = SystemInfo {"Name2", uuid2.toSimpleString(),  "00013", "10.0.0.2",  false, false,  true};
        system3 = SystemInfo {"Name",  "0002",                  "00014", "127.0.0.1", false, false,  true};
        system4 = SystemInfo {"Name4", "0001",                  "00015", "10.0.0.1",  true,  false,  true};
        system5 = SystemInfo {"Name5", "0000",                  "00010", "localhost", true,  false,  true};

        systemsModel->addSystem(system0);
        systemsModel->addSystem(system1);
        systemsModel->addSystem(system2);
        systemsModel->addSystem(system3);
        systemsModel->addSystem(system4);
        systemsModel->addSystem(system5);
    }

    nx::Uuid uuid1;
    nx::Uuid uuid2;
    SystemInfo system0;
    SystemInfo system1;
    SystemInfo system2;
    SystemInfo system3;
    SystemInfo system4;
    SystemInfo system5;
};

} // namespace

TEST_F(OrderedSystemsModelTest, emptyModelHasNoRows)
{
    ASSERT_EQ(0, model->rowCount());
    ASSERT_EQ(0, model->acceptedRowCount());
}

TEST_F(OrderedSystemsModelTest, factoryOnLocalhostGoesFirst)
{
    //                  name     systemId                               localId  address      isNew  isCloud isConnectable
    SystemInfo system0 {"Name1", nx::Uuid::createUuid().toSimpleString(), "00011", "127.0.0.1", false, false,  true};
    SystemInfo system1 {"Name2", nx::Uuid::createUuid().toSimpleString(), "00022", "127.0.0.1", true,  false,  true};

    systemsModel->addSystem(system0);
    systemsModel->addSystem(system1);

    ASSERT_EQ(2, model->rowCount());
    ASSERT_EQ(2, model->acceptedRowCount());

    QVector<SystemInfo> sorted {
        system1,
        system0
    };

    ASSERT_EQ(sorted, model->systems());
}

TEST_F(OrderedSystemsModelDataTest, weightSortOrder)
{
    // Verify that non-localhost non-factory systems are sorted by weight
    weightsManager->setWeight(uuid1, 1.0);
    weightsManager->setWeight(uuid2, 2.0);
    model->setWeights(weightsManager->weights(), weightsManager->unknownSystemsWeight());

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system2, // regular
            system1  // cloud
        }),
        model->systems());

    weightsManager->setWeight(uuid1, 2.0);
    weightsManager->setWeight(uuid2, 1.0);
    model->setWeights(weightsManager->weights(), weightsManager->unknownSystemsWeight());

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());
}

TEST_F(OrderedSystemsModelDataTest, systemIdSortOrder)
{
    // Verify that systems with the same name are sorted by systemId

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());

    system3.systemId = "0006";
    systemsModel->updateSystemAt(3, system3);

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system0, // localhost, id 0005
            system3, // localhost, id 0006
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());
}

TEST_F(OrderedSystemsModelDataTest, connectableForgottenArePresent)
{
    // Verify that connectable system can not be forgotten
    model->setForgottenId(system1.systemId);
    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());

    system1.isOnline = false;
    systemsModel->updateSystem(system1);

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
                     // cloud
            system2  // regular
        }),
        model->systems());

    ASSERT_EQ(5, model->acceptedRowCount());
}

TEST_F(OrderedSystemsModelDataTest, wildcardAcceptedRowCount)
{
    model->setFilterRole(QnSystemsModel::SystemNameRoleId);

    model->setFilterWildcard("NoSystemMatchesThis");
    ASSERT_EQ(0, model->rowCount());
    ASSERT_EQ(6, model->acceptedRowCount());

    model->setForgottenId(system1.systemId);
    system1.isOnline = false;
    systemsModel->updateSystem(system1);

    ASSERT_EQ(0, model->rowCount());
    ASSERT_EQ(5, model->acceptedRowCount());

    model->setForgottenId("");
    model->setFilterWildcard("");
    ASSERT_EQ(6, model->rowCount());
    ASSERT_EQ(6, model->acceptedRowCount());

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());
}

TEST_F(OrderedSystemsModelDataTest, wildcardWithSmallWeightAcceptedRowCount)
{
    // non-connectable, non-cloud systems with small weight shoud be filtered out
    system2.isOnline = false;
    systemsModel->updateSystem(system2);
    weightsManager->setWeight(uuid2, 0.0);
    model->setWeights(weightsManager->weights(), weightsManager->unknownSystemsWeight());
    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1  // cloud
                     // regular
        }),
        model->systems());
    ASSERT_EQ(5, model->acceptedRowCount());

    model->setFilterRole(QnSystemsModel::SystemNameRoleId);

    model->setFilterWildcard("Name5");
    ASSERT_EQ(
        QVector<SystemInfo>({
            system5 // localhost, factory
        }),
        model->systems());
    ASSERT_EQ(5, model->acceptedRowCount());

    weightsManager->setWeight(uuid2, 1.0);
    model->setWeights(weightsManager->weights(), weightsManager->unknownSystemsWeight());
    ASSERT_EQ(
        QVector<SystemInfo>({
            system5 // localhost, factory
        }),
        model->systems());
    ASSERT_EQ(6, model->acceptedRowCount());
}

TEST_F(OrderedSystemsModelDataTest, addSystem)
{
    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system4, // regular, factory
            system1, // cloud
            system2  // regular
        }),
        model->systems());

    //                  name     systemId                               localId  address      isNew  isCloud isConnectable
    SystemInfo system6 {"N",     nx::Uuid::createUuid().toSimpleString(), "00001", "127.0.0.1", false, false,  true};
    SystemInfo system7 {"N",     nx::Uuid::createUuid().toSimpleString(), "00002", "4.3.2.1",   false, false,  true};
    SystemInfo system8 {"Name4", "0000",                                "00003", "4.3.2.2",   true,  false,  true};

    systemsModel->addSystem(system6);
    systemsModel->addSystem(system7);
    systemsModel->addSystem(system8);

    ASSERT_EQ(
        QVector<SystemInfo>({
            system5, // localhost, factory
            system6,
            system3, // localhost, id 0002
            system0, // localhost, id 0005
            system8,
            system4, // regular, factory
            system7,
            system1, // cloud
            system2  // regular
        }),
        model->systems());
}


} // namespace test
} // namespace nx::vms::client::core
