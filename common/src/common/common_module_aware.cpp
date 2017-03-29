#include "common_module_aware.h"
#include "common_module.h"

QnCommonModuleAware::QnCommonModuleAware(QnCommonModule* commonModule)
{
    init(commonModule);
}

QnCommonModuleAware::QnCommonModuleAware(QObject* parent)
{
    init(parent);
}

void QnCommonModuleAware::init(QObject* parent)
{
    for (;parent; parent = parent->parent())
    {
        QnCommonModuleAware* moduleAware = dynamic_cast<QnCommonModuleAware*>(parent);
        if (moduleAware != nullptr)
        {
            m_commonModule = moduleAware->commonModule();
            NX_ASSERT(m_commonModule, Q_FUNC_INFO, "Invalid context");
            break;
        }

        m_commonModule = dynamic_cast<QnCommonModule*>(parent);
        if (m_commonModule)
            break;
    }
}

QnCommonModule* QnCommonModuleAware::commonModule() const
{
    return m_commonModule;
}

QnLicensePool* QnCommonModuleAware::licensePool() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->licensePool() : nullptr;
}

QnRuntimeInfoManager* QnCommonModuleAware::runtimeInfoManager() const
{
    NX_ASSERT(m_commonModule);
    return m_commonModule ? m_commonModule->runtimeInfoManager() : nullptr;
}
