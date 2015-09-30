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

/** Initial test. Check if empty helper is valid. */
TEST( QnWorkbenchAccessControllerTest, init )
{
    QScopedPointer<QnClientModule> module(new QnClientModule());
    QScopedPointer<QnSkin> skin(new QnSkin());

    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(qnResPool));  
    QnUserResourcePtr user(new QnUserResource());
    user->setId(QnUuid::createUuid());
    user->setName("ut");
    qnResPool->addResource(user);

    context->setUserName("ut");
    
    ASSERT_EQ(user, context->user());
}