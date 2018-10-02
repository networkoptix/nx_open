#pragma once

#include <nx/mediaserver/server_module_aware.h>
#ifdef ENABLE_FLIR
#include <plugins/resource/flir/flir_io_executor.h>
#endif

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers: public QObject, public nx::mediaserver::ServerModuleAware
{
public:
    QnMediaServerResourceSearchers(QnMediaServerModule* serverModule);
    virtual ~QnMediaServerResourceSearchers();

private:
    QList<QnAbstractResourceSearcher*> m_searchers;
#ifdef ENABLE_FLIR
    std::unique_ptr<nx::plugins::flir::IoExecutor> m_flirIoExecutor;
#endif
};
