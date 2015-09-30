#define QN_NO_KEYWORD_UNUSED
#include <gtest/gtest.h>

#include <common/common_module.h>

#include <client/client_module.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_type.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include <ui/style/skin.h>

namespace {
    const QString userName1 = QStringLiteral("unit_test_user_1");
    const QString userName2 = QStringLiteral("unit_test_user_2");
}

class QnWorkbenchAccessControllerTest : public testing::Test {
protected:  

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() {
        m_module.reset(new QnClientModule());
        m_skin.reset(new QnSkin());
        m_context.reset(new QnWorkbenchContext(qnResPool));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() {
        m_context.clear();
        m_skin.clear();
        m_module.clear();
    }

    QnUserResourcePtr addUser(const QString &name, Qn::Permissions globalPermissions) {
        QnUserResourcePtr user(new QnUserResource());
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setPermissions(globalPermissions);
        qnResPool->addResource(user);

        return user;
    }

    void loginAs(Qn::Permissions globalPermissions) {
        auto user = addUser(userName1, globalPermissions);  
        m_context->setUserName(userName1);
    }

    bool hasPermissions(const QnResourcePtr &resource, Qn::Permissions permissions) const {
        return m_context->accessController()->hasPermissions(resource, permissions);
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnClientModule> m_module;
    QSharedPointer<QnSkin> m_skin;
    QSharedPointer<QnWorkbenchContext> m_context;
};


/** Initial test. Check if current user is set correctly. */
TEST_F( QnWorkbenchAccessControllerTest, init )
{
    auto user = addUser(userName1, Qn::GlobalOwnerPermissions);  
    m_context->setUserName(userName1);
    
    ASSERT_EQ(user, m_context->user());
}

/** Test for safe mode. Check if the user cannot create new layouts on server. */
TEST_F( QnWorkbenchAccessControllerTest, safeDisableSaveLayoutAs )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);
    ASSERT_FALSE(hasPermissions(m_context->user(), Qn::CreateLayoutPermission));
}

/** Test for safe mode. Check if the user cannot save layout created with external permissions. */
TEST_F( QnWorkbenchAccessControllerTest, safeCannotSaveExternalPermissionsLayout )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    layout->setId(QnUuid::createUuid());
    layout->setData(Qn::LayoutPermissionsRole, Qn::ReadWriteSavePermission);
    qnResPool->addResource(layout);

    ASSERT_FALSE(hasPermissions(layout, Qn::SavePermission));
}

/** Fix permissions for exported layouts (files). */
TEST_F( QnWorkbenchAccessControllerTest, checkExportedLayouts )
{
    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    layout->setId(QnUuid::createUuid());
    layout->setUrl("path/to/file");
    layout->addFlags(Qn::url);
    qnResPool->addResource(layout);

    ASSERT_TRUE(QnWorkbenchLayoutSnapshotManager::isFile(layout));

    /* Exported layouts can be edited even by live users in safe mode. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* But their name is fixed. */
    Qn::Permission forbidden = Qn::WriteNamePermission;
    desired &= ~forbidden;

    ASSERT_TRUE(hasPermissions(layout, desired));
    ASSERT_FALSE(hasPermissions(layout, forbidden));

    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    /* Result is the same even for live users in safe mode. */
    ASSERT_TRUE(hasPermissions(layout, desired));
    ASSERT_FALSE(hasPermissions(layout, forbidden));
}
