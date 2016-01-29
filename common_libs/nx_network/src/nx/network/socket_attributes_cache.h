/**********************************************************
* Jan 29, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <boost/optional.hpp>

#include <utils/common/systemerror.h>

#include "nx/network/abstract_socket.h"


namespace nx {
namespace network {

class SocketAttributes
{
public:
    boost::optional<bool> reuseAddrFlag;
    boost::optional<bool> nonBlockingMode;
    boost::optional<unsigned int> sendBufferSize;
    boost::optional<unsigned int> recvBufferSize;
    boost::optional<unsigned int> recvTimeout;
    boost::optional<unsigned int> sendTimeout;

    void applyTo(AbstractSocket* const targetSocket)
    {
        if (reuseAddrFlag)
            targetSocket->setReuseAddrFlag(*reuseAddrFlag);
        if (nonBlockingMode)
            targetSocket->setNonBlockingMode(*nonBlockingMode);
        if (sendBufferSize)
            targetSocket->setSendBufferSize(*sendBufferSize);
        if (recvBufferSize)
            targetSocket->setRecvBufferSize(*recvBufferSize);
        if (recvTimeout)
            targetSocket->setRecvTimeout(*recvTimeout);
        if (sendTimeout)
            targetSocket->setSendTimeout(*sendTimeout);
    }
};

/** saves socket attributes and applies them to a given socket */
template<class ParentType, class SocketAttributesHolderType>
class AbstractSocketAttributesCache
:
    public ParentType
{
public:
    AbstractSocketAttributesCache()
    :
        m_delegate(nullptr)
    {
    }

    virtual ~AbstractSocketAttributesCache()
    {
    }

    virtual bool setReuseAddrFlag(bool val) override
    {
        return setAttributeValue(
            &m_socketOptions.reuseAddrFlag,
            &AbstractSocket::setReuseAddrFlag,
            val);
    }
    virtual bool getReuseAddrFlag(bool* val) const override
    {
        return getAttributeValue(
            m_socketOptions.reuseAddrFlag,
            &AbstractSocket::getReuseAddrFlag,
            decltype(m_socketOptions.reuseAddrFlag)(false),
            val);
    }
    virtual bool setNonBlockingMode(bool val) override
    {
        return setAttributeValue(
            &m_socketOptions.nonBlockingMode,
            &AbstractSocket::setNonBlockingMode,
            val);
    }
    virtual bool getNonBlockingMode(bool* val) const override
    {
        return getAttributeValue(
            m_socketOptions.nonBlockingMode,
            &AbstractSocket::getNonBlockingMode,
            decltype(m_socketOptions.nonBlockingMode)(false),
            val);
    }
    virtual bool getMtu(unsigned int* mtuValue) const
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
            &m_socketOptions.sendBufferSize,
            &AbstractSocket::setSendBufferSize,
            val);
    }
    virtual bool getSendBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_socketOptions.sendBufferSize,
            &AbstractSocket::getSendBufferSize,
            decltype(m_socketOptions.sendBufferSize)(),
            val);
    }
    virtual bool setRecvBufferSize(unsigned int val) override
    {
        return setAttributeValue(
            &m_socketOptions.recvBufferSize,
            &AbstractSocket::setRecvBufferSize,
            val);
    }
    virtual bool getRecvBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_socketOptions.recvBufferSize,
            &AbstractSocket::getRecvBufferSize,
            decltype(m_socketOptions.recvBufferSize)(),
            val);
    }
    virtual bool setRecvTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_socketOptions.recvTimeout,
            &AbstractSocket::setRecvTimeout,
            millis);
    }
    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_socketOptions.recvTimeout,
            &AbstractSocket::getRecvTimeout,
            decltype(m_socketOptions.recvTimeout)(0),
            millis);
    }
    virtual bool setSendTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_socketOptions.sendTimeout,
            &AbstractSocket::setSendTimeout,
            millis);
    }
    virtual bool getSendTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_socketOptions.sendTimeout,
            &AbstractSocket::getSendTimeout,
            decltype(m_socketOptions.sendTimeout)(0),
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

    void setDelegate(ParentType* _delegate)
    {
        m_delegate = _delegate;
        m_socketOptions.applyTo(m_delegate);
    }

    SocketAttributesHolderType getSocketOptions() const
    {
        return m_socketOptions;
    }

protected:
    ParentType* m_delegate;

    SocketAttributesHolderType m_socketOptions;

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


class StreamSocketAttributes
:
    public SocketAttributes
{
public:
    boost::optional<bool> noDelay;
    boost::optional<boost::optional< KeepAliveOptions >> keepAlive;

    void applyTo(AbstractStreamSocket* const streamSocket)
    {
        SocketAttributes::applyTo(streamSocket);

        if (noDelay)
            streamSocket->setNoDelay(*noDelay);
        if (keepAlive)
            streamSocket->setKeepAlive(*keepAlive);
    }
};

/** saves socket attributes and applies them to a given socket */
template<class ParentType>
class AbstractStreamSocketAttributesCache
:
    public AbstractSocketAttributesCache<ParentType, StreamSocketAttributes>
{
public:
    virtual bool setNoDelay(bool val) override
    {
        return this->setAttributeValue(
            &m_socketOptions.noDelay,
            &AbstractStreamSocket::setNoDelay,
            val);
    }
    virtual bool getNoDelay(bool* val) const override
    {
        return this->getAttributeValue(
            m_socketOptions.noDelay,
            &AbstractStreamSocket::getNoDelay,
            decltype(m_socketOptions.noDelay)(false),
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
            &m_socketOptions.keepAlive,
            &AbstractStreamSocket::setKeepAlive,
            val);
    }
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* val) const override
    {
        return this->getAttributeValue(
            m_socketOptions.keepAlive,
            &AbstractStreamSocket::getKeepAlive,
            decltype(m_socketOptions.keepAlive)(),
            val);
    }
};

}   //network
}   //nx
