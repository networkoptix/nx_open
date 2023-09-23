// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module_aware.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>

#include "common_module.h"

using namespace nx::core::access;
using namespace nx::vms::common;

QnCommonModuleAware::QnCommonModuleAware(QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
    NX_ASSERT(m_commonModule);
}

QnCommonModule* QnCommonModuleAware::commonModule() const
{
    return m_commonModule;
}

nx::vms::common::SystemContext* QnCommonModuleAware::systemContext() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext() : nullptr;
}

QnUuid QnCommonModuleAware::peerId() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext()->peerId() : QnUuid();
}

QnUuid QnCommonModuleAware::sessionId() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext()->sessionId() : QnUuid();
}

QnLicensePool* QnCommonModuleAware::licensePool() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext()->licensePool() : nullptr;
}

QnRuntimeInfoManager* QnCommonModuleAware::runtimeInfoManager() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->runtimeInfoManager()
        : nullptr;
}

QnResourcePool* QnCommonModuleAware::resourcePool() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext()->resourcePool() : nullptr;
}

QnResourceAccessManager* QnCommonModuleAware::resourceAccessManager() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->resourceAccessManager()
        : nullptr;
}

ResourceAccessSubjectHierarchy* QnCommonModuleAware::accessSubjectHierarchy() const
{
    return m_commonModule
        ? m_commonModule->systemContext()->accessSubjectHierarchy()
        : nullptr;
}

UserGroupManager* QnCommonModuleAware::userGroupManager() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->userGroupManager()
        : nullptr;
}

QnCameraHistoryPool* QnCommonModuleAware::cameraHistoryPool() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->cameraHistoryPool()
        : nullptr;
}

QnResourcePropertyDictionary* QnCommonModuleAware::resourcePropertyDictionary() const
{
    return m_commonModule
        ? m_commonModule->systemContext()->resourcePropertyDictionary()
        : nullptr;
}

QnResourceStatusDictionary* QnCommonModuleAware::statusDictionary() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->resourceStatusDictionary()
        : nullptr;
}

SystemSettings* QnCommonModuleAware::globalSettings() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->systemContext()->globalSettings() : nullptr;
}

nx::vms::event::RuleManager* QnCommonModuleAware::eventRuleManager() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->eventRuleManager()
        : nullptr;
}

QnResourceDataPool* QnCommonModuleAware::dataPool() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->resourceDataPool()
        : nullptr;
}

std::shared_ptr<ec2::AbstractECConnection> QnCommonModuleAware::messageBusConnection() const
{
    return NX_ASSERT(m_commonModule) ? m_commonModule->messageBusConnection() : nullptr;
}

QnCommonMessageProcessor* QnCommonModuleAware::messageProcessor() const
{
    return NX_ASSERT(m_commonModule)
        ? m_commonModule->systemContext()->messageProcessor()
        : nullptr;
}
