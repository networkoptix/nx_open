#pragma once

#include <typeindex>

#include <nx/mediaserver/server_module_aware.h>

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers: public QObject, public nx::mediaserver::ServerModuleAware
{
public:
    QnMediaServerResourceSearchers(QnMediaServerModule* serverModule);
    virtual ~QnMediaServerResourceSearchers() override;

    template <typename T>
    T* searcher() const { return dynamic_cast<T*>(m_searchers.value(std::type_index(typeid(T)))); }

    void start();
    void stop();
private:
    template <typename T> void registerSearcher(T* instance);
private:
    QMap<std::type_index, QnAbstractResourceSearcher*> m_searchers;
};
