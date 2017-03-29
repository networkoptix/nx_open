#pragma once

class QnCommonModule;
class QnLicensePool;
class QnRuntimeInfoManager;

class QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);
    QnCommonModuleAware(QObject* parent);

    QnCommonModule* commonModule() const;

    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
private:
    void init(QObject *parent);
private:
    QnCommonModule* m_commonModule;
};
