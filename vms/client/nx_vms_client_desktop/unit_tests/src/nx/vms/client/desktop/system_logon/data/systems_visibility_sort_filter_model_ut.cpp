// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QSet>

#include <nx/utils/math/math.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_logon/data/connect_tiles_proxy_model.h>
#include <nx/vms/client/desktop/system_logon/data/systems_visibility_sort_filter_model.h>
#include <ui/models/systems_model.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;
using TileScopeFilter = nx::vms::client::core::welcome_screen::TileScopeFilter;

// Dummy structure that represents a single system.
struct SystemInfo
{
    QString name;
    QString systemId;
    QString searchAddressPart;
    bool isNew = false;
    bool isCloud = false;
    bool isOnline = true;
    bool isCompatible = true;
    TileVisibilityScope scope = TileVisibilityScope::DefaultTileVisibilityScope;

    bool operator==(const SystemInfo& other) const
    {
        return other.name == name &&
            other.systemId == systemId &&
            other.searchAddressPart == searchAddressPart &&
            other.isNew == isNew &&
            other.isCloud == isCloud &&
            other.isOnline == isOnline &&
            other.isCompatible == isCompatible &&
            other.scope == scope;
    }
};

// A mock class for QnSystemsModel that implements Qt list model.
class MockSystemsModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

public:
    MockSystemsModel(QObject* parent = nullptr): base_type(parent) {}

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
                 return QString("%0  %1").arg(m_systems[row].name, m_systems[row].searchAddressPart);
            case QnSystemsModel::SystemNameRoleId:
                return m_systems[row].name;
            case QnSystemsModel::SystemIdRoleId:
                return m_systems[row].systemId;
            case QnSystemsModel::IsFactorySystemRoleId:
                return m_systems[row].isNew;
            case QnSystemsModel::IsCloudSystemRoleId:
                return m_systems[row].isCloud;
            case QnSystemsModel::IsOnlineRoleId:
                return m_systems[row].isOnline;
            case QnSystemsModel::IsCompatibleToDesktopClient:
                return m_systems[row].isCompatible;
            case QnSystemsModel::VisibilityScopeRoleId:
                return QVariant::fromValue(m_systems[row].scope);
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
        for (int i = 0; i < m_systems.size(); ++i)
        {
            if (m_systems[i].systemId == systemId)
                return i;
        }
        return -1;
    }

    bool removeSystem(const QString& systemId)
    {
        const int i = getRowBySystemId(systemId);
        if (i == -1)
            return false;

        beginRemoveRows(QModelIndex(), i, i);
        m_systems.remove(i);
        endRemoveRows();

        return true;
    }

    void updateSystems(const QVector<SystemInfo>& infos)
    {
        for (const auto& info: infos)
            updateSystemAt(getRowBySystemId(info.systemId), info);
    }

    void updateSystem(const SystemInfo& info)
    {
        updateSystemAt(getRowBySystemId(info.systemId), info);
    }

    void updateSystemAt(int row, const SystemInfo& info)
    {
        ASSERT_TRUE(row >= 0 || row < m_systems.size());

        const SystemInfo prevInfo = m_systems[row];

        QVector<int> roles;
        if (prevInfo.name != info.name)
            roles << QnSystemsModel::SystemNameRoleId;

        if (prevInfo.systemId != info.systemId)
            roles << QnSystemsModel::SystemIdRoleId;

        if (prevInfo.searchAddressPart != info.searchAddressPart)
            roles << QnSystemsModel::SearchRoleId;

        if (prevInfo.isNew != info.isNew)
            roles << QnSystemsModel::IsFactorySystemRoleId;

        if (prevInfo.isCloud != info.isCloud)
            roles << QnSystemsModel::IsCloudSystemRoleId;

        if (prevInfo.isOnline != info.isOnline)
            roles << QnSystemsModel::IsOnlineRoleId;

        if (prevInfo.isCompatible != info.isCompatible)
            roles << QnSystemsModel::IsCompatibleToDesktopClient;

        if (prevInfo.scope != info.scope)
            roles << QnSystemsModel::VisibilityScopeRoleId;

        if (roles.isEmpty())
            return;

        m_systems[row] = info;

        const auto index = this->index(row, 0);
        emit dataChanged(index, index, roles);
    }

private:
    QVector<SystemInfo> m_systems;
};

// All necessary models.
class SystemsVisibilitySortFilterModelTest: public testing::Test
{
public:
    SystemsVisibilitySortFilterModelTest()
    {
        m_visibilityModel.reset(new nx::vms::client::desktop::SystemsVisibilitySortFilterModel());

        m_visibilityModel->setVisibilityScopeFilterCallbacks(
            [this]()->TileScopeFilter
            {
                return filter;
            },
            [this](TileScopeFilter filter)
            {
                this->filter = filter;
            }
        );

        // No forgotten systems, by default.
        m_visibilityModel->setForgottenCheckCallback(
            [](const QString& /*systemId*/)->bool
            {
                return false;
            }
        );

        m_systemsModel.reset(new MockSystemsModel());
        m_visibilityModel->setSourceModel(m_systemsModel.get());

        m_connectTilesModel.reset(new nx::vms::client::desktop::ConnectTilesProxyModel());

        m_connectTilesModel->setCloudVisibilityScopeCallbacks(
            [this]()->TileVisibilityScope
            {
                return cloudTileVisibilityScope;
            },
            [this](TileVisibilityScope value)
            {
                cloudTileVisibilityScope = value;
            }
        );
        m_connectTilesModel->setSourceModel(m_visibilityModel.get());
    }

    void whenVisibilityFilterIsDefault()
    {
        m_connectTilesModel->setVisibilityFilter(TileScopeFilter::AllSystemsTileScopeFilter);
    }
    void whenVisibilityFilterIsFavorite()
    {
        m_connectTilesModel->setVisibilityFilter(TileScopeFilter::FavoritesTileScopeFilter);
    }
    void whenVisibilityFilterIsHidden()
    {
        m_connectTilesModel->setVisibilityFilter(TileScopeFilter::HiddenTileScopeFilter);
    }

    void thenTotalNumberOfSystems(int number)
    {
        ASSERT_EQ(number, m_visibilityModel->rowCount());
    }
    void thenVisibleNumberOfTiles(int number)
    {
        ASSERT_EQ(number, m_connectTilesModel->rowCount());
    }
    void thenTotalNumberOfTiles(int number)
    {
        ASSERT_EQ(number, m_connectTilesModel->totalSystemsCount());
    }

    void whenSomeSystemsAreForgotten(const QSet<QString>& forgottenSystems)
    {
        m_visibilityModel->setForgottenCheckCallback(
            [forgottenSystems](const QString& systemId) -> bool
            {
                return forgottenSystems.contains(systemId);
            }
        );

        for (const auto& systemId: forgottenSystems)
            m_visibilityModel->forgottenSystemAdded(systemId);
    };

protected:
    std::unique_ptr<MockSystemsModel> m_systemsModel;
    std::unique_ptr<nx::vms::client::desktop::SystemsVisibilitySortFilterModel> m_visibilityModel;
    std::unique_ptr<nx::vms::client::desktop::ConnectTilesProxyModel> m_connectTilesModel;

    TileScopeFilter filter = TileScopeFilter::AllSystemsTileScopeFilter;
    TileVisibilityScope cloudTileVisibilityScope = TileVisibilityScope::DefaultTileVisibilityScope;
};

QVector<SystemInfo> systemsList(const QAbstractProxyModel* model)
{
    QVector<SystemInfo> result;

    for (int i = 0; i < model->rowCount(); ++i)
    {
        const auto index = model->index(i, 0);

        QString name = index.data(QnSystemsModel::SystemNameRoleId).toString();
        QString systemId = index.data(QnSystemsModel::SystemIdRoleId).toString();
        QString searchString = index.data(QnSystemsModel::SearchRoleId).toString();
        QString address = searchString.mid(name.size() + 2);
        bool isNew = index.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
        bool isCloud = index.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
        bool isOnline = index.data(QnSystemsModel::IsOnlineRoleId).toBool();
        bool isCompatible = index.data(QnSystemsModel::IsCompatibleToDesktopClient).toBool();
        TileVisibilityScope visibilityScope = static_cast<TileVisibilityScope>(
            index.data(QnSystemsModel::VisibilityScopeRoleId).toInt());

        result.append(
            {name, systemId, address, isNew, isCloud, isOnline, isCompatible, visibilityScope});
    }

    return result;
}

enum SystemFlags
{
    noFlags = 0x0,
    isOffline = 0x01,
    isNew = 0x02,
    isCloud = 0x04,
    isIncompatible = 0x08,
};

SystemInfo createSystem(
    const QString& systemName,
    const QString& searchAddrPart,
    TileVisibilityScope visibilityScope,
    int flags = SystemFlags::noFlags)
{
    return SystemInfo{
        systemName,
        nx::Uuid::createUuid().toSimpleString(),
        searchAddrPart,
        bool(flags & SystemFlags::isNew),
        bool(flags & SystemFlags::isCloud),
        !(flags & SystemFlags::isOffline),
        !(flags & SystemFlags::isIncompatible),
        visibilityScope};
};

SystemInfo createSystem(
    const QString& systemName,
    const QString& searchAddrPart,
    int flags = SystemFlags::noFlags)
{
    return createSystem(
        systemName,
        searchAddrPart,
        TileVisibilityScope::DefaultTileVisibilityScope,
        flags);
};

} // namespace

TEST_F(SystemsVisibilitySortFilterModelTest, emptyModelHasNoRows)
{
    auto clearVisibilityModelDoesntHaveSystems = [&]()
        {
            ASSERT_EQ(0, m_visibilityModel->rowCount());
        };

    auto visibilityFilterIsAllSystemsByDefault = [&]()
        {
            ASSERT_EQ(TileScopeFilter::AllSystemsTileScopeFilter, m_visibilityModel->visibilityFilter());
        };

    // ---------------------------------------------

    clearVisibilityModelDoesntHaveSystems();

    visibilityFilterIsAllSystemsByDefault();

    thenVisibleNumberOfTiles(2);
    thenTotalNumberOfTiles(2);

    whenVisibilityFilterIsHidden();
    thenVisibleNumberOfTiles(0);
    thenTotalNumberOfTiles(2);

    whenVisibilityFilterIsFavorite();
    thenVisibleNumberOfTiles(0);
    thenTotalNumberOfTiles(2);

    whenVisibilityFilterIsDefault();
    thenVisibleNumberOfTiles(2);
    thenTotalNumberOfTiles(2);
}

TEST_F(SystemsVisibilitySortFilterModelTest, offlineIncompatibleSortCheck)
{
    SystemInfo factoryASystem = createSystem("FactoryA",   "192.168.2.1", SystemFlags::isNew);
    SystemInfo factoryBSystem = createSystem("FactoryB",   "192.168.2.1", SystemFlags::isNew);
    SystemInfo cloudASystem   = createSystem("CloudA",     "192.168.2.4", SystemFlags::isCloud);
    SystemInfo cloudBSystem   = createSystem("CloudB",     "192.168.2.4", SystemFlags::isCloud);
    SystemInfo localASystem   = createSystem("LocalhostA", "localhost");
    SystemInfo localBSystem   = createSystem("LocalhostB", "localhost");
    SystemInfo aSystem        = createSystem("A",          "192.168.2.2");
    SystemInfo bSystem        = createSystem("B",          "192.168.2.3");
    SystemInfo cSystem        = createSystem("C",          "192.168.2.3");

    m_systemsModel->addSystem(localBSystem);
    m_systemsModel->addSystem(bSystem);
    m_systemsModel->addSystem(factoryBSystem);
    m_systemsModel->addSystem(cloudASystem);
    m_systemsModel->addSystem(cloudBSystem);
    m_systemsModel->addSystem(factoryASystem);
    m_systemsModel->addSystem(aSystem);
    m_systemsModel->addSystem(cSystem);
    m_systemsModel->addSystem(localASystem);

    const QVector<SystemInfo> initialOrder {
        factoryASystem,
        factoryBSystem,
        cloudASystem,
        cloudBSystem,
        localASystem,
        localBSystem,
        aSystem,
        bSystem,
        cSystem,
    };

    auto setSomeSystemsOfflineAndIncompatible = [&]()
        {
            factoryASystem.isCompatible = false;
            cloudASystem.isOnline = false;
            localASystem.isCompatible = false;
            aSystem.isOnline = false;
            bSystem.isCompatible = false;
            m_systemsModel->updateSystems({factoryASystem, cloudASystem, localASystem, aSystem, bSystem});

            return QVector<SystemInfo>{
                factoryBSystem,
                factoryASystem,
                cloudBSystem,
                cloudASystem,
                localBSystem,
                cSystem,
                localASystem,
                aSystem,
                bSystem,
            };
        };

    // ---------------------------------------------

    ASSERT_EQ(initialOrder, systemsList(m_visibilityModel.get()));
    auto offlineIncompatibleOrder = setSomeSystemsOfflineAndIncompatible();
    ASSERT_EQ(offlineIncompatibleOrder, systemsList(m_visibilityModel.get()));
}

TEST_F(SystemsVisibilitySortFilterModelTest, visibilitySortCheck)
{
    SystemInfo lFactorySystem = createSystem("LFactory",   "localhost",   TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isNew);
    SystemInfo factorySystem  = createSystem("Factory",    "192.168.2.1", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isNew);
    SystemInfo fCloudSystem   = createSystem("FCloud",     "192.168.2.4", TileVisibilityScope::FavoriteTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo dCloudSystem   = createSystem("DCloud",     "192.168.2.5", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo hCloudSystem   = createSystem("HCloud",     "192.168.2.6", TileVisibilityScope::HiddenTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo fLocalSystem   = createSystem("FLocalhost", "localhost",   TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dLocalSystem   = createSystem("DLocalhost", "localhost",   TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hLocalSystem   = createSystem("HLocalhost", "localhost",   TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fASystem       = createSystem("FASystem",   "192.168.2.2", TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dASystem       = createSystem("DASystem",   "192.168.2.2", TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hASystem       = createSystem("HASystem",   "192.168.2.2", TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fBSystem       = createSystem("FBSystem",   "192.168.2.3", TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dBSystem       = createSystem("DBSystem",   "192.168.2.3", TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hBSystem       = createSystem("HBSystem",   "192.168.2.3", TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fSystemOff     = createSystem("FSystemOff", "192.168.2.7", TileVisibilityScope::FavoriteTileVisibilityScope, SystemFlags::isOffline);
    SystemInfo dSystemOff     = createSystem("DSystemOff", "192.168.2.7", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isOffline);
    SystemInfo hSystemOff     = createSystem("HSystemOff", "192.168.2.7", TileVisibilityScope::HiddenTileVisibilityScope, SystemFlags::isOffline);

    m_systemsModel->addSystem(hCloudSystem);
    m_systemsModel->addSystem(fBSystem);
    m_systemsModel->addSystem(hBSystem);
    m_systemsModel->addSystem(hSystemOff);
    m_systemsModel->addSystem(hLocalSystem);
    m_systemsModel->addSystem(factorySystem);
    m_systemsModel->addSystem(dASystem);
    m_systemsModel->addSystem(dBSystem);
    m_systemsModel->addSystem(dSystemOff);
    m_systemsModel->addSystem(fLocalSystem);
    m_systemsModel->addSystem(dCloudSystem);
    m_systemsModel->addSystem(lFactorySystem);
    m_systemsModel->addSystem(fASystem);
    m_systemsModel->addSystem(dLocalSystem);
    m_systemsModel->addSystem(fCloudSystem);
    m_systemsModel->addSystem(hASystem);
    m_systemsModel->addSystem(fSystemOff);

    const QVector<SystemInfo> defaultVisibilityOrder {
        lFactorySystem,
        factorySystem,

        fCloudSystem,
        fLocalSystem,
        fASystem,
        fBSystem,
        fSystemOff,

        dCloudSystem,
        dLocalSystem,
        dASystem,
        dBSystem,
        dSystemOff,
    };

    auto firstTileIsCloudButton = [&]()
        {
            const auto cloudTileIndex = m_connectTilesModel->index(0, 0);
            ASSERT_EQ(ConnectTilesProxyModel::CloudButtonTileType,
                m_connectTilesModel->data(cloudTileIndex, ConnectTilesProxyModel::TileTypeRoleId).toInt());
        };
    auto lastTileIsConnectButton = [&]()
        {
            const auto connectTileIndex =
                m_connectTilesModel->index(m_connectTilesModel->rowCount() - 1, 0);
            ASSERT_EQ(ConnectTilesProxyModel::ConnectButtonTileType,
                m_connectTilesModel->data(
                    connectTileIndex,
                    ConnectTilesProxyModel::TileTypeRoleId).toInt());
        };
    auto otherTilesIsOrderedSystems = [&]()
        {
            auto tileSystems = systemsList(m_connectTilesModel.get());
            tileSystems.pop_front();
            tileSystems.pop_back();
            ASSERT_EQ(defaultVisibilityOrder, tileSystems);
        };

    // ---------------------------------------------

    thenTotalNumberOfSystems(12);
    thenVisibleNumberOfTiles(14);
    thenTotalNumberOfTiles(19);

    whenVisibilityFilterIsFavorite();
    thenTotalNumberOfSystems(5);
    thenVisibleNumberOfTiles(5);
    thenTotalNumberOfTiles(19);

    whenVisibilityFilterIsHidden();
    thenTotalNumberOfSystems(5);
    thenVisibleNumberOfTiles(5);
    thenTotalNumberOfTiles(19);

    whenVisibilityFilterIsDefault();
    thenTotalNumberOfSystems(12);
    thenVisibleNumberOfTiles(14);
    thenTotalNumberOfTiles(19);
    firstTileIsCloudButton();
    lastTileIsConnectButton();
    otherTilesIsOrderedSystems();
}

TEST_F(SystemsVisibilitySortFilterModelTest, filtrationCheck)
{
    SystemInfo lFactorySystem = createSystem("LFactory",   "localhost",   TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isNew);
    SystemInfo factorySystem  = createSystem("Factory",    "192.168.2.1", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isNew);
    SystemInfo fCloudSystem   = createSystem("FCloud",     "192.168.2.4", TileVisibilityScope::FavoriteTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo dCloudSystem   = createSystem("DCloud",     "192.168.2.5", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo hCloudSystem   = createSystem("HCloud",     "192.168.2.6", TileVisibilityScope::HiddenTileVisibilityScope, SystemFlags::isCloud);
    SystemInfo fLocalSystem   = createSystem("FLocalhost", "localhost",   TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dLocalSystem   = createSystem("DLocalhost", "localhost",   TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hLocalSystem   = createSystem("HLocalhost", "localhost",   TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fASystem       = createSystem("FASystem",   "192.168.2.2", TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dASystem       = createSystem("DASystem",   "192.168.2.2", TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hASystem       = createSystem("HASystem",   "192.168.2.2", TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fBSystem       = createSystem("FBSystem",   "192.168.2.3", TileVisibilityScope::FavoriteTileVisibilityScope);
    SystemInfo dBSystem       = createSystem("DBSystem",   "192.168.2.3", TileVisibilityScope::DefaultTileVisibilityScope);
    SystemInfo hBSystem       = createSystem("HBSystem",   "192.168.2.3", TileVisibilityScope::HiddenTileVisibilityScope);
    SystemInfo fSystemOff     = createSystem("FSystemOff", "192.168.2.7", TileVisibilityScope::FavoriteTileVisibilityScope, SystemFlags::isOffline);
    SystemInfo dSystemOff     = createSystem("DSystemOff", "192.168.2.7", TileVisibilityScope::DefaultTileVisibilityScope, SystemFlags::isOffline);
    SystemInfo hSystemOff     = createSystem("HSystemOff", "192.168.2.7", TileVisibilityScope::HiddenTileVisibilityScope, SystemFlags::isOffline);

    m_systemsModel->addSystem(hCloudSystem);
    m_systemsModel->addSystem(fBSystem);
    m_systemsModel->addSystem(hBSystem);
    m_systemsModel->addSystem(hSystemOff);
    m_systemsModel->addSystem(hLocalSystem);
    m_systemsModel->addSystem(factorySystem);
    m_systemsModel->addSystem(dASystem);
    m_systemsModel->addSystem(dBSystem);
    m_systemsModel->addSystem(dSystemOff);
    m_systemsModel->addSystem(fLocalSystem);
    m_systemsModel->addSystem(dCloudSystem);
    m_systemsModel->addSystem(lFactorySystem);
    m_systemsModel->addSystem(fASystem);
    m_systemsModel->addSystem(dLocalSystem);
    m_systemsModel->addSystem(fCloudSystem);
    m_systemsModel->addSystem(hASystem);
    m_systemsModel->addSystem(fSystemOff);

    QVector<SystemInfo> filteredOrder {
        lFactorySystem,
        fLocalSystem,
        dLocalSystem,
        hLocalSystem,
    };

    auto whenFilteredByWildcard = [&](const QString& wildcard)
        {
            m_connectTilesModel->setFilterWildcard(wildcard);
        };

    // ---------------------------------------------

    whenFilteredByWildcard("localho");
    ASSERT_EQ(filteredOrder, systemsList(m_connectTilesModel.get()));
}

TEST_F(SystemsVisibilitySortFilterModelTest, forgottenCheck)
{
    SystemInfo aCloudSystem = createSystem("ACloud",     "192.168.1.1", SystemFlags::isCloud);
    SystemInfo bCloudSystem = createSystem("BCloud",     "192.168.1.2", SystemFlags::isOffline | SystemFlags::isCloud);
    SystemInfo cCloudSystem = createSystem("CCloud",     "192.168.1.3", SystemFlags::isOffline | SystemFlags::isCloud);
    SystemInfo aLocalSystem = createSystem("ALocalhost", "localhost");
    SystemInfo bLocalSystem = createSystem("BLocalhost", "localhost", SystemFlags::isOffline);
    SystemInfo cLocalSystem = createSystem("CLocalhost", "localhost", SystemFlags::isOffline);
    SystemInfo aSystem      = createSystem("ASystem",    "192.168.2.1");
    SystemInfo bSystem      = createSystem("BSystem",    "192.168.2.2", SystemFlags::isOffline);
    SystemInfo cSystem      = createSystem("CSystem",    "192.168.2.3", SystemFlags::isOffline);

    m_systemsModel->addSystem(aCloudSystem);
    m_systemsModel->addSystem(bCloudSystem);
    m_systemsModel->addSystem(cCloudSystem);
    m_systemsModel->addSystem(aLocalSystem);
    m_systemsModel->addSystem(bLocalSystem);
    m_systemsModel->addSystem(cLocalSystem);
    m_systemsModel->addSystem(aSystem);
    m_systemsModel->addSystem(bSystem);
    m_systemsModel->addSystem(cSystem);

    const auto forgottenSystems = QSet<QString>{
        aCloudSystem.systemId,
        bCloudSystem.systemId,
        aLocalSystem.systemId,
        bLocalSystem.systemId,
        aSystem.systemId,
        bSystem.systemId,
    };

    const QVector<SystemInfo> visibleSystemsOrder{
        cCloudSystem,
        cLocalSystem,
        cSystem,
    };

    // ---------------------------------------------

    whenSomeSystemsAreForgotten(forgottenSystems);
    ASSERT_EQ(visibleSystemsOrder, systemsList(m_visibilityModel.get()));
}

TEST_F(SystemsVisibilitySortFilterModelTest, totalSystemsForgottenCheck)
{
    SystemInfo regularSystem    = createSystem("ASystem", "192.168.2.1");
    SystemInfo forgottenSystem1 = createSystem("BSystem", "192.168.2.2");
    SystemInfo forgottenSystem2 = createSystem("CSystem", "192.168.2.3");

    m_systemsModel->addSystem(regularSystem);
    m_systemsModel->addSystem(forgottenSystem1);
    m_systemsModel->addSystem(forgottenSystem2);

    const auto forgottenSystems = QSet<QString>{
        forgottenSystem1.systemId,
        forgottenSystem2.systemId,
    };

    auto thenVisibleNumberOfSystems = [&](int number)
    {
        ASSERT_EQ(number, m_visibilityModel->rowCount());
        ASSERT_EQ(number, m_visibilityModel->totalSystemsCount());
    };

    auto whenForgottenSystemRemoved = [&](const SystemInfo& system)
    {
        ASSERT_TRUE(m_systemsModel->removeSystem(system.systemId));
    };

    bool totalSystemsCountChangedSignalRecieved = false;

    const auto ptr = m_visibilityModel.get();
    auto connection =
        QObject::connect(ptr, &SystemsVisibilitySortFilterModel::totalSystemsCountChanged, ptr,
            [&totalSystemsCountChangedSignalRecieved]
            {
                totalSystemsCountChangedSignalRecieved = true;
            }, Qt::DirectConnection);

    auto thenTotalSystemNumberIsChanged = [&]()
    {
        ASSERT_TRUE(totalSystemsCountChangedSignalRecieved);
    };

    // ---------------------------------------------

    whenSomeSystemsAreForgotten(forgottenSystems);
    thenTotalSystemNumberIsChanged();
    thenVisibleNumberOfSystems(1);
    whenForgottenSystemRemoved(forgottenSystem2);
    thenVisibleNumberOfSystems(1);
}

} // namespace test
} // namespace nx::vms::client::desktop
