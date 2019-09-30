// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef DEFAULT_REF_COUNTER_H
#define DEFAULT_REF_COUNTER_H

#include <plugins/plugin_tools.h>

namespace rpi_cam
{
    template <typename T>
    class DefaultRefCounter : public T
    {
    public:
        virtual int addRef() const override { return m_refManager.addRef(); }
        virtual int releaseRef() const override { return m_refManager.releaseRef(); }

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
