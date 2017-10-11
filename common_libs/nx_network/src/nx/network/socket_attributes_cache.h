#pragma once

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {

/**
 * Socket configuration attributes ready to apply to any AbstractSocket.
 */
class SocketAttributes
{
public:
    boost::optional<bool> reuseAddrFlag;
    boost::optional<bool> nonBlockingMode;
    boost::optional<unsigned int> sendBufferSize;
    boost::optional<unsigned int> recvBufferSize;
    boost::optional<unsigned int> recvTimeout;
    boost::optional<unsigned int> sendTimeout;
    boost::optional<aio::AbstractAioThread*> aioThread;

    bool applyTo(AbstractSocket* const socket) const
    {
        if (aioThread)
            socket->bindToAioThread(*aioThread); //< Success or assert.

        return
            apply(socket, &AbstractSocket::setReuseAddrFlag, reuseAddrFlag) &&
            apply(socket, &AbstractSocket::setNonBlockingMode, nonBlockingMode) &&
            apply(socket, &AbstractSocket::setSendBufferSize, sendBufferSize) &&
            apply(socket, &AbstractSocket::setRecvBufferSize, recvBufferSize) &&
            apply(socket, &AbstractSocket::setRecvTimeout, recvTimeout) &&
            apply(socket, &AbstractSocket::setSendTimeout, sendTimeout);
    }

protected:
    template<typename Socket, typename Attribute>
    bool apply(
        Socket* const socket,
        bool (Socket::*function)(Attribute value),
        const boost::optional<Attribute>& value) const
    {
        return value ? (socket->*function)(*value) : true;
    }
};

NX_NETWORK_API bool verifySocketAttributes(
    const AbstractSocket& socket,
    const SocketAttributes& attributes);

/**
 * Saves socket attributes and applies them to a given socket.
 */
template<class ParentType, class SocketAttributesHolderType>
class AbstractSocketAttributesCache:
    public ParentType
{
public:
    AbstractSocketAttributesCache(ParentType* delegate = nullptr):
        m_delegate(delegate)
    {
    }
    virtual ~AbstractSocketAttributesCache() = default;

    virtual bool setReuseAddrFlag(bool val) override
    {
        return setAttributeValue(
            &m_socketAttributes.reuseAddrFlag,
            &AbstractSocket::setReuseAddrFlag,
            val);
    }
    virtual bool getReuseAddrFlag(bool* val) const override
    {
        return getAttributeValue(
            m_socketAttributes.reuseAddrFlag,
            &AbstractSocket::getReuseAddrFlag,
            decltype(m_socketAttributes.reuseAddrFlag)(false),
            val);
    }
    virtual bool setNonBlockingMode(bool val) override
    {
        return setAttributeValue(
            &m_socketAttributes.nonBlockingMode,
            &AbstractSocket::setNonBlockingMode,
            val);
    }
    virtual bool getNonBlockingMode(bool* val) const override
    {
        return getAttributeValue(
            m_socketAttributes.nonBlockingMode,
            &AbstractSocket::getNonBlockingMode,
            decltype(m_socketAttributes.nonBlockingMode)(false),
            val);
    }
    virtual bool getMtu(unsigned int* mtuValue) const override
    {
        if (!m_delegate)
        {
            SystemError::setLastErrorCode(SystemError::notSupported);
            return false;
        }
        return m_delegate->getMtu(mtuValue);
    }
    virtual bool setSendBufferSize(unsigned int val) override
    {
        return setAttributeValue(
            &m_socketAttributes.sendBufferSize,
            &AbstractSocket::setSendBufferSize,
            val);
    }
    virtual bool getSendBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_socketAttributes.sendBufferSize,
            &AbstractSocket::getSendBufferSize,
            decltype(m_socketAttributes.sendBufferSize)(),
            val);
    }
    virtual bool setRecvBufferSize(unsigned int val) override
    {
        return setAttributeValue(
            &m_socketAttributes.recvBufferSize,
            &AbstractSocket::setRecvBufferSize,
            val);
    }
    virtual bool getRecvBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_socketAttributes.recvBufferSize,
            &AbstractSocket::getRecvBufferSize,
            decltype(m_socketAttributes.recvBufferSize)(),
            val);
    }
    virtual bool setRecvTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_socketAttributes.recvTimeout,
            static_cast<bool (AbstractSocket::*)(unsigned int)>(&AbstractSocket::setRecvTimeout),
            millis);
    }
    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_socketAttributes.recvTimeout,
            &AbstractSocket::getRecvTimeout,
            decltype(m_socketAttributes.recvTimeout)(0),
            millis);
    }
    virtual bool setSendTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_socketAttributes.sendTimeout,
            static_cast<bool (AbstractSocket::*)(unsigned int)>(&AbstractSocket::setSendTimeout),
            millis);
    }
    virtual bool getSendTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_socketAttributes.sendTimeout,
            &AbstractSocket::getSendTimeout,
            decltype(m_socketAttributes.sendTimeout)(0),
            millis);
    }
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override
    {
        if (!m_delegate)
        {
            SystemError::setLastErrorCode(SystemError::notSupported);
            return false;
        }
        return m_delegate->getLastError(errorCode);
    }
    virtual AbstractSocket::SOCKET_HANDLE handle() const override
    {
        NX_CRITICAL(m_delegate, "Not supported");
        return m_delegate->handle();
    }
    virtual nx::network::Pollable* pollable() override
    {
        if (!m_delegate)
            return nullptr;

        return m_delegate->pollable();
    }
    virtual aio::AbstractAioThread* getAioThread() const override
    {
        if (!m_delegate)
        {
            NX_CRITICAL(m_socketAttributes.aioThread,
                "Notfully supported while delegate is not set");

            return *m_socketAttributes.aioThread;
        }

        return m_delegate->getAioThread();
    }
    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        NX_CRITICAL(m_delegate, "Not supported");
        return m_delegate->post(std::move(handler));
    }
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        NX_CRITICAL(m_delegate, "Not supported");
        return m_delegate->dispatch(std::move(handler));
    }
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        if (!m_delegate)
            m_socketAttributes.aioThread = aioThread;
        else
            m_delegate->bindToAioThread(aioThread);
    }

protected:
    ParentType* m_delegate;
    SocketAttributesHolderType m_socketAttributes;

    void setDelegate(ParentType* _delegate)
    {
        m_delegate = _delegate;
        if (m_delegate)
            m_socketAttributes.applyTo(m_delegate);
    }

    SocketAttributesHolderType getSocketAttributes() const
    {
        return m_socketAttributes;
    }

    template<typename AttributeType, typename SocketClassType>
    bool setAttributeValue(
        boost::optional<AttributeType>* const attributeValueHolder,
        bool(SocketClassType::*attributeSetFunc)(AttributeType newValue),
        AttributeType attributeValue)
    {
        if (!m_delegate)
        {
            *attributeValueHolder = attributeValue;
            return true;
        }

        if ((m_delegate->*attributeSetFunc)(attributeValue))
        {
            *attributeValueHolder = attributeValue;
            return true;
        }
        return false;
    }

    template<typename AttributeType, typename SocketClassType>
    bool getAttributeValue(
        const boost::optional<AttributeType>& attributeValueHolder,
        bool(SocketClassType::*attributeGetFunc)(AttributeType* value) const,
        boost::optional<AttributeType> defaultAttributeValue,
        AttributeType* const attributeValue) const
    {
        if (m_delegate)
            return (m_delegate->*attributeGetFunc)(attributeValue);
        if (attributeValueHolder)
        {
            *attributeValue = *attributeValueHolder;
            return true;
        }

        if (defaultAttributeValue)
        {
            *attributeValue = *defaultAttributeValue;
            return true;
        }
        SystemError::setLastErrorCode(SystemError::notSupported);
        return false;
    }
};

/**
 * Socket configuration attributes ready to apply to any AbstractStreamSocket.
 */
class StreamSocketAttributes:
    public SocketAttributes
{
public:
    boost::optional<bool> noDelay;
    boost::optional<boost::optional<KeepAliveOptions>> keepAlive;

    bool applyTo(AbstractStreamSocket* const socket) const
    {
        return
            SocketAttributes::applyTo(socket) &&
            apply(socket, &AbstractStreamSocket::setNoDelay, noDelay) &&
            apply(socket, &AbstractStreamSocket::setKeepAlive, keepAlive);
    }
};

/**
 * Saves socket attributes and applies them to a given socket.
 */
template<class ParentType>
class AbstractStreamSocketAttributesCache:
    public AbstractSocketAttributesCache<ParentType, StreamSocketAttributes>
{
public:
    AbstractStreamSocketAttributesCache(ParentType* delegate = nullptr):
        AbstractSocketAttributesCache<ParentType, StreamSocketAttributes>(delegate)
    {
    }

    virtual bool setNoDelay(bool val) override
    {
        return this->setAttributeValue(
            &this->m_socketAttributes.noDelay,
            &AbstractStreamSocket::setNoDelay,
            val);
    }
    virtual bool getNoDelay(bool* val) const override
    {
        return this->getAttributeValue(
            this->m_socketAttributes.noDelay,
            &AbstractStreamSocket::getNoDelay,
            decltype(this->m_socketAttributes.noDelay)(false),
            val);
    }
    virtual bool toggleStatisticsCollection(bool val) override
    {
        if (this->m_delegate)
            return this->m_delegate->toggleStatisticsCollection(val);
        SystemError::setLastErrorCode(SystemError::notSupported);
        return false;
    }
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override
    {
        if (this->m_delegate)
            return this->m_delegate->getConnectionStatistics(info);
        SystemError::setLastErrorCode(SystemError::notSupported);
        return false;
    }
    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > val) override
    {
        return this->setAttributeValue(
            &this->m_socketAttributes.keepAlive,
            &AbstractStreamSocket::setKeepAlive,
            val);
    }
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* val) const override
    {
        return this->getAttributeValue(
            this->m_socketAttributes.keepAlive,
            &AbstractStreamSocket::getKeepAlive,
            decltype(this->m_socketAttributes.keepAlive)(),
            val);
    }
    virtual void registerTimer(
        std::chrono::milliseconds timeoutMs,
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        NX_CRITICAL(this->m_delegate, "Not supported");
        return this->m_delegate->registerTimer(timeoutMs, std::move(handler));
    }
};

} // namespace network
} // namespace nx
