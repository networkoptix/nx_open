#pragma once

#include <nx/mediaserver/server_module_aware.h>

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers: public QObject, public nx::mediaserver::ServerModuleAware
{
public:
    QnMediaServerResourceSearchers(QnMediaServerModule* serverModule);
    virtual ~QnMediaServerResourceSearchers();

private:
    QList<QnAbstractResourceSearcher*> m_searchers;
};
