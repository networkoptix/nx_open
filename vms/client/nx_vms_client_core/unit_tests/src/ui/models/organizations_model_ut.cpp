// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>

#include <nx/vms/client/core/common/models/linearization_list_model.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_description.h>
#include <nx/vms/client/core/system_finder/private/local_system_description.h>
#include <ui/models/organizations_model.h>
#include <ui/models/systems_model.h>

#include "test_systems_controller.h"

// clang-format off

namespace nx::vms::client::core {
namespace test {

namespace {

using SystemStatus = nx::cloud::db::api::SystemStatus;

SystemInOrganization makeSystem(const std::string& name, SystemStatus status)
{
    return {
        .systemId = nx::Uuid::createUuid(),
        .name = name,
        .state = "active",
        .system_state = status,
    };
}

// Helper function to recursively dump the tree structure of a model into a QStringList.
// Each line in the list represents a node in the tree, with indentation based on the level of
// the node in the hierarchy. Section information is appended after ": " and newlines are replaced
// with "<br>" for better readability in the output.
void dumpTree(
    QStringList& result,
    const QAbstractItemModel& model,
    const QModelIndex& parent = {},
    int level = 0)
{
    for (int row = 0; row < model.rowCount(parent); ++row)
    {
        const QModelIndex index = model.index(row, 0, parent);
        QString s = QString(level * 2, ' ');
        s += QString("%1: %2")
            .arg(index.data(Qt::DisplayRole).toString())
            .arg(index.data(OrganizationsModel::SectionRole).toString().replace("\n", "<br>"));
        result << s;
        dumpTree(result, model, index, level + 1);
    }
}

QStringList dumpTree(const QAbstractItemModel& model)
{
    QStringList result;
    dumpTree(result, model, {}, 0);
    return result;
}

QStringList dumpNames(const QAbstractItemModel& model, const QModelIndex& parent = {})
{
    QStringList result;
    for (int row = 0; row < model.rowCount(parent); ++row)
        result << model.index(row, 0, parent).data(Qt::DisplayRole).toString();
    return result;
}

// Names of all items of the tree in depth-first order.
QStringList dumpNamesRecursively(const QAbstractItemModel& model, const QModelIndex& parent = {})
{
    QStringList result;
    for (int row = 0; row < model.rowCount(parent); ++row)
    {
        const auto index = model.index(row, 0, parent);
        result << index.data(Qt::DisplayRole).toString();
        result << dumpNamesRecursively(model, index);
    }
    return result;
}

// Checks that the index refers to an item which is actually in the model: it's the same index
// the model returns for its position.
bool isModelIndex(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    const auto parent = index.parent();
    if (parent.isValid() && !isModelIndex(parent))
        return false;

    return index.model()->index(index.row(), index.column(), parent) == index;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// OrganizationsModelTest fixture

class OrganizationsModelTest: public testing::Test
{
public:
    virtual void SetUp() override;

    // Channel partners with organizations, folders and systems; see the 'removal' test for the
    // resulting tree.
    void givenTestStructure();

    // Organizations with folders and systems having the same names.
    void givenTestStructureWithSameNames();

    // An organization with folders and systems not sorted by name.
    void givenUnsortedOrganization();

    // A single organization with no folders, and an empty systems model.
    void givenOrganization();

    void givenSystemsModel();
    void givenSystemInSystemsModel(const SystemInOrganization& system, bool isPending);
    QString givenLocalSite(const QString& name = "Local system 1");
    void givenCloudSite(const QString& name);

    // Linearized model with all nodes expanded, as it's used in search.
    void givenLinearizationModel();

    void givenSortModel();

    // Model signals emitted before this call are not taken into account.
    void givenModelSignalsRecorded();

    // Insertions and removals of rows under indexes which are not in the model are recorded.
    void givenInvalidParentsRecorded();

    void whenChannelPartnersUpdated(const ChannelPartnerList& partners);
    void whenOrganizationsUpdated(const OrganizationList& organizations, nx::Uuid parentId);
    void whenOrgStructureUpdated(nx::Uuid orgId, const std::vector<GroupStructure>& structure);
    void whenOrgSystemsUpdated(nx::Uuid orgId, const std::vector<SystemInOrganization>& systems);
    void whenSiteLost(const QString& id);
    void whenSystemsModelRemoved();

    void thenTreeIs(const QStringList& tree);
    void thenChildrenAre(nx::Uuid parentId, const QStringList& names);

    // Names of all items under the node in depth-first order.
    void thenSubtreeIs(nx::Uuid parentId, const QStringList& names);

    void thenOrg1HasInitialContent();
    void thenChildCount(nx::Uuid parentId, int count);
    void thenParentIs(nx::Uuid id, nx::Uuid parentId);
    void thenNodeIsInModel(nx::Uuid id, bool isInModel);
    void thenNodeType(nx::Uuid id, OrganizationsModel::NodeType type);
    void thenTabSection(nx::Uuid id, OrganizationsModel::TabSection tab);
    void thenPathFromRoot(nx::Uuid id, const QString& path);
    void thenHasChildren(nx::Uuid id, bool hasChildren);
    void thenHasFolders(nx::Uuid id, bool hasFolders);
    void thenChildSystemsAre(nx::Uuid parentId, const std::vector<nx::Uuid>& systemIds);
    void thenSystemIds(nx::Uuid systemId, nx::Uuid orgId);
    void thenIsFromSites(nx::Uuid id, bool isFromSites);
    void thenOrgSystemIsShown(const SystemInOrganization& system, bool isShown);
    void thenOrgSystemCount(nx::Uuid orgId, int count);
    void thenSitesNodeIsFirst();
    void thenSitesAre(const QStringList& names);
    void thenSiteIsUnderSitesNode(const QString& id, const QString& section);
    void thenHasChannelPartners(bool hasChannelPartners, int changeCount);
    void thenHasOrganizations(bool hasOrganizations, int changeCount);
    void thenNoRowsInsertedOrRemoved();
    void thenNoModelChanges();
    void thenRowsInsertedOnce(const QModelIndex& parent, int firstRow);
    void thenDataChangedOnce(nx::Uuid id);
    void thenFoldersChangedLastIn(nx::Uuid orgId);
    void thenNoInvalidParents();
    void thenLinearizationMatchesModel();
    void thenSortedTreeIs(const QStringList& tree);

public:
    std::unique_ptr<TestSystemsController> controller;
    std::unique_ptr<QnSystemsModel> systemsModel;

    OrganizationsModel model;
    std::unique_ptr<QAbstractItemModelTester> modelTester;

    QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    QSignalSpy dataChangedSpy{&model, &QAbstractItemModel::dataChanged};
    QSignalSpy foldersChangedSpy{&model, &OrganizationsModel::foldersChanged};
    QSignalSpy hasChannelPartnersChangedSpy{
        &model, &OrganizationsModel::hasChannelPartnersChanged};
    QSignalSpy hasOrganizationsChangedSpy{&model, &OrganizationsModel::hasOrganizationsChanged};
    QStringList invalidParents;

    LinearizationListModel linearizationModel;
    OrganizationsSortModel sortModel;

    ChannelPartnerList partnerList;
    OrganizationList orgsList;
    SystemInOrganization system1;
    SystemInOrganization system2;
    std::vector<GroupStructure> org1Structure;

    nx::Uuid cp1Id = nx::Uuid::createUuid();
    nx::Uuid org1Id = nx::Uuid::createUuid();
    nx::Uuid org2Id = nx::Uuid::createUuid();
    nx::Uuid group1Id = nx::Uuid::createUuid();
    nx::Uuid subGroup1Id = nx::Uuid::createUuid();
    nx::Uuid subGroup2Id = nx::Uuid::createUuid();
    nx::Uuid system1Id = nx::Uuid::createUuid();
    nx::Uuid system2Id = nx::Uuid::createUuid();
};

void OrganizationsModelTest::SetUp()
{
    // OrganizationsModel is not fully compliant only regarding layout changes and moves in
    // the systems model, which don't happen in these tests.
    modelTester = std::make_unique<QAbstractItemModelTester>(
        &model, QAbstractItemModelTester::FailureReportingMode::Fatal);
}

void OrganizationsModelTest::givenTestStructure()
{
    partnerList = {
        .count = 3,
        .results = {
            {
                .id = cp1Id,
                .name = "ChannelPartner1",
                .partnerCount = 2,
            },
            {
                .id = nx::Uuid::createUuid(),
                .name = "ChannelPartner2",
                .partnerCount = 0,
            },
            {
                .id = nx::Uuid::createUuid(),
                .name = "ChannelPartner22",
                .partnerCount = 0,
            }
        }
    };

    orgsList = {
        .count = 2,
        .results = {
            {
                .id = org1Id,
                .name = "Org1",
                .systemCount = 1,
                .channelPartner = partnerList.results[0].id,
            },
            {
                .id = org2Id,
                .name = "Org2",
                .systemCount = 2,
                .channelPartner = partnerList.results[0].id,
            }
        }
    };

    OrganizationList orgs2 = {
        .count = 1,
        .results = {
            {
                .id = nx::Uuid::createUuid(),
                .name = "Org3",
                .systemCount = 1,
                .channelPartner = partnerList.results[2].id,
            }
        }
    };

    GroupStructure group1 = {
        .id = group1Id,
        .name = "Group1",
        .children = {
            {
                .id = subGroup1Id,
                .name = "SubGroup1",
                .parentId = group1Id,
            },
            {
                .id = subGroup2Id,
                .name = "SubGroup2",
                .parentId = group1Id,
            }
        }
    };

    nx::Uuid group3Id = nx::Uuid::createUuid();
    GroupStructure group3 = {
        .id = group3Id,
        .name = "Group3",
        .children = {
            {
                .id = nx::Uuid::createUuid(),
                .name = "SubGroup3_2",
                .parentId = group3Id,
            }
        }
    };

    system1 = {
        .systemId = system1Id,
        .name = "System1",
        .state = "active",
        .groupId = group1.children[0].id,
        .system_state = SystemStatus::activated,
    };

    system2 = {
        .systemId = system2Id,
        .name = "System2",
        .state = "active",
        .groupId = group1Id,
        .system_state = SystemStatus::activated,
    };

    model.setChannelPartners(partnerList);
    model.setOrganizations(orgsList, partnerList.results[0].id);

    org1Structure = {group1};
    model.setOrgStructure(orgsList.results[0].id, org1Structure);
    model.setOrgSystems(orgsList.results[0].id, {system1, system2});

    model.setOrganizations(orgs2, partnerList.results[2].id);
    model.setOrgStructure(orgs2.results[0].id, {group3});

    givenSystemsModel();
}

void OrganizationsModelTest::givenTestStructureWithSameNames()
{
    auto cp1Id = nx::Uuid::createUuid();
    auto cp2Id = nx::Uuid::createUuid();
    ChannelPartnerList partnerList = {
        .count = 2,
        .results = {
            {
                .id = cp1Id,
                .name = "Partner 1",
                .partnerCount = 2,
            },
            {
                .id = cp2Id,
                .name = "Partner 2",
                .partnerCount = 0,
            }
        }
    };

    OrganizationList orgs1 = {
        .count = 2,
        .results = {
            {
                .id = nx::Uuid::createUuid(),
                .name = "Organization 1.1",
                .systemCount = 1,
                .channelPartner = cp1Id,
            },
            {
                .id = nx::Uuid::createUuid(),
                .name = "Organization 1.2",
                .systemCount = 1,
                .channelPartner = cp1Id,
            }
        }
    };

    OrganizationList orgs2 = {
        .count = 1,
        .results = {
            {
                .id = nx::Uuid::createUuid(),
                .name = "Organization 2.1",
                .systemCount = 1,
                .channelPartner = cp2Id,
            },
        }
    };

    nx::Uuid group1Id = nx::Uuid::createUuid();
    GroupStructure group1 = {
        .id = group1Id,
        .name = "Folder A",
    };

    nx::Uuid group2Id = nx::Uuid::createUuid();
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Folder B",
    };

    nx::Uuid group3Id = nx::Uuid::createUuid();
    GroupStructure group3 = {
        .id = group3Id,
        .name = "Folder A",
    };

    SystemInOrganization system1 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "Site A",
        .state = "active",
        .system_state = SystemStatus::activated,
    };

    SystemInOrganization system2 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "Site A",
        .state = "active",
        .groupId = group3Id,
        .system_state = SystemStatus::activated,
    };

    model.setChannelPartners(partnerList);
    model.setOrganizations(orgs1, partnerList.results[0].id);

    model.setOrgStructure(orgs1.results[0].id, {group1, group2});
    model.setOrgSystems(orgs1.results[0].id, {system1});

    model.setOrgStructure(orgs1.results[1].id, {group3});
    model.setOrgSystems(orgs1.results[1].id, {system2});

    nx::Uuid group4Id = nx::Uuid::createUuid();
    GroupStructure group4 = {
        .id = group4Id,
        .name = "Folder A",
    };

    SystemInOrganization system4 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "Site A",
        .state = "active",
        .groupId = group4Id,
        .system_state = SystemStatus::activated,
    };

    model.setOrganizations(orgs2, partnerList.results[1].id);
    model.setOrgStructure(orgs2.results[0].id, {group4});
    model.setOrgSystems(orgs2.results[0].id, {system4});

    givenSystemsModel();
}

void OrganizationsModelTest::givenUnsortedOrganization()
{
    OrganizationList orgs = {
        .count = 2,
        .results = {
            {.id = org1Id, .name = "Org B"},
            {.id = org2Id, .name = "Org A"},
        }
    };

    model.setOrganizations(orgs, /*parentId*/ {});
    model.setOrgStructure(org1Id, {
        {.id = group1Id, .name = "Folder 10"},
        {.id = subGroup1Id, .name = "folder 2"},
    });
    model.setOrgSystems(org1Id, {
        {
            .systemId = system1Id,
            .name = "Site B",
            .state = "active",
            .system_state = SystemStatus::activated,
        },
        {
            .systemId = system2Id,
            .name = "Site A",
            .state = "active",
            .system_state = SystemStatus::activated,
        },
    });
}

void OrganizationsModelTest::givenOrganization()
{
    const ChannelPartner partner = {
        .id = cp1Id,
        .name = "ChannelPartner1",
        .partnerCount = 1,
    };

    const Organization org = {
        .id = org1Id,
        .name = "Org1",
        .channelPartner = cp1Id,
    };

    model.setChannelPartners({.count = 1, .results = {partner}});
    model.setOrganizations({.count = 1, .results = {org}}, cp1Id);

    givenSystemsModel();
}

void OrganizationsModelTest::givenSystemsModel()
{
    controller.reset(new TestSystemsController());
    systemsModel.reset(new QnSystemsModel(controller.get()));
    model.setSystemsModel(systemsModel.get());
}

void OrganizationsModelTest::givenSystemInSystemsModel(
    const SystemInOrganization& system, bool isPending)
{
    ASSERT_TRUE(controller->discoverSystem(QnCloudSystemDescription::create({
        .cloudId = system.systemId.toSimpleString(),
        .localId = nx::Uuid::createUuid(),
        .name = QString::fromStdString(system.name),
        .online = false,
        .isPending = isPending,
    })));
}

QString OrganizationsModelTest::givenLocalSite(const QString& name)
{
    const auto id = nx::Uuid::createUuid().toSimpleString();
    EXPECT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        id,
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        name)));
    return id;
}

void OrganizationsModelTest::givenCloudSite(const QString& name)
{
    ASSERT_TRUE(controller->discoverSystem(QnCloudSystemDescription::create({
        .cloudId = nx::Uuid::createUuid().toSimpleString(),
        .localId = nx::Uuid::createUuid(),
        .name = name,
        .online = true,
        .version = "1.0.0",
    })));
}

void OrganizationsModelTest::givenLinearizationModel()
{
    linearizationModel.setSourceModel(&model);
    linearizationModel.setAutoExpandAll(true);
}

void OrganizationsModelTest::givenSortModel()
{
    sortModel.setSourceModel(&model);
}

void OrganizationsModelTest::givenModelSignalsRecorded()
{
    rowsInsertedSpy.clear();
    rowsRemovedSpy.clear();
    dataChangedSpy.clear();
    foldersChangedSpy.clear();
}

void OrganizationsModelTest::givenInvalidParentsRecorded()
{
    const auto recordInvalidParent =
        [this](const QModelIndex& parent)
        {
            if (parent.isValid() && !isModelIndex(parent))
                invalidParents << parent.data().toString();
        };

    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model,
        recordInvalidParent);
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model,
        recordInvalidParent);
}

void OrganizationsModelTest::whenChannelPartnersUpdated(const ChannelPartnerList& partners)
{
    model.setChannelPartners(partners);
}

void OrganizationsModelTest::whenOrganizationsUpdated(
    const OrganizationList& organizations, nx::Uuid parentId)
{
    model.setOrganizations(organizations, parentId);
}

void OrganizationsModelTest::whenOrgStructureUpdated(
    nx::Uuid orgId, const std::vector<GroupStructure>& structure)
{
    model.setOrgStructure(orgId, structure);
}

void OrganizationsModelTest::whenOrgSystemsUpdated(
    nx::Uuid orgId, const std::vector<SystemInOrganization>& systems)
{
    model.setOrgSystems(orgId, systems);
}

void OrganizationsModelTest::whenSiteLost(const QString& id)
{
    ASSERT_TRUE(controller->loseSystem(id));
}

void OrganizationsModelTest::whenSystemsModelRemoved()
{
    model.setSystemsModel(nullptr);
}

void OrganizationsModelTest::thenTreeIs(const QStringList& tree)
{
    EXPECT_EQ(tree, dumpTree(model));
}

void OrganizationsModelTest::thenChildrenAre(nx::Uuid parentId, const QStringList& names)
{
    EXPECT_EQ(names, dumpNames(model, model.indexFromNodeId(parentId)));
}

void OrganizationsModelTest::thenSubtreeIs(nx::Uuid parentId, const QStringList& names)
{
    EXPECT_EQ(names, dumpNamesRecursively(model, model.indexFromNodeId(parentId)));
}

void OrganizationsModelTest::thenOrg1HasInitialContent()
{
    thenSubtreeIs(org1Id, {"Group1", "SubGroup1", "System1", "SubGroup2", "System2"});
}

void OrganizationsModelTest::thenChildCount(nx::Uuid parentId, int count)
{
    EXPECT_EQ(count, model.rowCount(model.indexFromNodeId(parentId)));
}

void OrganizationsModelTest::thenParentIs(nx::Uuid id, nx::Uuid parentId)
{
    const auto index = model.indexFromNodeId(id);
    ASSERT_TRUE(index.isValid());
    EXPECT_EQ(model.indexFromNodeId(parentId), index.parent());
}

void OrganizationsModelTest::thenNodeIsInModel(nx::Uuid id, bool isInModel)
{
    EXPECT_EQ(isInModel, model.indexFromNodeId(id).isValid());
}

void OrganizationsModelTest::thenNodeType(nx::Uuid id, OrganizationsModel::NodeType type)
{
    EXPECT_EQ(type, model.indexFromNodeId(id).data(OrganizationsModel::TypeRole)
        .value<OrganizationsModel::NodeType>());
}

void OrganizationsModelTest::thenTabSection(nx::Uuid id, OrganizationsModel::TabSection tab)
{
    EXPECT_EQ(tab, model.indexFromNodeId(id).data(OrganizationsModel::TabSectionRole)
        .value<OrganizationsModel::TabSection>());
}

void OrganizationsModelTest::thenPathFromRoot(nx::Uuid id, const QString& path)
{
    EXPECT_EQ(path,
        model.indexFromNodeId(id).data(OrganizationsModel::PathFromRootRole).toString());
}

void OrganizationsModelTest::thenHasChildren(nx::Uuid id, bool hasChildren)
{
    EXPECT_EQ(hasChildren, model.hasChildren(model.indexFromNodeId(id)));
}

void OrganizationsModelTest::thenHasFolders(nx::Uuid id, bool hasFolders)
{
    EXPECT_EQ(hasFolders, model.hasFolders(model.indexFromNodeId(id)));
}

void OrganizationsModelTest::thenChildSystemsAre(
    nx::Uuid parentId, const std::vector<nx::Uuid>& systemIds)
{
    QStringList ids;
    for (const auto& id: systemIds)
        ids << id.toSimpleString();

    EXPECT_EQ(ids, model.childSystemIds(model.indexFromNodeId(parentId)));
}

void OrganizationsModelTest::thenSystemIds(nx::Uuid systemId, nx::Uuid orgId)
{
    const auto system = model.indexFromNodeId(systemId);
    EXPECT_EQ(systemId, system.data(OrganizationsModel::IdRole).value<nx::Uuid>());
    EXPECT_EQ(systemId.toSimpleString(),
        system.data(QnSystemsModel::SystemIdRoleId).toString());
    EXPECT_EQ(orgId, system.data(QnSystemsModel::OrganizationIdRole).value<nx::Uuid>());
}

void OrganizationsModelTest::thenIsFromSites(nx::Uuid id, bool isFromSites)
{
    EXPECT_EQ(isFromSites,
        model.indexFromNodeId(id).data(OrganizationsModel::IsFromSitesRole).toBool());
}

void OrganizationsModelTest::thenOrgSystemIsShown(const SystemInOrganization& system, bool isShown)
{
    EXPECT_EQ(isShown, model.indexFromNodeId(system.systemId).isValid()) << system.name;
}

void OrganizationsModelTest::thenOrgSystemCount(nx::Uuid orgId, int count)
{
    EXPECT_EQ(count,
        model.indexFromNodeId(orgId).data(OrganizationsModel::SystemCountRole).toInt());
}

void OrganizationsModelTest::thenSitesNodeIsFirst()
{
    const auto sitesRoot = model.sitesRoot();
    EXPECT_EQ(model.index(0, 0), sitesRoot);
    EXPECT_EQ(OrganizationsModel::SitesNode,
        sitesRoot.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>());
}

void OrganizationsModelTest::thenSitesAre(const QStringList& names)
{
    EXPECT_EQ(names, dumpNames(model, model.sitesRoot()));
}

void OrganizationsModelTest::thenSiteIsUnderSitesNode(const QString& id, const QString& section)
{
    const auto site = model.indexFromSystemId(id);
    EXPECT_EQ(model.sitesRoot(), site.parent());
    EXPECT_TRUE(site.data(OrganizationsModel::IsFromSitesRole).toBool());
    EXPECT_EQ(section, site.data(OrganizationsModel::SectionRole).toString());
    EXPECT_EQ(OrganizationsModel::SitesTab,
        site.data(OrganizationsModel::TabSectionRole).value<OrganizationsModel::TabSection>());
}

void OrganizationsModelTest::thenHasChannelPartners(bool hasChannelPartners, int changeCount)
{
    EXPECT_EQ(hasChannelPartners, model.hasChannelPartners());
    EXPECT_EQ(changeCount, hasChannelPartnersChangedSpy.count());
}

void OrganizationsModelTest::thenHasOrganizations(bool hasOrganizations, int changeCount)
{
    EXPECT_EQ(hasOrganizations, model.hasOrganizations());
    EXPECT_EQ(changeCount, hasOrganizationsChangedSpy.count());
}

void OrganizationsModelTest::thenNoRowsInsertedOrRemoved()
{
    EXPECT_TRUE(rowsInsertedSpy.empty());
    EXPECT_TRUE(rowsRemovedSpy.empty());
}

void OrganizationsModelTest::thenNoModelChanges()
{
    thenNoRowsInsertedOrRemoved();
    EXPECT_TRUE(dataChangedSpy.empty());
}

void OrganizationsModelTest::thenRowsInsertedOnce(const QModelIndex& parent, int firstRow)
{
    ASSERT_EQ(1, rowsInsertedSpy.count());
    EXPECT_EQ(parent, rowsInsertedSpy[0][0].toModelIndex());
    EXPECT_EQ(firstRow, rowsInsertedSpy[0][1].toInt());
}

void OrganizationsModelTest::thenDataChangedOnce(nx::Uuid id)
{
    ASSERT_EQ(1, dataChangedSpy.count());
    EXPECT_EQ(model.indexFromNodeId(id), dataChangedSpy[0][0].toModelIndex());
}

void OrganizationsModelTest::thenFoldersChangedLastIn(nx::Uuid orgId)
{
    ASSERT_FALSE(foldersChangedSpy.empty());
    EXPECT_EQ(model.indexFromNodeId(orgId), foldersChangedSpy.last()[0].toModelIndex());
}

void OrganizationsModelTest::thenNoInvalidParents()
{
    EXPECT_TRUE(invalidParents.empty())
        << "Rows inserted or removed under: " << invalidParents.join(", ").toStdString();
}

void OrganizationsModelTest::thenLinearizationMatchesModel()
{
    EXPECT_EQ(dumpNamesRecursively(model), dumpNames(linearizationModel));
}

void OrganizationsModelTest::thenSortedTreeIs(const QStringList& tree)
{
    EXPECT_EQ(tree, dumpTree(sortModel));
}

// ------------------------------------------------------------------------------------------------
// OrganizationsModel tests

TEST_F(OrganizationsModelTest, removal)
{
    givenTestStructure();
    thenTreeIs({
        "Sites: ",
        "ChannelPartner1: Partners",
        "  Org1: Organizations",
        "    Group1: Folders",
        "      SubGroup1: Folders",
        "        System1: Sites",
        "      SubGroup2: Folders",
        "      System2: Sites",
        "  Org2: Organizations",
        "ChannelPartner2: Partners",
        "ChannelPartner22: Partners",
        "  Org3: Organizations",
        "    Group3: Folders",
        "      SubGroup3_2: Folders",
    });

    // Remove systems from 'Org1'.
    whenOrgSystemsUpdated(org1Id, {});
    thenTreeIs({
        "Sites: ",
        "ChannelPartner1: Partners",
        "  Org1: Organizations",
        "    Group1: Folders",
        "      SubGroup1: Folders",
        "      SubGroup2: Folders",
        "  Org2: Organizations",
        "ChannelPartner2: Partners",
        "ChannelPartner22: Partners",
        "  Org3: Organizations",
        "    Group3: Folders",
        "      SubGroup3_2: Folders",
    });

    // Remove 'SubGroup1' by updating 'Group1'.
    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {{.id = subGroup2Id, .name = "SubGroup2", .parentId = group1Id}},
    }});
    thenTreeIs({
        "Sites: ",
        "ChannelPartner1: Partners",
        "  Org1: Organizations",
        "    Group1: Folders",
        "      SubGroup2: Folders",
        "  Org2: Organizations",
        "ChannelPartner2: Partners",
        "ChannelPartner22: Partners",
        "  Org3: Organizations",
        "    Group3: Folders",
        "      SubGroup3_2: Folders",
    });

    // Remove all channel partners.
    whenChannelPartnersUpdated({});
    thenTreeIs({"Sites: "});
}

TEST_F(OrganizationsModelTest, channelPartnerRoles)
{
    givenTestStructure();

    thenNodeType(cp1Id, OrganizationsModel::ChannelPartner);
    thenTabSection(cp1Id, OrganizationsModel::ChannelPartnersTab);
    thenPathFromRoot(cp1Id, "Partners");
}

TEST_F(OrganizationsModelTest, organizationRoles)
{
    givenTestStructure();

    thenNodeType(org1Id, OrganizationsModel::Organization);
    thenParentIs(org1Id, cp1Id);

    // System count is updated from the actual list of organization systems.
    thenOrgSystemCount(org1Id, 2);
}

TEST_F(OrganizationsModelTest, organizationSystemRoles)
{
    givenTestStructure();

    thenNodeType(system1Id, OrganizationsModel::System);
    thenSystemIds(system1Id, org1Id);
    thenIsFromSites(system1Id, false);
    thenPathFromRoot(system1Id, "ChannelPartner1 / Org1 / Group1 / SubGroup1");
    thenTabSection(system1Id, OrganizationsModel::ChannelPartnersTab);
}

TEST_F(OrganizationsModelTest, sitesNode)
{
    givenTestStructure();

    thenSitesNodeIsFirst();
    thenSitesAre({});
}

TEST_F(OrganizationsModelTest, hasChildrenAndFolders)
{
    givenTestStructure();

    thenHasChildren(group1Id, true);
    thenHasChildren(subGroup2Id, false);

    thenHasFolders(org1Id, true);
    thenHasFolders(group1Id, true);
    thenHasFolders(subGroup1Id, false);
    thenHasFolders(org2Id, false);
}

TEST_F(OrganizationsModelTest, childSystemIds)
{
    givenTestStructure();

    thenChildSystemsAre(org1Id, {system1Id, system2Id});
    thenChildSystemsAre(subGroup1Id, {system1Id});
    thenChildSystemsAre(subGroup2Id, {});
}

TEST_F(OrganizationsModelTest, unknownNodeIsNotInModel)
{
    givenTestStructure();

    thenNodeIsInModel(nx::Uuid::createUuid(), false);
}

TEST_F(OrganizationsModelTest, hasChannelPartnersAndOrganizations)
{
    thenHasChannelPartners(false, /*changeCount*/ 0);
    thenHasOrganizations(false, /*changeCount*/ 0);

    const ChannelPartnerList partners = {
        .count = 1,
        .results = {{.id = cp1Id, .name = "Partner"}},
    };

    whenChannelPartnersUpdated(partners);
    thenHasChannelPartners(true, /*changeCount*/ 1);

    // Organizations directly under the root are not related to channel partners.
    const OrganizationList orgs = {
        .count = 1,
        .results = {{.id = org1Id, .name = "Organization"}},
    };

    whenOrganizationsUpdated(orgs, /*parentId*/ {});
    thenHasOrganizations(true, /*changeCount*/ 1);
    thenTreeIs({
        "Sites: ",
        "Partner: Partners",
        "Organization: Organizations",
    });
    thenTabSection(org1Id, OrganizationsModel::OrganizationsTab);

    // Organizations under a channel partner do not affect `hasOrganizations`.
    whenOrganizationsUpdated({}, /*parentId*/ {});
    whenOrganizationsUpdated(orgs, cp1Id);
    thenHasOrganizations(false, /*changeCount*/ 2);

    // Same data does not emit signals.
    whenChannelPartnersUpdated(partners);
    thenHasChannelPartners(true, /*changeCount*/ 1);

    whenChannelPartnersUpdated({});
    thenHasChannelPartners(false, /*changeCount*/ 2);
    thenTreeIs({"Sites: "});
}

TEST_F(OrganizationsModelTest, channelPartnerRenamedAndAdded)
{
    givenTestStructure();
    givenModelSignalsRecorded();

    partnerList.results[0].name = "Partner1";
    partnerList.results.push_back({.id = nx::Uuid::createUuid(), .name = "ChannelPartner3"});
    whenChannelPartnersUpdated(partnerList);

    thenDataChangedOnce(cp1Id);
    thenRowsInsertedOnce(/*parent*/ {}, /*firstRow*/ 4);
    thenTreeIs({
        "Sites: ",
        "Partner1: Partners",
        "  Org1: Organizations",
        "    Group1: Folders",
        "      SubGroup1: Folders",
        "        System1: Sites",
        "      SubGroup2: Folders",
        "      System2: Sites",
        "  Org2: Organizations",
        "ChannelPartner2: Partners",
        "ChannelPartner22: Partners",
        "  Org3: Organizations",
        "    Group3: Folders",
        "      SubGroup3_2: Folders",
        "ChannelPartner3: Partners",
    });
}

TEST_F(OrganizationsModelTest, folderMovedToNewFolder)
{
    givenTestStructure();
    givenModelSignalsRecorded();

    // Move 'SubGroup2' from 'Group1' to a new folder 'Group2'.
    const auto group2Id = nx::Uuid::createUuid();
    GroupStructure group1 = {
        .id = group1Id,
        .name = "Group1",
        .children = {{.id = subGroup1Id, .name = "SubGroup1", .parentId = group1Id}},
    };
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {{.id = subGroup2Id, .name = "SubGroup2", .parentId = group2Id}},
    };

    whenOrgStructureUpdated(org1Id, {group1, group2});

    thenFoldersChangedLastIn(org1Id);
    thenChildrenAre(org1Id, {"Group1", "Group2"});
    thenChildrenAre(group1Id, {"SubGroup1", "System2"});
    thenChildrenAre(group2Id, {"SubGroup2"});
    thenPathFromRoot(subGroup2Id, "ChannelPartner1 / Org1 / Group2");
}

TEST_F(OrganizationsModelTest, sameStructureCausesNoChanges)
{
    givenTestStructure();
    givenModelSignalsRecorded();

    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {
            {.id = subGroup1Id, .name = "SubGroup1", .parentId = group1Id},
            {.id = subGroup2Id, .name = "SubGroup2", .parentId = group1Id},
        },
    }});

    thenNoModelChanges();
}

TEST_F(OrganizationsModelTest, systemMovedToAnotherFolder)
{
    givenTestStructure();

    // Move 'System1' from 'SubGroup1' to 'Group1', which already has children.
    system1.groupId = group1Id;
    whenOrgSystemsUpdated(org1Id, {system1, system2});

    // A moved system is appended to its new folder.
    thenChildrenAre(group1Id, {"SubGroup1", "SubGroup2", "System2", "System1"});
    thenChildSystemsAre(subGroup1Id, {});
    thenOrgSystemCount(org1Id, 2);
}

TEST_F(OrganizationsModelTest, systemRenamed)
{
    givenTestStructure();

    system2.name = "System2a";
    whenOrgSystemsUpdated(org1Id, {system1, system2});

    // A renamed system keeps its position.
    thenChildrenAre(group1Id, {"SubGroup1", "SubGroup2", "System2a"});
}

// Organizations are loaded concurrently, so when a system is moved to another organization, the
// new organization can be updated before or after the previous one.

TEST_F(OrganizationsModelTest, systemMovedToAnotherOrganizationUpdatedFirst)
{
    givenTestStructure();

    // Move 'System1' from 'Org1' to 'Org2'.
    auto movedSystem1 = system1;
    movedSystem1.groupId = {};

    whenOrgSystemsUpdated(org2Id, {movedSystem1});
    whenOrgSystemsUpdated(org1Id, {system2});

    thenParentIs(system1Id, org2Id);
    thenOrgSystemCount(org1Id, 1);
    thenOrgSystemCount(org2Id, 1);

    // The previous organization doesn't take it back when its structure is restored.
    whenOrgStructureUpdated(org1Id, {});
    whenOrgStructureUpdated(org1Id, org1Structure);
    thenParentIs(system1Id, org2Id);
}

TEST_F(OrganizationsModelTest, systemMovedToAnotherOrganizationUpdatedLast)
{
    givenTestStructure();

    // Move 'System1' from 'Org1' to 'Org2'.
    auto movedSystem1 = system1;
    movedSystem1.groupId = {};

    whenOrgSystemsUpdated(org1Id, {system2});
    whenOrgSystemsUpdated(org2Id, {movedSystem1});

    thenParentIs(system1Id, org2Id);
    thenOrgSystemCount(org1Id, 1);
    thenOrgSystemCount(org2Id, 1);
}

TEST_F(OrganizationsModelTest, systemMovedToAnotherOrganizationBeforePreviousOneUpdated)
{
    givenTestStructure();

    // Move 'System1' from 'Org1' to 'Org2'; 'Org1' is not updated yet.
    auto movedSystem1 = system1;
    movedSystem1.groupId = {};

    whenOrgSystemsUpdated(org2Id, {movedSystem1});

    // The previous organization doesn't count it anymore.
    thenParentIs(system1Id, org2Id);
    thenOrgSystemCount(org1Id, 1);
}

TEST_F(OrganizationsModelTest, systemRemoved)
{
    givenTestStructure();

    whenOrgSystemsUpdated(org1Id, {system2});

    thenNodeIsInModel(system1Id, false);
    thenChildSystemsAre(subGroup1Id, {});
    thenOrgSystemCount(org1Id, 1);
}

TEST_F(OrganizationsModelTest, organizationRemoved)
{
    givenTestStructure();

    // Remove 'Org2'.
    orgsList.results.pop_back();
    whenOrganizationsUpdated(orgsList, cp1Id);

    thenNodeIsInModel(org2Id, false);
    thenChildrenAre(cp1Id, {"Org1"});
}

TEST_F(OrganizationsModelTest, organizationRemovedAndAddedBack)
{
    givenTestStructure();

    // Remove 'Org1'.
    whenOrganizationsUpdated({.count = 1, .results = {orgsList.results[1]}}, cp1Id);
    thenNodeIsInModel(org1Id, false);

    // Add it back with the same content, in the same order as in loading.
    whenOrganizationsUpdated(orgsList, cp1Id);
    whenOrgStructureUpdated(org1Id, org1Structure);
    whenOrgSystemsUpdated(org1Id, {system1, system2});

    thenOrg1HasInitialContent();
}

TEST_F(OrganizationsModelTest, channelPartnerRemovedAndAddedBack)
{
    givenTestStructure();

    // Remove 'ChannelPartner1' along with its organizations.
    auto partners = partnerList;
    partners.results.erase(partners.results.begin());
    whenChannelPartnersUpdated(partners);
    thenNodeIsInModel(org1Id, false);

    // Add it back with the same content, in the same order as in loading.
    whenChannelPartnersUpdated(partnerList);
    whenOrganizationsUpdated(orgsList, cp1Id);
    whenOrgStructureUpdated(org1Id, org1Structure);
    whenOrgSystemsUpdated(org1Id, {system1, system2});

    thenOrg1HasInitialContent();
}

TEST_F(OrganizationsModelTest, sitesSystemDiscoveredAndLost)
{
    givenTestStructure();

    const auto localSiteId = givenLocalSite("Local system 1");
    thenSitesAre({"Local system 1"});
    thenSiteIsUnderSitesNode(localSiteId, /*section*/ "Local");

    whenSiteLost(localSiteId);
    thenSitesAre({});
}

TEST_F(OrganizationsModelTest, systemsModelRemoved)
{
    givenTestStructure();
    givenLocalSite();
    thenSitesAre({"Local system 1"});

    whenSystemsModelRemoved();
    thenSitesAre({});
}

TEST_F(OrganizationsModelTest, folderWithSystemsMoved)
{
    givenTestStructure();

    // Move 'SubGroup1', which contains 'System1', from 'Group1' to a new folder 'Group2'.
    const auto group2Id = nx::Uuid::createUuid();
    GroupStructure group1 = {
        .id = group1Id,
        .name = "Group1",
        .children = {{.id = subGroup2Id, .name = "SubGroup2", .parentId = group1Id}},
    };
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {{.id = subGroup1Id, .name = "SubGroup1", .parentId = group2Id}},
    };

    // Systems of the moved folder are kept without setOrgSystems() call, e.g. if loading of
    // systems fails.
    whenOrgStructureUpdated(org1Id, {group1, group2});

    thenParentIs(system1Id, subGroup1Id);
    thenParentIs(subGroup1Id, group2Id);
    thenOrgSystemCount(org1Id, 2);

    // Systems reference their folders by id, so the system list doesn't change.
    givenModelSignalsRecorded();
    whenOrgSystemsUpdated(org1Id, {system1, system2});
    thenNoRowsInsertedOrRemoved();
}

TEST_F(OrganizationsModelTest, folderWithSystemsMovedIntoExistingFolder)
{
    givenTestStructure();

    // Move 'SubGroup1', which contains 'System1', from 'Group1' into existing 'SubGroup2'.
    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {
            {
                .id = subGroup2Id,
                .name = "SubGroup2",
                .parentId = group1Id,
                .children = {{.id = subGroup1Id, .name = "SubGroup1", .parentId = subGroup2Id}},
            },
        },
    }});

    thenParentIs(system1Id, subGroup1Id);
    thenParentIs(subGroup1Id, subGroup2Id);
    thenOrgSystemCount(org1Id, 2);
}

TEST_F(OrganizationsModelTest, structureClearedAndRestored)
{
    givenTestStructure();

    // Clearing the structure removes the folders along with their systems. Systems of
    // non-existent folders are not shown. The order of calls is the same as in loading.
    whenOrgStructureUpdated(org1Id, {});
    whenOrgSystemsUpdated(org1Id, {system1, system2});
    thenChildCount(org1Id, 0);

    // Systems are restored along with their folders.
    whenOrgStructureUpdated(org1Id, org1Structure);
    thenOrg1HasInitialContent();
}

TEST_F(OrganizationsModelTest, folderMovedIntoSiblingFolder)
{
    givenTestStructure();

    // Move 'SubGroup2' from 'Group1' into 'SubGroup1'.
    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {
            {
                .id = subGroup1Id,
                .name = "SubGroup1",
                .parentId = group1Id,
                .children = {{.id = subGroup2Id, .name = "SubGroup2", .parentId = subGroup1Id}},
            },
        },
    }});

    thenParentIs(subGroup2Id, subGroup1Id);
    thenChildrenAre(group1Id, {"SubGroup1", "System2"});
}

TEST_F(OrganizationsModelTest, rowsInsertedUnderModelIndexes)
{
    givenInvalidParentsRecorded();

    // Contains folders with subfolders.
    givenTestStructure();

    thenNoInvalidParents();
}

TEST_F(OrganizationsModelTest, linearizationOfNewFoldersWithSubfolders)
{
    givenTestStructure();
    givenLocalSite(); //< 'Sites' node has children.
    givenLinearizationModel();
    thenLinearizationMatchesModel();

    // Add new folders with one and two subfolders to 'Org1' at once.
    const auto group2Id = nx::Uuid::createUuid();
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {{.id = nx::Uuid::createUuid(), .name = "SubGroup2_1", .parentId = group2Id}},
    };
    const auto group4Id = nx::Uuid::createUuid();
    GroupStructure group4 = {
        .id = group4Id,
        .name = "Group4",
        .children = {
            {.id = nx::Uuid::createUuid(), .name = "SubGroup4_1", .parentId = group4Id},
            {.id = nx::Uuid::createUuid(), .name = "SubGroup4_2", .parentId = group4Id},
        },
    };

    whenOrgStructureUpdated(org1Id, {org1Structure[0], group2, group4});

    thenLinearizationMatchesModel();
}

TEST_F(OrganizationsModelTest, structureWithDuplicateIds)
{
    givenTestStructure();
    givenInvalidParentsRecorded();

    // A new folder with two subfolders having the same id.
    const auto group2Id = nx::Uuid::createUuid();
    const auto duplicateId = nx::Uuid::createUuid();
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {
            {.id = duplicateId, .name = "Duplicate1", .parentId = group2Id},
            {.id = duplicateId, .name = "Duplicate2", .parentId = group2Id},
        },
    };

    whenOrgStructureUpdated(org1Id, {org1Structure[0], group2});

    thenNoInvalidParents();
    thenChildCount(group2Id, 1);
}

TEST_F(OrganizationsModelTest, structureWithRecursiveIds)
{
    givenTestStructure();

    // A new folder containing a folder which contains a folder with the same id as the first one.
    const auto group2Id = nx::Uuid::createUuid();
    const auto group3Id = nx::Uuid::createUuid();
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {
            {
                .id = group3Id,
                .name = "Group3",
                .parentId = group2Id,
                .children = {{.id = group2Id, .name = "Group2", .parentId = group3Id}},
            },
        },
    };

    whenOrgStructureUpdated(org1Id, {org1Structure[0], group2});

    // Nodes outside of the organization are not affected: removing the recursive group could
    // remove the 'Sites' node instead.
    thenSitesNodeIsFirst();

    // The recursive group is skipped, the rest of the structure is intact.
    thenParentIs(group3Id, group2Id);
    thenSubtreeIs(org1Id, {
        "Group1",
        "SubGroup1",
        "System1",
        "SubGroup2",
        "System2",
        "Group2",
        "Group3",
    });
}

TEST_F(OrganizationsModelTest, structureWithFolderInsideItself)
{
    givenTestStructure();

    // 'Group1' is listed inside its own subfolder 'SubGroup1'.
    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {
            {
                .id = subGroup1Id,
                .name = "SubGroup1",
                .parentId = group1Id,
                .children = {{.id = group1Id, .name = "Group1", .parentId = subGroup1Id}},
            },
            {.id = subGroup2Id, .name = "SubGroup2", .parentId = group1Id},
        },
    }});

    thenOrg1HasInitialContent();
}

TEST_F(OrganizationsModelTest, structureWithNewFolderContainingAncestor)
{
    givenTestStructure();

    // A new folder 'Group2' in 'SubGroup2' contains 'Group1', which is an ancestor of 'SubGroup2'.
    const auto group2Id = nx::Uuid::createUuid();
    whenOrgStructureUpdated(org1Id, {{
        .id = group1Id,
        .name = "Group1",
        .children = {
            {.id = subGroup1Id, .name = "SubGroup1", .parentId = group1Id},
            {
                .id = subGroup2Id,
                .name = "SubGroup2",
                .parentId = group1Id,
                .children = {
                    {
                        .id = group2Id,
                        .name = "Group2",
                        .parentId = subGroup2Id,
                        .children = {{.id = group1Id, .name = "Group1", .parentId = group2Id}},
                    },
                },
            },
        },
    }});

    thenSubtreeIs(org1Id, {
        "Group1",
        "SubGroup1",
        "System1",
        "SubGroup2",
        "Group2",
        "System2",
    });
}

TEST_F(OrganizationsModelTest, sortModel)
{
    givenUnsortedOrganization();
    givenSortModel();

    // Organizations keep their original order, folders and systems are sorted by name.
    thenSortedTreeIs({
        "Sites: ",
        "Org B: Organizations",
        "  folder 2: Folders",
        "  Folder 10: Folders",
        "  Site A: Sites",
        "  Site B: Sites",
        "Org A: Organizations",
    });
}

TEST_F(OrganizationsModelTest, activatedSystemIsShown)
{
    givenOrganization();
    const auto system = makeSystem("Activated", SystemStatus::activated);

    whenOrgSystemsUpdated(org1Id, {system});

    thenOrgSystemIsShown(system, true);
}

TEST_F(OrganizationsModelTest, pendingSystemIsShown)
{
    givenOrganization();
    const auto system = makeSystem("Pending", SystemStatus::deploymentReady);
    givenSystemInSystemsModel(system, /*isPending*/ true);

    whenOrgSystemsUpdated(org1Id, {system});

    thenOrgSystemIsShown(system, true);
}

// The tile of an organization Site takes most of its data from the systems model, so a Site which
// is missing there can't be displayed properly (see MOBILE-2662).
TEST_F(OrganizationsModelTest, pendingSystemMissingFromSystemsModelIsHidden)
{
    givenOrganization();
    const auto system = makeSystem("Pending", SystemStatus::deploymentReady);

    whenOrgSystemsUpdated(org1Id, {system});

    thenOrgSystemIsShown(system, false);
}

TEST_F(OrganizationsModelTest, pendingSystemNotPendingInSystemsModelIsHidden)
{
    givenOrganization();
    const auto system = makeSystem("Pending", SystemStatus::deploymentReady);
    givenSystemInSystemsModel(system, /*isPending*/ false);

    whenOrgSystemsUpdated(org1Id, {system});

    thenOrgSystemIsShown(system, false);
}

TEST_F(OrganizationsModelTest, systemsInOtherStatesAreHidden)
{
    givenOrganization();
    const std::vector<SystemInOrganization> systems = {
        makeSystem("Invalid", SystemStatus::invalid),
        makeSystem("NotActivated", SystemStatus::notActivated),
        makeSystem("Deleted", SystemStatus::deleted_),
        makeSystem("BeingMerged", SystemStatus::beingMerged),
        makeSystem("DeletedByMerge", SystemStatus::deletedByMerge),
    };

    // Even a pending row in the systems model must not make such a Site visible.
    for (const auto& system: systems)
        givenSystemInSystemsModel(system, /*isPending*/ true);

    whenOrgSystemsUpdated(org1Id, systems);

    for (const auto& system: systems)
        thenOrgSystemIsShown(system, false);
}

TEST_F(OrganizationsModelTest, systemCountIncludesOnlyShownSystems)
{
    givenOrganization();
    const auto activated = makeSystem("Activated", SystemStatus::activated);
    const auto pending = makeSystem("Pending", SystemStatus::deploymentReady);
    const auto unknownPending = makeSystem("UnknownPending", SystemStatus::deploymentReady);
    const auto notActivated = makeSystem("NotActivated", SystemStatus::notActivated);
    givenSystemInSystemsModel(pending, /*isPending*/ true);

    whenOrgSystemsUpdated(org1Id, {activated, pending, unknownPending, notActivated});

    thenOrgSystemCount(org1Id, 2);
}

// Organizations and the systems model are updated independently, so a pending Site may come with
// the organization before the systems model knows it. It must appear with the next update.
TEST_F(OrganizationsModelTest, pendingSystemIsShownOnUpdateAfterDiscovery)
{
    givenOrganization();
    const auto system = makeSystem("Pending", SystemStatus::deploymentReady);

    whenOrgSystemsUpdated(org1Id, {system});
    thenOrgSystemIsShown(system, false);
    thenOrgSystemCount(org1Id, 0);

    givenSystemInSystemsModel(system, /*isPending*/ true);
    whenOrgSystemsUpdated(org1Id, {system});
    thenOrgSystemIsShown(system, true);
    thenOrgSystemCount(org1Id, 1);
}

// ------------------------------------------------------------------------------------------------
// OrganizationsFilterModelTest fixture

// Search and filtering of the linearized OrganizationsModel, as it's done in the UI.
class OrganizationsFilterModelTest: public OrganizationsModelTest
{
public:
    void givenFilterModel();

    // An organization directly under the root, i.e. not under a channel partner.
    nx::Uuid givenTopLevelOrganization(const std::string& name);

    void whenPartnersTabOpened();
    void whenOrganizationsTabOpened();
    void whenSitesTabOpened();
    void whenNodeOpened(nx::Uuid id);
    void whenSearching(const QString& text);
    void whenShowOnly(const QList<OrganizationsModel::NodeType>& types);
    void whenHideOrgSystemsFromSites(bool value);

    void thenSearchResultsAre(const QStringList& tree);
    void thenFilteredNamesAre(const QStringList& names);
    void thenFilteredIdsAre(const std::vector<nx::Uuid>& ids);
    void thenFilteredRowCount(int count);
    void thenFilteredCounts(int partnerCount, int organizationCount, int siteCount);
    void thenFilteredPartnerCount(int count);
    void thenSitesNodeIsFilteredOut();
    void thenShowOnlyChangeCount(int count);
    void thenHideOrgSystemsFromSitesChangeCount(int count);
    void thenCurrentTabChangeCount(int count);
    void thenCurrentRoot(nx::Uuid id);
    void thenNoCurrentRoot();
    void thenCurrentRootChangeCount(int count);

public:
    OrganizationsFilterModel filterModel;
    QSignalSpy showOnlyChangedSpy{&filterModel, &OrganizationsFilterModel::showOnlyChanged};
    QSignalSpy hideOrgSystemsFromSitesChangedSpy{
        &filterModel, &OrganizationsFilterModel::hideOrgSystemsFromSitesChanged};
    QSignalSpy currentTabChangedSpy{&filterModel, &OrganizationsFilterModel::currentTabChanged};
    QSignalSpy currentRootChangedSpy{&filterModel, &OrganizationsFilterModel::currentRootChanged};
};

void OrganizationsFilterModelTest::givenFilterModel()
{
    givenLinearizationModel();
    filterModel.setSourceModel(&linearizationModel);
}

nx::Uuid OrganizationsFilterModelTest::givenTopLevelOrganization(const std::string& name)
{
    const auto id = nx::Uuid::createUuid();
    model.setOrganizations({.count = 1, .results = {{.id = id, .name = name}}}, /*parentId*/ {});
    return id;
}

void OrganizationsFilterModelTest::whenPartnersTabOpened()
{
    filterModel.setCurrentTab(OrganizationsModel::ChannelPartnersTab);
}

void OrganizationsFilterModelTest::whenOrganizationsTabOpened()
{
    filterModel.setCurrentTab(OrganizationsModel::OrganizationsTab);
}

void OrganizationsFilterModelTest::whenSitesTabOpened()
{
    filterModel.setCurrentTab(OrganizationsModel::SitesTab);
    filterModel.setCurrentRoot(model.sitesRoot());
}

void OrganizationsFilterModelTest::whenNodeOpened(nx::Uuid id)
{
    filterModel.setCurrentRoot(model.indexFromNodeId(id));
}

void OrganizationsFilterModelTest::whenSearching(const QString& text)
{
    filterModel.setFilterRegularExpression(text);
}

void OrganizationsFilterModelTest::whenShowOnly(const QList<OrganizationsModel::NodeType>& types)
{
    filterModel.setShowOnly(types);
}

void OrganizationsFilterModelTest::whenHideOrgSystemsFromSites(bool value)
{
    filterModel.setHideOrgSystemsFromSites(value);
}

void OrganizationsFilterModelTest::thenSearchResultsAre(const QStringList& tree)
{
    EXPECT_EQ(tree, dumpTree(filterModel));
}

void OrganizationsFilterModelTest::thenFilteredNamesAre(const QStringList& names)
{
    EXPECT_EQ(names, dumpNames(filterModel));
}

void OrganizationsFilterModelTest::thenFilteredIdsAre(const std::vector<nx::Uuid>& ids)
{
    std::vector<nx::Uuid> filteredIds;
    for (int row = 0; row < filterModel.rowCount(); ++row)
    {
        filteredIds.push_back(
            filterModel.index(row, 0).data(OrganizationsModel::IdRole).value<nx::Uuid>());
    }

    EXPECT_EQ(ids, filteredIds);
}

void OrganizationsFilterModelTest::thenFilteredRowCount(int count)
{
    EXPECT_EQ(count, filterModel.rowCount());
}

void OrganizationsFilterModelTest::thenFilteredCounts(
    int partnerCount, int organizationCount, int siteCount)
{
    thenFilteredPartnerCount(partnerCount);
    EXPECT_EQ(organizationCount, filterModel.organizationCount());
    EXPECT_EQ(siteCount, filterModel.siteCount());
}

void OrganizationsFilterModelTest::thenFilteredPartnerCount(int count)
{
    EXPECT_EQ(count, filterModel.partnerCount());
}

void OrganizationsFilterModelTest::thenSitesNodeIsFilteredOut()
{
    EXPECT_FALSE(dumpNames(filterModel).contains("Sites"));
}

void OrganizationsFilterModelTest::thenShowOnlyChangeCount(int count)
{
    EXPECT_EQ(count, showOnlyChangedSpy.count());
}

void OrganizationsFilterModelTest::thenHideOrgSystemsFromSitesChangeCount(int count)
{
    EXPECT_EQ(count, hideOrgSystemsFromSitesChangedSpy.count());
}

void OrganizationsFilterModelTest::thenCurrentTabChangeCount(int count)
{
    EXPECT_EQ(count, currentTabChangedSpy.count());
}

void OrganizationsFilterModelTest::thenCurrentRoot(nx::Uuid id)
{
    const auto index = model.indexFromNodeId(id);
    ASSERT_TRUE(index.isValid());
    EXPECT_EQ(index, filterModel.currentRoot());
}

void OrganizationsFilterModelTest::thenNoCurrentRoot()
{
    EXPECT_FALSE(filterModel.currentRoot().isValid());
}

void OrganizationsFilterModelTest::thenCurrentRootChangeCount(int count)
{
    EXPECT_EQ(count, currentRootChangedSpy.count());
}

// ------------------------------------------------------------------------------------------------
// OrganizationsFilterModel test

TEST_F(OrganizationsFilterModelTest, search)
{
    givenTestStructure();
    givenFilterModel();

    // Search for '2' through the whole tree.
    whenPartnersTabOpened();
    whenSearching("2");
    thenSearchResultsAre({
        "ChannelPartner2: Partners",
        "ChannelPartner22: Partners",
        "Org2: ChannelPartner1",
        "SubGroup2: ChannelPartner1 / Org1 / Group1",
        "System2: ChannelPartner1 / Org1 / Group1",
        "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
    });

    // Search for '2' inside 'ChannelPartner1'.
    whenNodeOpened(cp1Id);
    thenSearchResultsAre({
        "Org2: Organizations",
        "SubGroup2: ChannelPartner1 / Org1 / Group1",
        "System2: ChannelPartner1 / Org1 / Group1",
        "ChannelPartner2: Other results<br>Partners",
        "ChannelPartner22: Other results<br>Partners",
        "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
    });
}

TEST_F(OrganizationsFilterModelTest, searchGrouping)
{
    givenTestStructureWithSameNames();
    givenLocalSite("Local system 1");
    givenCloudSite("Cloud system a 1");
    givenFilterModel();

    // Search for 'a' through the whole tree, starting from 'Channel partners' tab.
    whenSearching("a");
    whenPartnersTabOpened();
    thenSearchResultsAre({
        "Partner 1: Partners",
        "Partner 2: Partners",
        "Organization 1.1: Partner 1",
        "Organization 1.2: Partner 1",
        "Organization 2.1: Partner 2",
        "Cloud system a 1: Other results<br>Cloud",
        "Local system 1: Local",
    });

    // The same search, but from 'Sites' tab.
    whenSitesTabOpened();
    thenSearchResultsAre({
        "Cloud system a 1: Cloud",
        "Local system 1: Local",
        "Partner 1: Other results<br>Partners",
        "Partner 2: Other results<br>Partners",
        "Organization 1.1: Partner 1",
        "Organization 1.2: Partner 1",
        "Organization 2.1: Partner 2",
    });
}

TEST_F(OrganizationsFilterModelTest, sitesSearch)
{
    givenTestStructure();
    givenLocalSite("Local system 1");
    givenCloudSite("Cloud system 1");
    givenFilterModel();

    // Search for 'o' through the whole tree, starting from 'Sites' tab.
    whenSearching("o");
    whenSitesTabOpened();
    thenSearchResultsAre({
        "Cloud system 1: Cloud",
        "Local system 1: Local",
        "Group1: Other results<br>ChannelPartner1 / Org1",
        "SubGroup1: ChannelPartner1 / Org1 / Group1",
        "SubGroup2: ChannelPartner1 / Org1 / Group1",
        "Group3: ChannelPartner22 / Org3",
        "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
    });
}

TEST_F(OrganizationsFilterModelTest, filterShowOnly)
{
    givenTestStructure();
    givenFilterModel();
    whenPartnersTabOpened();

    whenShowOnly({OrganizationsModel::ChannelPartner});
    thenShowOnlyChangeCount(1);
    thenFilteredNamesAre({"ChannelPartner1", "ChannelPartner2", "ChannelPartner22"});
    thenFilteredCounts(/*partnerCount*/ 3, /*organizationCount*/ 0, /*siteCount*/ 0);

    // Rows are counted by the tab they belong to.
    whenShowOnly({OrganizationsModel::Folder});
    thenShowOnlyChangeCount(2);
    thenFilteredNamesAre({"Group1", "SubGroup1", "SubGroup2", "Group3", "SubGroup3_2"});
    thenFilteredPartnerCount(5);

    // Same value does nothing.
    whenShowOnly({OrganizationsModel::Folder});
    thenShowOnlyChangeCount(2);

    whenShowOnly({OrganizationsModel::Organization, OrganizationsModel::System});
    thenFilteredNamesAre({"Org1", "Org2", "Org3", "System2", "System1"});

    // 'Sites' node is never shown.
    whenShowOnly({});
    thenSitesNodeIsFilteredOut();
    thenFilteredRowCount(13);
}

TEST_F(OrganizationsFilterModelTest, searchInsideOrganization)
{
    givenTestStructure();
    givenFilterModel();

    whenNodeOpened(org1Id);
    whenSearching("Group");

    // Direct children of the current root are in their own sections, deeper ones are in the
    // sections of their paths.
    thenSearchResultsAre({
        "Group1: Folders",
        "SubGroup1: ChannelPartner1 / Org1 / Group1",
        "SubGroup2: ChannelPartner1 / Org1 / Group1",
        "Group3: Other results<br>ChannelPartner22 / Org3",
        "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
    });
}

TEST_F(OrganizationsFilterModelTest, searchWithNoResultsInCurrentRoot)
{
    givenTestStructure();
    givenFilterModel();

    whenNodeOpened(org1Id);
    whenSearching("3");

    // The first section of other results also contains a placeholder for the empty results.
    thenSearchResultsAre({
        "Org3: Other results<br>ChannelPartner22<br>Try changing the search parameters",
        "Group3: ChannelPartner22 / Org3",
        "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
    });
}

TEST_F(OrganizationsFilterModelTest, searchFromOrganizationsTab)
{
    givenTestStructure();
    givenTopLevelOrganization("Org4");
    givenFilterModel();

    whenOrganizationsTabOpened();
    whenSearching("Org");

    thenSearchResultsAre({
        "Org4: Organizations",
        "Org1: Other results<br>ChannelPartner1",
        "Org2: Other results<br>ChannelPartner1",
        "Org3: ChannelPartner22",
    });
}

TEST_F(OrganizationsFilterModelTest, sameNamesOrderedById)
{
    givenTestStructure();
    givenFilterModel();

    // Two folders in the same section with the same name, with random ids.
    const auto twin1Id = nx::Uuid::createUuid();
    const auto twin2Id = nx::Uuid::createUuid();
    whenOrgStructureUpdated(org1Id, {
        org1Structure[0],
        {.id = twin1Id, .name = "Twin"},
        {.id = twin2Id, .name = "Twin"},
    });

    whenPartnersTabOpened();
    whenSearching("Twin");

    thenFilteredIdsAre({std::min(twin1Id, twin2Id), std::max(twin1Id, twin2Id)});
}

TEST_F(OrganizationsFilterModelTest, countsByTab)
{
    givenTestStructure();
    givenTopLevelOrganization("Org4");
    givenLocalSite();
    givenFilterModel();

    thenFilteredCounts(/*partnerCount*/ 13, /*organizationCount*/ 1, /*siteCount*/ 1);
}

TEST_F(OrganizationsFilterModelTest, setCurrentTab)
{
    givenFilterModel();

    whenOrganizationsTabOpened();
    thenCurrentTabChangeCount(1);

    // Same value does nothing.
    whenOrganizationsTabOpened();
    thenCurrentTabChangeCount(1);
}

TEST_F(OrganizationsFilterModelTest, setCurrentRoot)
{
    givenTestStructure();
    givenFilterModel();

    whenNodeOpened(org2Id);
    thenCurrentRoot(org2Id);
    thenCurrentRootChangeCount(1);

    // Same value does nothing.
    whenNodeOpened(org2Id);
    thenCurrentRootChangeCount(1);
}

TEST_F(OrganizationsFilterModelTest, currentRootMoved)
{
    givenTestStructure();
    givenFilterModel();
    whenNodeOpened(org2Id);

    // Remove 'Org1', so 'Org2' moves to its row.
    whenOrganizationsUpdated({.count = 1, .results = {orgsList.results[1]}}, cp1Id);

    thenCurrentRoot(org2Id);
    thenCurrentRootChangeCount(2);
}

TEST_F(OrganizationsFilterModelTest, currentRootRemoved)
{
    givenTestStructure();
    givenFilterModel();
    whenNodeOpened(org2Id);

    // Remove 'Org2'.
    whenOrganizationsUpdated({.count = 1, .results = {orgsList.results[0]}}, cp1Id);

    thenNoCurrentRoot();
    thenCurrentRootChangeCount(2);
}

// Sites of the 'Sites' node are hidden only if they are accessible through an organization, which
// is known only after organizations are loaded from the cloud.
TEST_F(OrganizationsFilterModelTest, hideOrgSystemsFromSitesKeepsOtherSites)
{
    givenTestStructure();
    givenLocalSite();
    givenFilterModel();

    whenHideOrgSystemsFromSites(true);
    thenHideOrgSystemsFromSitesChangeCount(1);
    thenFilteredCounts(/*partnerCount*/ 13, /*organizationCount*/ 0, /*siteCount*/ 1);

    // Same value does nothing.
    whenHideOrgSystemsFromSites(true);
    thenHideOrgSystemsFromSitesChangeCount(1);
}

} // namespace test
} // namespace nx::vms::client::core
