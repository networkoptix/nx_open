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

class OrganizationsModelTest: public testing::Test
{
public:
    virtual void SetUp() override
    {
        // OrganizationsModel is not fully compliant only regarding layout changes and moves in
        // the systems model, which don't happen in these tests.
        modelTester = std::make_unique<QAbstractItemModelTester>(
            &model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    }

    void buildTree();

public:
    std::unique_ptr<TestSystemsController> controller;
    std::unique_ptr<QnSystemsModel> systemsModel;

    OrganizationsModel model;
    std::unique_ptr<QAbstractItemModelTester> modelTester;

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

void OrganizationsModelTest::buildTree()
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
    };

    system2 = {
        .systemId = system2Id,
        .name = "System2",
        .state = "active",
        .groupId = group1Id,
    };

    model.setChannelPartners(partnerList);
    model.setOrganizations(orgsList, partnerList.results[0].id);

    org1Structure = {group1};
    model.setOrgStructure(orgsList.results[0].id, org1Structure);
    model.setOrgSystems(orgsList.results[0].id, {system1, system2});

    model.setOrganizations(orgs2, partnerList.results[2].id);
    model.setOrgStructure(orgs2.results[0].id, {group3});

    controller.reset(new TestSystemsController());
    systemsModel.reset(new QnSystemsModel(controller.get()));
    model.setSystemsModel(systemsModel.get());
}

TEST_F(OrganizationsModelTest, removal)
{
    buildTree();

    ASSERT_EQ(
        QStringList({
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
        }),
        dumpTree(model));

    // Remove systems from 'Org1'.
    model.setOrgSystems(org1Id, {});

    ASSERT_EQ(
        QStringList({
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
        }),
        dumpTree(model));

    GroupStructure group1Update = {
        .id = group1Id,
        .name = "Group1",
        .children = {
            {
                .id = subGroup2Id,
                .name = "SubGroup2",
                .parentId = group1Id,
            }
        }
    };

    // Remove 'SubGroup1' by updating 'Group1'.
    model.setOrgStructure(org1Id, {group1Update});

    ASSERT_EQ(
        QStringList({
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
        }),
        dumpTree(model));

    // Remove all channel partners.
    model.setChannelPartners({});
    ASSERT_EQ(
        QStringList({
            "Sites: ",
        }),
        dumpTree(model));
}

TEST_F(OrganizationsModelTest, search)
{
    buildTree();

    OrganizationsFilterModel filterModel;
    LinearizationListModel linearizationModel;
    linearizationModel.setSourceModel(&model);
    filterModel.setSourceModel(&linearizationModel);
    filterModel.setCurrentTab(OrganizationsModel::ChannelPartnersTab);

    // Search for '2' through the whole tree.
    linearizationModel.setAutoExpandAll(true);
    filterModel.setFilterRegularExpression("2");

    ASSERT_EQ(
        QStringList({
            "ChannelPartner2: Partners",
            "ChannelPartner22: Partners",
            "Org2: ChannelPartner1",
            "SubGroup2: ChannelPartner1 / Org1 / Group1",
            "System2: ChannelPartner1 / Org1 / Group1",
            "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
        }),
        dumpTree(filterModel));

    // Search for '2' inside 'ChannelPartner1'.
    filterModel.setCurrentRoot(model.indexFromNodeId(cp1Id));

    ASSERT_EQ(
        QStringList({
            "Org2: Organizations",
            "SubGroup2: ChannelPartner1 / Org1 / Group1",
            "System2: ChannelPartner1 / Org1 / Group1",
            "ChannelPartner2: Other results<br>Partners",
            "ChannelPartner22: Other results<br>Partners",
            "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
        }),
        dumpTree(filterModel));
}

TEST_F(OrganizationsModelTest, searchGrouping)
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
    };

    SystemInOrganization system2 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "Site A",
        .state = "active",
        .groupId = group3Id,
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
        .groupId = group4Id
    };

    model.setOrganizations(orgs2, partnerList.results[1].id);
    model.setOrgStructure(orgs2.results[0].id, {group4});
    model.setOrgSystems(orgs2.results[0].id, {system4});

    controller.reset(new TestSystemsController());
    systemsModel.reset(new QnSystemsModel(controller.get()));
    model.setSystemsModel(systemsModel.get());

    ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1")));

    ASSERT_TRUE(controller->discoverSystem(QnCloudSystemDescription::create(
        QnCloudSystem{
            .cloudId = nx::Uuid::createUuid().toSimpleString(),
            .localId = nx::Uuid::createUuid(),
            .name = "Cloud system a 1",
            .ownerAccountEmail = "",
            .ownerFullName = "",
            .authKey = "",
            .weight = 0.0,
            .lastLoginTimeUtcMs = 0,
            .online = true,
            .system2faEnabled = false,
            .version = "1.0.0",
            .organizationId = {},
        })));

    OrganizationsFilterModel filterModel;
    LinearizationListModel linearizationModel;
    linearizationModel.setSourceModel(&model);
    filterModel.setSourceModel(&linearizationModel);

    // Search for 'a' through the whole tree, starting from 'Channel partners' tab.
    linearizationModel.setAutoExpandAll(true);
    filterModel.setFilterRegularExpression("a");

    filterModel.setCurrentTab(OrganizationsModel::ChannelPartnersTab);

    ASSERT_EQ(
        QStringList({
            "Partner 1: Partners",
            "Partner 2: Partners",
            "Organization 1.1: Partner 1",
            "Organization 1.2: Partner 1",
            "Organization 2.1: Partner 2",
            "Cloud system a 1: Other results<br>Cloud",
            "Local system 1: Local",
        }),
        dumpTree(filterModel));

    // The same search, but from 'Sites' tab.

    filterModel.setCurrentTab(OrganizationsModel::SitesTab);
    filterModel.setCurrentRoot(model.sitesRoot());

    ASSERT_EQ(
        QStringList({
            "Cloud system a 1: Cloud",
            "Local system 1: Local",
            "Partner 1: Other results<br>Partners",
            "Partner 2: Other results<br>Partners",
            "Organization 1.1: Partner 1",
            "Organization 1.2: Partner 1",
            "Organization 2.1: Partner 2",
        }),
        dumpTree(filterModel));
}

TEST_F(OrganizationsModelTest, sitesSearch)
{
    buildTree();

    ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1")));

    ASSERT_TRUE(controller->discoverSystem(QnCloudSystemDescription::create(
        QnCloudSystem{
            .cloudId = nx::Uuid::createUuid().toSimpleString(),
            .localId = nx::Uuid::createUuid(),
            .name = "Cloud system 1",
            .ownerAccountEmail = "",
            .ownerFullName = "",
            .authKey = "",
            .weight = 0.0,
            .lastLoginTimeUtcMs = 0,
            .online = true,
            .system2faEnabled = false,
            .version = "1.0.0",
            .organizationId = {},
        })));

    OrganizationsFilterModel filterModel;
    LinearizationListModel linearizationModel;
    linearizationModel.setSourceModel(&model);
    filterModel.setSourceModel(&linearizationModel);

    // Search for 'o' through the whole tree, starting from 'Sites' tab.
    linearizationModel.setAutoExpandAll(true);
    filterModel.setFilterRegularExpression("o");
    filterModel.setCurrentTab(OrganizationsModel::SitesTab);
    filterModel.setCurrentRoot(model.sitesRoot());

    ASSERT_EQ(
        QStringList({
            "Cloud system 1: Cloud",
            "Local system 1: Local",
            "Group1: Other results<br>ChannelPartner1 / Org1",
            "SubGroup1: ChannelPartner1 / Org1 / Group1",
            "SubGroup2: ChannelPartner1 / Org1 / Group1",
            "Group3: ChannelPartner22 / Org3",
            "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
        }),
        dumpTree(filterModel));
}

TEST_F(OrganizationsModelTest, roles)
{
    buildTree();

    const auto cp1 = model.indexFromNodeId(cp1Id);
    ASSERT_EQ(OrganizationsModel::ChannelPartner,
        cp1.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>());
    ASSERT_EQ(OrganizationsModel::ChannelPartnersTab,
        cp1.data(OrganizationsModel::TabSectionRole).value<OrganizationsModel::TabSection>());
    ASSERT_EQ("Partners", cp1.data(OrganizationsModel::PathFromRootRole).toString());

    // System count is updated from the actual list of organization systems.
    const auto org1 = model.indexFromNodeId(org1Id);
    ASSERT_EQ(2, org1.data(OrganizationsModel::SystemCountRole).toInt());
    ASSERT_EQ(cp1, org1.parent());

    const auto system1 = model.indexFromNodeId(system1Id);
    ASSERT_EQ(OrganizationsModel::System,
        system1.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>());
    ASSERT_EQ(system1Id, system1.data(OrganizationsModel::IdRole).value<nx::Uuid>());
    ASSERT_EQ(system1Id.toSimpleString(), system1.data(QnSystemsModel::SystemIdRoleId).toString());
    ASSERT_EQ(org1Id, system1.data(QnSystemsModel::OrganizationIdRole).value<nx::Uuid>());
    ASSERT_EQ("ChannelPartner1 / Org1 / Group1 / SubGroup1",
        system1.data(OrganizationsModel::PathFromRootRole).toString());
    ASSERT_EQ(OrganizationsModel::ChannelPartnersTab,
        system1.data(OrganizationsModel::TabSectionRole).value<OrganizationsModel::TabSection>());
    ASSERT_FALSE(system1.data(OrganizationsModel::IsFromSitesRole).toBool());

    const auto sitesRoot = model.sitesRoot();
    ASSERT_EQ(OrganizationsModel::SitesNode,
        sitesRoot.data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>());
    ASSERT_FALSE(model.hasChildren(sitesRoot));

    ASSERT_TRUE(model.hasChildren(model.indexFromNodeId(group1Id)));
    ASSERT_FALSE(model.hasChildren(model.indexFromNodeId(subGroup2Id)));

    ASSERT_TRUE(model.hasFolders(org1));
    ASSERT_TRUE(model.hasFolders(model.indexFromNodeId(group1Id)));
    ASSERT_FALSE(model.hasFolders(model.indexFromNodeId(subGroup1Id)));
    ASSERT_FALSE(model.hasFolders(model.indexFromNodeId(org2Id)));

    ASSERT_EQ(QStringList({system1Id.toSimpleString(), system2Id.toSimpleString()}),
        model.childSystemIds(org1));
    ASSERT_EQ(QStringList({system1Id.toSimpleString()}),
        model.childSystemIds(model.indexFromNodeId(subGroup1Id)));
    ASSERT_TRUE(model.childSystemIds(model.indexFromNodeId(subGroup2Id)).empty());

    ASSERT_FALSE(model.indexFromNodeId(nx::Uuid::createUuid()).isValid());
}

TEST_F(OrganizationsModelTest, hasChannelPartnersAndOrganizations)
{
    QSignalSpy hasChannelPartnersSpy(&model, &OrganizationsModel::hasChannelPartnersChanged);
    QSignalSpy hasOrganizationsSpy(&model, &OrganizationsModel::hasOrganizationsChanged);

    ASSERT_FALSE(model.hasChannelPartners());
    ASSERT_FALSE(model.hasOrganizations());

    ChannelPartnerList partners = {
        .count = 1,
        .results = {{.id = cp1Id, .name = "Partner"}},
    };

    model.setChannelPartners(partners);
    ASSERT_TRUE(model.hasChannelPartners());
    ASSERT_EQ(1, hasChannelPartnersSpy.count());

    // Organizations directly under the root are not related to channel partners.
    OrganizationList orgs = {
        .count = 1,
        .results = {{.id = org1Id, .name = "Organization"}},
    };

    model.setOrganizations(orgs, /*parentId*/ {});
    ASSERT_TRUE(model.hasOrganizations());
    ASSERT_EQ(1, hasOrganizationsSpy.count());

    ASSERT_EQ(
        QStringList({
            "Sites: ",
            "Partner: Partners",
            "Organization: Organizations",
        }),
        dumpTree(model));

    ASSERT_EQ(OrganizationsModel::OrganizationsTab,
        model.indexFromNodeId(org1Id).data(OrganizationsModel::TabSectionRole)
            .value<OrganizationsModel::TabSection>());

    // Organizations under a channel partner do not affect `hasOrganizations`.
    model.setOrganizations({}, /*parentId*/ {});
    model.setOrganizations(orgs, cp1Id);
    ASSERT_FALSE(model.hasOrganizations());
    ASSERT_EQ(2, hasOrganizationsSpy.count());

    // Same data does not emit signals.
    model.setChannelPartners(partners);
    ASSERT_EQ(1, hasChannelPartnersSpy.count());

    model.setChannelPartners({});
    ASSERT_FALSE(model.hasChannelPartners());
    ASSERT_EQ(2, hasChannelPartnersSpy.count());

    ASSERT_EQ(QStringList({"Sites: "}), dumpTree(model));
}

TEST_F(OrganizationsModelTest, channelPartnerRenamedAndAdded)
{
    buildTree();

    partnerList.results[0].name = "Partner1";
    partnerList.results.push_back({.id = nx::Uuid::createUuid(), .name = "ChannelPartner3"});

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);

    model.setChannelPartners(partnerList);

    ASSERT_EQ(1, dataChangedSpy.count());
    ASSERT_EQ(model.indexFromNodeId(cp1Id), dataChangedSpy[0][0].toModelIndex());
    ASSERT_EQ(1, rowsInsertedSpy.count());
    ASSERT_FALSE(rowsInsertedSpy[0][0].toModelIndex().isValid());
    ASSERT_EQ(4, rowsInsertedSpy[0][1].toInt());

    ASSERT_EQ(
        QStringList({
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
        }),
        dumpTree(model));
}

TEST_F(OrganizationsModelTest, folderMovedToNewFolder)
{
    buildTree();

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

    QSignalSpy foldersChangedSpy(&model, &OrganizationsModel::foldersChanged);

    model.setOrgStructure(org1Id, {group1, group2});

    ASSERT_FALSE(foldersChangedSpy.empty());
    ASSERT_EQ(model.indexFromNodeId(org1Id), foldersChangedSpy.last()[0].toModelIndex());

    ASSERT_EQ(
        QStringList({
            "Group1",
            "Group2",
        }),
        dumpNames(model, model.indexFromNodeId(org1Id)));
    ASSERT_EQ(
        QStringList({
            "SubGroup1",
            "System2",
        }),
        dumpNames(model, model.indexFromNodeId(group1Id)));
    ASSERT_EQ(
        QStringList({
            "SubGroup2",
        }),
        dumpNames(model, model.indexFromNodeId(group2Id)));
    ASSERT_EQ("ChannelPartner1 / Org1 / Group2",
        model.indexFromNodeId(subGroup2Id).data(OrganizationsModel::PathFromRootRole).toString());
}

TEST_F(OrganizationsModelTest, sameStructureCausesNoChanges)
{
    buildTree();

    GroupStructure group1 = {
        .id = group1Id,
        .name = "Group1",
        .children = {
            {.id = subGroup1Id, .name = "SubGroup1", .parentId = group1Id},
            {.id = subGroup2Id, .name = "SubGroup2", .parentId = group1Id},
        },
    };

    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    model.setOrgStructure(org1Id, {group1});

    ASSERT_TRUE(rowsInsertedSpy.empty());
    ASSERT_TRUE(rowsRemovedSpy.empty());
    ASSERT_TRUE(dataChangedSpy.empty());
}

TEST_F(OrganizationsModelTest, systemRenamed)
{
    buildTree();

    system2.name = "System2a";
    model.setOrgSystems(org1Id, {system1, system2});

    // A renamed system keeps its position.
    ASSERT_EQ(
        QStringList({
            "SubGroup1",
            "SubGroup2",
            "System2a",
        }),
        dumpNames(model, model.indexFromNodeId(group1Id)));
}

// Organizations are loaded concurrently, so when a system is moved to another organization, the
// new organization can be updated before or after the previous one.

TEST_F(OrganizationsModelTest, systemMovedToAnotherOrganizationUpdatedLast)
{
    buildTree();

    // Move 'System1' from 'Org1' to 'Org2'.
    auto movedSystem1 = system1;
    movedSystem1.groupId = {};

    model.setOrgSystems(org1Id, {system2});
    model.setOrgSystems(org2Id, {movedSystem1});

    ASSERT_EQ(model.indexFromNodeId(org2Id), model.indexFromNodeId(system1Id).parent());
    ASSERT_EQ(1, model.indexFromNodeId(org1Id).data(OrganizationsModel::SystemCountRole).toInt());
    ASSERT_EQ(1, model.indexFromNodeId(org2Id).data(OrganizationsModel::SystemCountRole).toInt());
}

TEST_F(OrganizationsModelTest, systemRemoved)
{
    buildTree();

    model.setOrgSystems(org1Id, {system2});

    ASSERT_FALSE(model.indexFromNodeId(system1Id).isValid());
    ASSERT_TRUE(model.childSystemIds(model.indexFromNodeId(subGroup1Id)).empty());
    ASSERT_EQ(1, model.indexFromNodeId(org1Id).data(OrganizationsModel::SystemCountRole).toInt());
}

TEST_F(OrganizationsModelTest, organizationRemoved)
{
    buildTree();

    // Remove 'Org2'.
    orgsList.results.pop_back();
    model.setOrganizations(orgsList, cp1Id);

    ASSERT_FALSE(model.indexFromNodeId(org2Id).isValid());
    ASSERT_EQ(QStringList({"Org1"}), dumpNames(model, model.indexFromNodeId(cp1Id)));
}

TEST_F(OrganizationsModelTest, organizationRemovedAndAddedBack)
{
    buildTree();

    // Remove 'Org1'.
    model.setOrganizations({.count = 1, .results = {orgsList.results[1]}}, cp1Id);
    ASSERT_FALSE(model.indexFromNodeId(org1Id).isValid());

    // Add it back with the same content, in the same order as in loading.
    model.setOrganizations(orgsList, cp1Id);
    model.setOrgStructure(org1Id, org1Structure);
    model.setOrgSystems(org1Id, {system1, system2});

    ASSERT_EQ(
        QStringList({
            "Group1",
            "SubGroup1",
            "System1",
            "SubGroup2",
            "System2",
        }),
        dumpNamesRecursively(model, model.indexFromNodeId(org1Id)));
}

TEST_F(OrganizationsModelTest, channelPartnerRemovedAndAddedBack)
{
    buildTree();

    // Remove 'ChannelPartner1' along with its organizations.
    auto partners = partnerList;
    partners.results.erase(partners.results.begin());
    model.setChannelPartners(partners);
    ASSERT_FALSE(model.indexFromNodeId(org1Id).isValid());

    // Add it back with the same content, in the same order as in loading.
    model.setChannelPartners(partnerList);
    model.setOrganizations(orgsList, cp1Id);
    model.setOrgStructure(org1Id, org1Structure);
    model.setOrgSystems(org1Id, {system1, system2});

    ASSERT_EQ(
        QStringList({
            "Group1",
            "SubGroup1",
            "System1",
            "SubGroup2",
            "System2",
        }),
        dumpNamesRecursively(model, model.indexFromNodeId(org1Id)));
}

TEST_F(OrganizationsModelTest, sitesSystemDiscoveredAndLost)
{
    buildTree();

    const auto localSystemId = nx::Uuid::createUuid().toSimpleString();
    ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        localSystemId,
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1")));

    ASSERT_EQ(QStringList({"Local system 1"}), dumpNames(model, model.sitesRoot()));
    const auto localSystem = model.indexFromSystemId(localSystemId);
    ASSERT_EQ(model.sitesRoot(), localSystem.parent());
    ASSERT_TRUE(localSystem.data(OrganizationsModel::IsFromSitesRole).toBool());
    ASSERT_EQ("Local", localSystem.data(OrganizationsModel::SectionRole).toString());
    ASSERT_EQ(OrganizationsModel::SitesTab,
        localSystem.data(OrganizationsModel::TabSectionRole)
            .value<OrganizationsModel::TabSection>());

    ASSERT_TRUE(controller->loseSystem(localSystemId));
    ASSERT_EQ(0, model.rowCount(model.sitesRoot()));
}

TEST_F(OrganizationsModelTest, systemsModelRemoved)
{
    buildTree();

    ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1")));
    ASSERT_EQ(1, model.rowCount(model.sitesRoot()));

    model.setSystemsModel(nullptr);
    ASSERT_EQ(0, model.rowCount(model.sitesRoot()));
}

TEST_F(OrganizationsModelTest, folderWithSystemsMoved)
{
    buildTree();

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
    model.setOrgStructure(org1Id, {group1, group2});

    ASSERT_EQ(model.indexFromNodeId(subGroup1Id), model.indexFromNodeId(system1Id).parent());
    ASSERT_EQ(model.indexFromNodeId(group2Id), model.indexFromNodeId(subGroup1Id).parent());
    ASSERT_EQ(2, model.indexFromNodeId(org1Id).data(OrganizationsModel::SystemCountRole).toInt());

    // Systems reference their folders by id, so the system list doesn't change.
    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.setOrgSystems(org1Id, {system1, system2});
    ASSERT_TRUE(rowsInsertedSpy.empty());
    ASSERT_TRUE(rowsRemovedSpy.empty());
}

TEST_F(OrganizationsModelTest, folderWithSystemsMovedIntoExistingFolder)
{
    buildTree();

    // Move 'SubGroup1', which contains 'System1', from 'Group1' into existing 'SubGroup2'.
    GroupStructure group1 = {
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
    };

    model.setOrgStructure(org1Id, {group1});

    ASSERT_EQ(model.indexFromNodeId(subGroup1Id), model.indexFromNodeId(system1Id).parent());
    ASSERT_EQ(model.indexFromNodeId(subGroup2Id), model.indexFromNodeId(subGroup1Id).parent());
    ASSERT_EQ(2, model.indexFromNodeId(org1Id).data(OrganizationsModel::SystemCountRole).toInt());
}

TEST_F(OrganizationsModelTest, structureClearedAndRestored)
{
    buildTree();

    // Clearing the structure removes the folders along with their systems. Systems of
    // non-existent folders are not shown. The order of calls is the same as in loading.
    model.setOrgStructure(org1Id, {});
    model.setOrgSystems(org1Id, {system1, system2});
    ASSERT_EQ(0, model.rowCount(model.indexFromNodeId(org1Id)));

    // Systems are restored along with their folders.
    model.setOrgStructure(org1Id, org1Structure);

    ASSERT_EQ(
        QStringList({
            "Group1",
            "SubGroup1",
            "System1",
            "SubGroup2",
            "System2",
        }),
        dumpNamesRecursively(model, model.indexFromNodeId(org1Id)));
}

TEST_F(OrganizationsModelTest, folderMovedIntoSiblingFolder)
{
    buildTree();

    // Move 'SubGroup2' from 'Group1' into 'SubGroup1'.
    GroupStructure group1 = {
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
    };

    model.setOrgStructure(org1Id, {group1});

    ASSERT_TRUE(model.indexFromNodeId(subGroup2Id).isValid());
    ASSERT_EQ(model.indexFromNodeId(subGroup1Id), model.indexFromNodeId(subGroup2Id).parent());
    ASSERT_EQ(
        QStringList({
            "SubGroup1",
            "System2",
        }),
        dumpNames(model, model.indexFromNodeId(group1Id)));
}

TEST_F(OrganizationsModelTest, rowsInsertedUnderModelIndexes)
{
    QStringList invalidParents;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model,
        [&invalidParents](const QModelIndex& parent)
        {
            if (parent.isValid() && !isModelIndex(parent))
                invalidParents << parent.data().toString();
        });

    // Contains folders with subfolders.
    buildTree();

    ASSERT_TRUE(invalidParents.empty())
        << "Rows inserted under: " << invalidParents.join(", ").toStdString();
}

TEST_F(OrganizationsModelTest, linearizationOfNewFolderWithTwoSubfolders)
{
    buildTree();

    // 'Sites' node has children.
    ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1")));

    LinearizationListModel linearizationModel;
    linearizationModel.setSourceModel(&model);
    linearizationModel.setAutoExpandAll(true);

    // Add a new folder with two subfolders to 'Org1'.
    const auto group2Id = nx::Uuid::createUuid();
    GroupStructure group2 = {
        .id = group2Id,
        .name = "Group2",
        .children = {
            {.id = nx::Uuid::createUuid(), .name = "SubGroup2_1", .parentId = group2Id},
            {.id = nx::Uuid::createUuid(), .name = "SubGroup2_2", .parentId = group2Id},
        },
    };

    model.setOrgStructure(org1Id, {org1Structure[0], group2});

    ASSERT_EQ(dumpNamesRecursively(model), dumpNames(linearizationModel));
}

TEST_F(OrganizationsModelTest, structureWithDuplicateIds)
{
    buildTree();

    QStringList invalidParents;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model,
        [&invalidParents](const QModelIndex& parent)
        {
            if (parent.isValid() && !isModelIndex(parent))
                invalidParents << parent.data().toString();
        });

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

    model.setOrgStructure(org1Id, {org1Structure[0], group2});

    ASSERT_TRUE(invalidParents.empty())
        << "Rows removed under: " << invalidParents.join(", ").toStdString();
    ASSERT_EQ(1, model.rowCount(model.indexFromNodeId(group2Id)));
}

TEST_F(OrganizationsModelTest, structureWithRecursiveIds)
{
    buildTree();

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

    model.setOrgStructure(org1Id, {org1Structure[0], group2});

    // Nodes outside of the organization are not affected: removing the recursive group could
    // remove the 'Sites' node instead.
    ASSERT_EQ(OrganizationsModel::SitesNode,
        model.index(0, 0).data(OrganizationsModel::TypeRole).value<OrganizationsModel::NodeType>());

    // The recursive group is skipped, the rest of the structure is intact.
    ASSERT_EQ(model.indexFromNodeId(group2Id), model.indexFromNodeId(group3Id).parent());
    ASSERT_EQ(
        QStringList({
            "Group1",
            "SubGroup1",
            "System1",
            "SubGroup2",
            "System2",
            "Group2",
            "Group3",
        }),
        dumpNamesRecursively(model, model.indexFromNodeId(org1Id)));
}

TEST_F(OrganizationsModelTest, sortModel)
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
        {.systemId = system1Id, .name = "Site B", .state = "active"},
        {.systemId = system2Id, .name = "Site A", .state = "active"},
    });

    OrganizationsSortModel sortModel;
    sortModel.setSourceModel(&model);

    // Organizations keep their original order, folders and systems are sorted by name.
    ASSERT_EQ(
        QStringList({
            "Sites: ",
            "Org B: Organizations",
            "  folder 2: Folders",
            "  Folder 10: Folders",
            "  Site A: Sites",
            "  Site B: Sites",
            "Org A: Organizations",
        }),
        dumpTree(sortModel));
}

TEST_F(OrganizationsModelTest, filterShowOnly)
{
    buildTree();

    OrganizationsFilterModel filterModel;
    LinearizationListModel linearizationModel;
    linearizationModel.setSourceModel(&model);
    filterModel.setSourceModel(&linearizationModel);
    filterModel.setCurrentTab(OrganizationsModel::ChannelPartnersTab);
    linearizationModel.setAutoExpandAll(true);

    QSignalSpy showOnlyChangedSpy(&filterModel, &OrganizationsFilterModel::showOnlyChanged);

    filterModel.setShowOnly({OrganizationsModel::ChannelPartner});
    ASSERT_EQ(1, showOnlyChangedSpy.count());
    ASSERT_EQ(
        QStringList({
            "ChannelPartner1",
            "ChannelPartner2",
            "ChannelPartner22",
        }),
        dumpNames(filterModel));
    ASSERT_EQ(3, filterModel.partnerCount());
    ASSERT_EQ(0, filterModel.organizationCount());
    ASSERT_EQ(0, filterModel.siteCount());

    // Rows are counted by the tab they belong to.
    filterModel.setShowOnly({OrganizationsModel::Folder});
    ASSERT_EQ(2, showOnlyChangedSpy.count());
    ASSERT_EQ(
        QStringList({
            "Group1",
            "SubGroup1",
            "SubGroup2",
            "Group3",
            "SubGroup3_2",
        }),
        dumpNames(filterModel));
    ASSERT_EQ(5, filterModel.partnerCount());

    // Same value does nothing.
    filterModel.setShowOnly({OrganizationsModel::Folder});
    ASSERT_EQ(2, showOnlyChangedSpy.count());

    filterModel.setShowOnly({OrganizationsModel::Organization, OrganizationsModel::System});
    ASSERT_EQ(
        QStringList({
            "Org1",
            "Org2",
            "Org3",
            "System2",
            "System1",
        }),
        dumpNames(filterModel));

    // 'Sites' node is never shown.
    filterModel.setShowOnly({});
    ASSERT_FALSE(dumpNames(filterModel).contains("Sites"));
    ASSERT_EQ(13, filterModel.rowCount());
}

} // namespace test
} // namespace nx::vms::client::core
