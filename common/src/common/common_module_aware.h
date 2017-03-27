#pragma once

class QnCommonModule;

class QnCommonModuleAware
{
public:
    QnCommonModuleAware(QnCommonModule* commonModule);
    QnCommonModuleAware(QObject* parent);

    QnCommonModule* commonModule() const;
private:
    void init(QObject *parent);
private:
    QnCommonModule* m_commonModule;
};
