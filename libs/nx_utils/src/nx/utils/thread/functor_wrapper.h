// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRunnable>

namespace nx {
namespace utils {

template<typename Functor>
class FunctorWrapper: public QRunnable
{
public:
    FunctorWrapper(Functor functor):
        m_functor(functor)
    {
    }

    virtual void run() override
    {
        m_functor();
    }

private:
    Functor m_functor;
};

} // namespace utils
} // namespace nx
