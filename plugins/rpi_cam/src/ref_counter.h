#ifndef DEFAULT_REF_COUNTER_H
#define DEFAULT_REF_COUNTER_H

#include <plugins/plugin_tools.h>

namespace nxpl
{
    inline bool operator == ( const nxpl::NX_GUID& id1, const nxpl::NX_GUID& id2 )
    {
        for( unsigned i=0; i < sizeof(nxpl::NX_GUID); ++i )
        {
            if (id1.bytes[i] != id2.bytes[i])
                return false;
        }
        return true;
    }
}

#define DEF_REF_COUNTER \
    public: \
        virtual unsigned int addRef() override; \
        virtual unsigned int releaseRef() override; \
        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override; \
    private: \
        nxpt::CommonRefManager m_refManager;

#define DEFAULT_REF_COUNTER(ClassName) \
    unsigned int ClassName::addRef() { return m_refManager.addRef(); } \
    unsigned int ClassName::releaseRef() { return m_refManager.releaseRef(); }

#endif //DEFAULT_REF_COUNTER_H
