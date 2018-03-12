#ifndef DEFAULT_REF_COUNTER_H
#define DEFAULT_REF_COUNTER_H

#include <plugins/plugin_tools.h>

namespace rpi_cam
{
    template <typename T>
    class DefaultRefCounter : public T
    {
    public:
        virtual unsigned int addRef() override { return m_refManager.addRef(); }
        virtual unsigned int releaseRef() override { return m_refManager.releaseRef(); }

    protected:
        nxpt::CommonRefManager m_refManager;

        DefaultRefCounter()
        :   m_refManager(this)
        {}

        DefaultRefCounter(nxpt::CommonRefManager * refManager)
        :   m_refManager(refManager)
        {}
    };
}

#endif //DEFAULT_REF_COUNTER_H
