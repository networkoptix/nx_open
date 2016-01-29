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

/** saves socket attributes and applies them to a given socket */
template<class ParentType>
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
            &m_reuseAddrFlag,
            &AbstractSocket::setReuseAddrFlag,
            val);
    }
    virtual bool getReuseAddrFlag(bool* val) const override
    {
        return getAttributeValue(
            m_reuseAddrFlag,
            &AbstractSocket::getReuseAddrFlag,
            decltype(m_reuseAddrFlag)(false),
            val);
    }
    virtual bool setNonBlockingMode(bool val) override
    {
        return setAttributeValue(
            &m_nonBlockingMode,
            &AbstractSocket::setNonBlockingMode,
            val);
    }
    virtual bool getNonBlockingMode(bool* val) const override
    {
        return getAttributeValue(
            m_nonBlockingMode,
            &AbstractSocket::getNonBlockingMode,
            decltype(m_nonBlockingMode)(false),
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
            &m_sendBufferSize,
            &AbstractSocket::setSendBufferSize,
            val);
    }
    virtual bool getSendBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_sendBufferSize,
            &AbstractSocket::getSendBufferSize,
            decltype(m_sendBufferSize)(),
            val);
    }
    virtual bool setRecvBufferSize(unsigned int val) override
    {
        return setAttributeValue(
            &m_recvBufferSize,
            &AbstractSocket::setRecvBufferSize,
            val);
    }
    virtual bool getRecvBufferSize(unsigned int* val) const override
    {
        return getAttributeValue(
            m_recvBufferSize,
            &AbstractSocket::getRecvBufferSize,
            decltype(m_recvBufferSize)(),
            val);
    }
    virtual bool setRecvTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_recvTimeout,
            &AbstractSocket::setRecvTimeout,
            millis);
    }
    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_recvTimeout,
            &AbstractSocket::getRecvTimeout,
            decltype(m_recvTimeout)(0),
            millis);
    }
    virtual bool setSendTimeout(unsigned int millis) override
    {
        return setAttributeValue(
            &m_sendTimeout,
            &AbstractSocket::setSendTimeout,
            millis);
    }
    virtual bool getSendTimeout(unsigned int* millis) const override
    {
        return getAttributeValue(
            m_sendTimeout,
            &AbstractSocket::getSendTimeout,
            decltype(m_sendTimeout)(0),
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

        if (m_reuseAddrFlag)
            m_delegate->setReuseAddrFlag(*m_reuseAddrFlag);
        if (m_nonBlockingMode)
            m_delegate->setNonBlockingMode(*m_nonBlockingMode);
        if (m_sendBufferSize)
            m_delegate->setSendBufferSize(*m_sendBufferSize);
        if (m_recvBufferSize)
            m_delegate->setRecvBufferSize(*m_recvBufferSize);
        if (m_recvTimeout)
            m_delegate->setRecvTimeout(*m_recvTimeout);
        if (m_sendTimeout)
            m_delegate->setSendTimeout(*m_sendTimeout);
    }

protected:
    ParentType* m_delegate;

    boost::optional<bool> m_reuseAddrFlag;
    boost::optional<bool> m_nonBlockingMode;
    boost::optional<unsigned int> m_sendBufferSize;
    boost::optional<unsigned int> m_recvBufferSize;
    boost::optional<unsigned int> m_recvTimeout;
    boost::optional<unsigned int> m_sendTimeout;

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

/** saves socket attributes and applies them to a given socket */
template<class ParentType>
class AbstractStreamSocketAttributesCache
:
    public AbstractSocketAttributesCache<ParentType>
{
public:
    virtual bool setNoDelay(bool val) override
    {
        return this->setAttributeValue(
            &m_noDelay,
            &AbstractStreamSocket::setNoDelay,
            val);
    }
    virtual bool getNoDelay(bool* val) const override
    {
        return this->getAttributeValue(
            m_noDelay,
            &AbstractStreamSocket::getNoDelay,
            boost::optional<bool>(false),
            val);
    }
    virtual bool toggleStatisticsCollection(bool val) override
    {
        if (m_delegate)
            return m_delegate->toggleStatisticsCollection(val);
        SystemError::setLastErrorCode(SystemError::notSupported);
        return false;
    }
    virtual bool getConnectionStatistics(StreamSocketInfo* info) override
    {
        if (m_delegate)
            return m_delegate->getConnectionStatistics(info);
        SystemError::setLastErrorCode(SystemError::notSupported);
        return false;
    }
    virtual bool setKeepAlive(boost::optional< KeepAliveOptions > val) override
    {
        return this->setAttributeValue(
            &m_keepAlive,
            &AbstractStreamSocket::setKeepAlive,
            val);
    }
    virtual bool getKeepAlive(boost::optional< KeepAliveOptions >* val) const override
    {
        return this->getAttributeValue(
            m_keepAlive,
            &AbstractStreamSocket::getKeepAlive,
            decltype(m_keepAlive)(),
            val);
    }

    void setDelegate(ParentType* _delegate)
    {
        this->AbstractSocketAttributesCache<ParentType>::setDelegate(_delegate);

        if (m_noDelay)
            m_delegate->setNoDelay(*m_noDelay);
        if (m_keepAlive)
            m_delegate->setKeepAlive(*m_keepAlive);
    }

private:
    boost::optional<bool> m_noDelay;
    boost::optional<boost::optional< KeepAliveOptions >> m_keepAlive;
};

}   //network
}   //nx
