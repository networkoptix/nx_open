#include "guard.h"

Guard::Guard(const Callback &callback)
    : m_callback(callback)
{
}

Guard::Guard(Callback&& callback)
    : m_callback(std::move(callback))
{
}

Guard::~Guard()
{
    try
    {
        fire();
    }
    catch(...) // shoud not have happen, but what if
    {
    }
}

void Guard::fire()
{
    if (m_callback)
        m_callback();

    disarm();
}

void Guard::disarm()
{
    m_callback = nullptr;
}
