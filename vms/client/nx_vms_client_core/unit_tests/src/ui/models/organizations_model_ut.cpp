// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/common/models/linearization_list_model.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <ui/models/organizations_model.h>

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

} // namespace

class TestOrganizationsModel: public testing::Test
{
public:
    void buildTree();

public:
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

    // Search for '2' through the whole tree.
    linearizationModel.setAutoExpandAll(true);
    filterModel.setFilterRegularExpression("2");

    ASSERT_EQ(
        QStringList({
            "ChannelPartner2: Partners",
            "ChannelPartner22: Partners",
            "Org2: Organizations",
            "SubGroup2: Folders",
            "SubGroup3_2: Folders",
            "System2: Sites",
        }),
        dumpTree(filterModel));

    // Search for '2' inside 'ChannelPartner1'.
    filterModel.setCurrentRoot(model.indexFromNodeId(cp1Id));

    ASSERT_EQ(
        QStringList({
            "Org2: Organizations",
            "SubGroup2: Folders",
            "System2: Sites",
            "ChannelPartner2: Other results<br>Partners",
            "ChannelPartner22: Other results<br>Partners",
            "SubGroup3_2: ChannelPartner22 / Org3 / Group3",
        }),
        dumpTree(filterModel));
}

} // namespace test
} // namespace nx::vms::client::core
