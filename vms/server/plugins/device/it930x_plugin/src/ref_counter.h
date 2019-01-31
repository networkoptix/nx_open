#ifndef DEFAULT_REF_COUNTER_H
#define DEFAULT_REF_COUNTER_H

#include <typeinfo>
#include "object_counter.h"
#include <plugins/plugin_tools.h>

extern Logger logger;

namespace ite
{
    template <typename T>
    class DefaultRefCounter : public T
    {
    public:
        virtual unsigned int addRef() override { return m_refManager.addRef(); }
        virtual unsigned int releaseRef() override
        {
            int refCount = m_refManager.releaseRef();
            return refCount;
        }

    protected:
        nxpt::CommonRefManager m_refManager;

        DefaultRefCounter()
        :   m_refManager(static_cast<T*>(this))
        {}

        DefaultRefCounter(nxpt::CommonRefManager * refManager)
        :   m_refManager(refManager)
        {}
    };
}

#endif //DEFAULT_REF_COUNTER_H
