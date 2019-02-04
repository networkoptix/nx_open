#pragma once

#include <typeindex>

#include <nx/vms/server/server_module_aware.h>

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnMediaServerResourceSearchers(QnMediaServerModule* serverModule);
    virtual ~QnMediaServerResourceSearchers() override;

    template <typename T>
    T* searcher() const
    {
        auto result = dynamic_cast<T*>(m_searchers.value(std::type_index(typeid(T))));
        NX_ASSERT(result);
        return result;
    }

    void initialize();
    void clear();

private:
    template <typename T> void registerSearcher(T* instance);

private:
    QMap<std::type_index, QnAbstractResourceSearcher*> m_searchers;
};
