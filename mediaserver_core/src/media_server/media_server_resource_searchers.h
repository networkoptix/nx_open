#pragma once

#include <common/common_module_aware.h>

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers: public QObject, public QnCommonModuleAware
{
public:
    QnMediaServerResourceSearchers(QnCommonModule* commonModule);
    virtual ~QnMediaServerResourceSearchers();

private:
    QList<QnAbstractResourceSearcher*> m_searchers;
};
