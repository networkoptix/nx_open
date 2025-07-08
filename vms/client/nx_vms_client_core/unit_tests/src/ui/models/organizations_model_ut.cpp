// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/common/models/linearization_list_model.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <nx/vms/client/core/system_finder/private/local_system_description.h>
#include <ui/models/organizations_model.h>
#include <ui/models/systems_model.h>

#include "test_systems_controller.h"

namespace nx::vms::client::core {
namespace test {

namespace {

// Helper function to recursively dump the tree structure of a model into a QStringList.
// Each line in the list represents a node in the tree, with indentation based on the level of
// the node in the hierarchy. Section information is appended after ": " and newlines are replaced
// with "<br>" for better readability in the outout.
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

} // namespace

class TestOrganizationsModel: public testing::Test
{
public:
    void buildTree();

public:
    std::unique_ptr<TestSystemsController> controller;
    std::unique_ptr<QnSystemsModel> systemsModel;

    OrganizationsModel model;

    nx::Uuid cp1Id = nx::Uuid::createUuid();
    nx::Uuid org1Id = nx::Uuid::createUuid();
    nx::Uuid group1Id = nx::Uuid::createUuid();
    nx::Uuid subGroup2Id = nx::Uuid::createUuid();
};

void TestOrganizationsModel::buildTree()
{
    ChannelPartnerList partnerList = {
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

    OrganizationList orgs1 = {
        .count = 2,
        .results = {
            {
                .id = org1Id,
                .name = "Org1",
                .systemCount = 1,
                .channelPartner = partnerList.results[0].id,
            },
            {
                .id = nx::Uuid::createUuid(),
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
                .id = nx::Uuid::createUuid(),
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

    SystemInOrganization system1 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "System1",
        .state = "active",
        .groupId = group1.children[0].id,
    };

    SystemInOrganization system2 = {
        .systemId = nx::Uuid::createUuid(),
        .name = "System2",
        .state = "active",
        .groupId = group1Id,
    };

    model.setChannelPartners(partnerList);
    model.setOrganizations(orgs1, partnerList.results[0].id);

    model.setOrgStructure(orgs1.results[0].id, {group1});
    model.setOrgSystems(orgs1.results[0].id, {system1, system2});

    model.setOrganizations(orgs2, partnerList.results[2].id);
    model.setOrgStructure(orgs2.results[0].id, {group3});

    controller.reset(new TestSystemsController());
    systemsModel.reset(new QnSystemsModel(controller.get()));
    model.setSystemsModel(systemsModel.get());
}

TEST_F(TestOrganizationsModel, removal)
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

TEST_F(TestOrganizationsModel, search)
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

TEST_F(TestOrganizationsModel, searchGrouping)
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

    controller->emitSystemDiscovered(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1"));

    controller->emitSystemDiscovered(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ nx::Uuid::createUuid().toSimpleString(),
        "Cloud system a 1"));

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
            "Cloud system a 1: Other results<br>Sites",
            "Local system 1: Other results<br>Sites",
        }),
        dumpTree(filterModel));

    // The same search, but from 'Sites' tab.

    filterModel.setCurrentTab(OrganizationsModel::SitesTab);
    filterModel.setCurrentRoot(model.sitesRoot());

    ASSERT_EQ(
        QStringList({
            "Cloud system a 1: Sites",
            "Local system 1: Sites",
            "Partner 1: Other results<br>Partners",
            "Partner 2: Other results<br>Partners",
            "Organization 1.1: Partner 1",
            "Organization 1.2: Partner 1",
            "Organization 2.1: Partner 2",
        }),
        dumpTree(filterModel));
}

TEST_F(TestOrganizationsModel, sitesSearch)
{
    buildTree();

    controller->emitSystemDiscovered(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "Local system 1"));

    controller->emitSystemDiscovered(LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ nx::Uuid::createUuid().toSimpleString(),
        "Cloud system 1"));

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
            "Cloud system 1: Sites",
            "Local system 1: Sites",
            "Group1: Other results<br>ChannelPartner1 / Org1",
            "SubGroup1: ChannelPartner1 / Org1 / Group1",
            "SubGroup2: ChannelPartner1 / Org1 / Group1",
            "Group3: ChannelPartner22 / Org3",
            "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
        }),
        dumpTree(filterModel));
}

} // namespace test
} // namespace nx::vms::client::core
