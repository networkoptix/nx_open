// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pollable.h"

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "aio_thread.h"
#include "../common_socket_impl.h"
#include "../socket_global.h"

namespace nx::network {

Pollable::Pollable(
    aio::AbstractAioThread* aioThread,
    AbstractSocket::SOCKET_HANDLE fd,
    std::unique_ptr<CommonSocketImpl> impl)
    :
    m_fd(fd),
    m_impl(impl ? std::move(impl) : std::make_unique<CommonSocketImpl>())
{
    SocketGlobals::verifyInitialization();

    if (!m_impl)
        m_impl = std::make_unique<CommonSocketImpl>();

    bindToAioThread(aioThread);
}

// NOTE: The destructor is needed to hide CommonSocketImpl declaration.
Pollable::~Pollable() = default;

AbstractSocket::SOCKET_HANDLE Pollable::handle() const
{
    return m_fd;
}

AbstractSocket::SOCKET_HANDLE Pollable::takeHandle()
{
    auto systemHandle = m_fd;
    m_fd = AbstractSocket::kInvalidSocket;
    return systemHandle;
}

bool Pollable::getRecvTimeout(unsigned int* millis) const
{
    *millis = m_readTimeoutMS;
    return true;
}

bool Pollable::getSendTimeout(unsigned int* millis) const
{
    *millis = m_writeTimeoutMS;
    return true;
}

CommonSocketImpl* Pollable::impl()
{
    return m_impl.get();
}

const CommonSocketImpl* Pollable::impl() const
{
    return m_impl.get();
}

bool Pollable::getLastError(SystemError::ErrorCode* /*errorCode*/) const
{
    SystemError::setLastErrorCode(SystemError::notImplemented);
    return false;
}

nx::network::aio::AbstractAioThread* Pollable::getAioThread() const
{
    return m_impl->aioThread->load();
}

void Pollable::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    // TODO: #akolesnikov Get rid of the static_cast here.
    const auto desiredThread = static_cast<aio::AioThread*>(aioThread);
    if (m_impl->aioThread->load() == desiredThread)
        return;

    if (m_impl->aioThread.get())
    {
        NX_ASSERT(!m_impl->aioThread->load()->isSocketBeingMonitored(this));
    }

    // Socket can be bound to another aio thread, if it is not used at the moment.
    m_impl->aioThread.emplace(static_cast<aio::AioThread*>(desiredThread));
}

bool Pollable::isInSelfAioThread() const
{
    return m_impl->aioThread->load() != nullptr
        && m_impl->aioThread->load()->systemThreadId() == nx::utils::Thread::currentThreadSystemId();
}

} // namespace nx::network
