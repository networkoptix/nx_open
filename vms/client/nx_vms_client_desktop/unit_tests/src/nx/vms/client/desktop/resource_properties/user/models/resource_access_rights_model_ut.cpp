// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtCore/QRegularExpression>
#include <QtCore/QVector>

#include <nx/utils/qt_helpers.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_properties/user/models/resource_access_rights_model.h>

#include "../utils/access_subject_editing_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::core::access;
using namespace nx::vms::api;

using ResourceTree::NodeType;

using nx::utils::toQSet;

class ResourceAccessRightsModelTest: public AccessSubjectEditingFixture
{
public:
    virtual void SetUp()
    {
        AccessSubjectEditingFixture::SetUp();

        customGroupId = createUserGroup("Custom Group", kViewersGroupId).id;
        testUser = addUser({kLiveViewersGroupId, customGroupId}, "Test User");

        editingContext->setCurrentSubject(testUser->getId(),
            AccessSubjectEditingContext::SubjectType::user);

        accessRightsModel.reset(new ResourceAccessRightsModel());
        accessRightsModel->setContext(editingContext.get());
        accessRightsModel->setAccessRightsList(accessRightsList);

        ASSERT_EQ(accessRightsModel->info().size(), accessRightsList.size());

        const auto layout1 = resource("Layout1").objectCast<LayoutResource>();
        ASSERT_TRUE((bool) layout1);
        addToLayout(layout1, resource("Camera1"));
        addToLayout(layout1, resource("Camera2"));
        addToLayout(layout1, resource("Camera3"));
        layout1->storeSnapshot();

        const auto layout2 = resource("Layout2").objectCast<LayoutResource>();
        ASSERT_TRUE((bool) layout2);
        addToLayout(layout2, resource("Camera1"));
        addToLayout(layout2, resource("Camera2"));
        layout2->storeSnapshot();

        const auto videoWallLayout1 = addLayoutForVideoWall(
            resource("VideoWall1").objectCast<QnVideoWallResource>()).objectCast<LayoutResource>();
        ASSERT_TRUE((bool) videoWallLayout1);
        addToLayout(videoWallLayout1, resource("Camera1"));
        addToLayout(videoWallLayout1, resource("Camera4"));

        videoWallLayout1->storeSnapshot();
    }

    virtual void TearDown() override
    {
        accessRightsModel.reset();
        testUser.reset();
        AccessSubjectEditingFixture::TearDown();
    }

    int indexOf(AccessRight accessRight) const { return accessRightsList.indexOf(accessRight); }
    using AccessSubjectEditingFixture::indexOf;

    // In a single tooltip line, access providers are tagged with <b> </b>.
    // Use it to extract the providers.
    QStringList splitTooltipLine(const QString& source) const
    {
        QStringList result;
        static const QRegularExpression matcher{"<b>([^<]*)</b>"};

        for (const auto match: matcher.globalMatch(source))
            result.push_back(match.captured(1));

        return result;
    }

protected:
    QnUuid customGroupId;
    QnUserResourcePtr testUser;

    std::unique_ptr<ResourceAccessRightsModel> accessRightsModel;

    const QVector<AccessRight> accessRightsList{{
        AccessRight::view,
        AccessRight::viewArchive,
        AccessRight::exportArchive,
        AccessRight::viewBookmarks,
        AccessRight::manageBookmarks,
        AccessRight::userInput,
        AccessRight::edit}};
};

TEST_F(ResourceAccessRightsModelTest, ownPermissions)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));
    editingContext->setOwnResourceAccessMap({
        {resource("Camera1")->getId(), AccessRight::view | AccessRight::userInput}});

    auto info = accessRightsModel->info()[indexOf(AccessRight::userInput)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::own);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::own);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());
}

TEST_F(ResourceAccessRightsModelTest, inheritanceFromOwnResourceGroup)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));
    editingContext->setOwnResourceAccessMap({
        {kAllDevicesGroupId, AccessRight::view | AccessRight::userInput}});

    auto info = accessRightsModel->info()[indexOf(AccessRight::userInput)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::ownResourceGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.parentResourceGroupId, kAllDevicesGroupId);
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::ownResourceGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.parentResourceGroupId, kAllDevicesGroupId);
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());
}

TEST_F(ResourceAccessRightsModelTest, inheritanceFromLayout)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));
    editingContext->setOwnResourceAccessMap({
        {resource("Layout1")->getId(), AccessRight::view | AccessRight::userInput}});

    auto info = accessRightsModel->info()[indexOf(AccessRight::userInput)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_EQ(info.indirectProviders, QVector<QnResourcePtr>({resource("Layout1")}));

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_EQ(info.indirectProviders, QVector<QnResourcePtr>({resource("Layout1")}));
}

TEST_F(ResourceAccessRightsModelTest, inheritanceFromVideoWall)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));
    editingContext->setOwnResourceAccessMap({
        {resource("VideoWall1")->getId(), AccessRight::edit}});

    const auto info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::videowall);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::videowall);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_EQ(info.indirectProviders, QVector<QnResourcePtr>({resource("VideoWall1")}));
}

TEST_F(ResourceAccessRightsModelTest, inheritanceFromGroups)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));

    auto info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());

    info = accessRightsModel->info()[indexOf(AccessRight::viewArchive)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());

    info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
}

TEST_F(ResourceAccessRightsModelTest, multipleInheritance)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));
    EXPECT_EQ(accessRightsModel->rowType(), ResourceAccessTreeItem::Type::resource);

    editingContext->setOwnResourceAccessMap({
        {kAllDevicesGroupId, AccessRight::view | AccessRight::viewArchive},
        {resource("Camera1")->getId(), AccessRight::view},
        {resource("Layout1")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput},
        {resource("VideoWall1")->getId(), AccessRight::edit}});

    auto info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::own);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_EQ(toQSet(info.indirectProviders),
        QSet<QnResourcePtr>({resource("Layout1"), resource("VideoWall1")}));

    info = accessRightsModel->info()[indexOf(AccessRight::viewArchive)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::ownResourceGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_EQ(info.parentResourceGroupId, kAllDevicesGroupId);
    EXPECT_EQ(info.providerUserGroups, QVector<QnUuid>({customGroupId}));
    EXPECT_EQ(info.indirectProviders, QVector<QnResourcePtr>({resource("Layout1")}));

    info = accessRightsModel->info()[indexOf(AccessRight::exportArchive)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(info.providerUserGroups, QVector<QnUuid>({customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());

    info = accessRightsModel->info()[indexOf(AccessRight::userInput)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::layout);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_EQ(info.indirectProviders, QVector<QnResourcePtr>({resource("Layout1")}));

    info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
}

TEST_F(ResourceAccessRightsModelTest, specialResourceGroup)
{
    accessRightsModel->setResourceTreeIndex(indexOf(NodeType::camerasAndDevices));
    EXPECT_EQ(accessRightsModel->rowType(), ResourceAccessTreeItem::Type::specialResourceGroup);

    auto info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 0);

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 5);

    editingContext->setOwnResourceAccessMap({{kAllDevicesGroupId,
        AccessRight::view | AccessRight::userInput | AccessRight::edit}});

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::own);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 0); //< Children aren't explicitly checked.
    EXPECT_EQ(info.checkedAndInheritedChildCount, 5);

    info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::own);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 0); //< Children aren't explicitly checked.
    EXPECT_EQ(info.checkedAndInheritedChildCount, 5);

    editingContext->setOwnResourceAccessMap({{resource("Layout1")->getId(),
        AccessRight::view | AccessRight::userInput | AccessRight::edit}});

    info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 3);

    editingContext->setOwnResourceAccessMap({{resource("Camera1")->getId(), AccessRight::view}});

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::parentUserGroup);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_EQ(toQSet(info.providerUserGroups), QnUuidSet({kLiveViewersGroupId, customGroupId}));
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 5);
    EXPECT_EQ(info.checkedChildCount, 1);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 5);
}

TEST_F(ResourceAccessRightsModelTest, simpleGroupingNode)
{
    accessRightsModel->setResourceTreeIndex(indexOf("Group1"));
    EXPECT_EQ(accessRightsModel->rowType(), ResourceAccessTreeItem::Type::groupingNode);

    auto info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 2);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 0);

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 2);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 2);

    editingContext->setOwnResourceAccessMap({{resource("Layout1")->getId(),
        AccessRight::view | AccessRight::userInput | AccessRight::edit}});

    info = accessRightsModel->info()[indexOf(AccessRight::edit)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 2);
    EXPECT_EQ(info.checkedChildCount, 0);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 2);

    editingContext->setOwnResourceAccessMap({{resource("Camera1")->getId(), AccessRight::view}});

    info = accessRightsModel->info()[indexOf(AccessRight::view)];
    EXPECT_EQ(info.providedVia, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_EQ(info.inheritedFrom, ResourceAccessInfo::ProvidedVia::none);
    EXPECT_TRUE(info.parentResourceGroupId.isNull());
    EXPECT_TRUE(info.providerUserGroups.empty());
    EXPECT_TRUE(info.indirectProviders.empty());
    EXPECT_EQ(info.totalChildCount, 2);
    EXPECT_EQ(info.checkedChildCount, 1);
    EXPECT_EQ(info.checkedAndInheritedChildCount, 2);
}

TEST_F(ResourceAccessRightsModelTest, tooltipSorting)
{
    accessRightsModel->setResourceTreeIndex(indexOf(resource("Camera1")));

    UserGroupData ldapGroup;
    ldapGroup.type = UserType::ldap;
    ldapGroup.parentGroupIds = {kPowerUsersGroupId};

    const auto ldapId1 = QnUuid::createUuid();
    ldapGroup.id = ldapId1;
    ldapGroup.name = "LDAP Group";
    addOrUpdateUserGroup(ldapGroup);

    const auto ldapId2 = QnUuid::createUuid();
    ldapGroup.id = ldapId2;
    ldapGroup.name = "Another LDAP Group";
    addOrUpdateUserGroup(ldapGroup);

    testUser->setGroupIds({ldapId1, ldapId2, kLiveViewersGroupId, kViewersGroupId, customGroupId});

    editingContext->setOwnResourceAccessMap({
        {kAllDevicesGroupId, AccessRight::view},
        {resource("Layout1")->getId(), AccessRight::view},
        {resource("Layout2")->getId(), AccessRight::view},
        {resource("VideoWall1")->getId(), AccessRight::edit}});

    const auto tooltip = accessRightsModel->data(
        accessRightsModel->index(indexOf(AccessRight::view), 0), Qt::ToolTipRole).toString();

    const auto lines = tooltip.split("<br>");

    ASSERT_EQ(lines.size(), 4);
    EXPECT_EQ(splitTooltipLine(lines[0]), QStringList({"Cameras & Devices"}));
    EXPECT_EQ(splitTooltipLine(lines[1]), QStringList({"Layout1", "Layout2"}));
    EXPECT_EQ(splitTooltipLine(lines[2]), QStringList({"VideoWall1"}));
    EXPECT_EQ(splitTooltipLine(lines[3]), QStringList(
        {"Viewers", "Live Viewers", "Custom Group", "Another LDAP Group", "LDAP Group"}));
}

} // namespace test
} // namespace nx::vms::client::desktop
