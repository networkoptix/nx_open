#ifndef DEFAULT_REF_COUNTER_H
#define DEFAULT_REF_COUNTER_H

#include <plugins/plugin_tools.h>

namespace rpi_cam
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

    template <typename T>
    class DefaultRefCounter : public T
    {
    public:
        virtual unsigned int addRef() override;
        virtual unsigned int releaseRef() override;

    protected:
        nxpt::CommonRefManager m_refManager;

        DefaultRefCounter()
        :   m_refManager(this)
        {}

        DefaultRefCounter(nxpt::CommonRefManager * refManager)
        :   m_refManager(refManager)
        {}
    };

    #define DEFAULT_REF_COUNTER(T) \
        template <> unsigned int DefaultRefCounter<T>::addRef() { return m_refManager.addRef(); } \
        template <> unsigned int DefaultRefCounter<T>::releaseRef() { return m_refManager.releaseRef(); }
}

#endif //DEFAULT_REF_COUNTER_H
