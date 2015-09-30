#define QN_NO_KEYWORD_UNUSED
#include <gtest/gtest.h>

#include <client/client_module.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

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



    // Declares the variables your tests want to use.
    QSharedPointer<QnClientModule> m_module;
    QSharedPointer<QnSkin> m_skin;
    QSharedPointer<QnWorkbenchContext> m_context;
};



/** Initial test. Check if empty helper is valid. */
TEST_F( QnWorkbenchAccessControllerTest, init )
{
    auto user = addUser(userName1, Qn::GlobalOwnerPermissions);  
    m_context->setUserName(userName1);
    
    ASSERT_EQ(user, m_context->user());
}
