#pragma once

#include <functional>
#include <tuple>

#include <nx/utils/atomic_unique_ptr.h>

#include "abstract_async_channel.h"
#include "../protocol_detector.h"

namespace nx {
namespace network {
namespace aio {

/**
 * 1. Reads data from another channel.
 * 2. Detects protocol using pre-configured rules.
 * 3. When detected some protocol, instantiates proper channel type
 *    and delegates already-received data and all futher I/O operations to the new channel.
 */
template<typename Base, typename AsyncChannelInterface>
// requires AsyncChannel<Base> && AsyncChannel<AsyncChannelInterface>
class BaseProtocolDetectingAsyncChannel:
    public Base
{
    using base_type = Base;

public:
    using ProtocolProcessorFactoryFunc =
        std::function<std::unique_ptr<AsyncChannelInterface>(
            std::unique_ptr<AsyncChannelInterface> /*rawDataSource*/)>;

    template<typename ... BaseInitializeArgs>
    BaseProtocolDetectingAsyncChannel(
        std::unique_ptr<AsyncChannelInterface> dataSource,
        BaseInitializeArgs&& ... baseInitializeArgs);

    BaseProtocolDetectingAsyncChannel(const BaseProtocolDetectingAsyncChannel&) = delete;
    BaseProtocolDetectingAsyncChannel& operator=(const BaseProtocolDetectingAsyncChannel&) = delete;
    BaseProtocolDetectingAsyncChannel(BaseProtocolDetectingAsyncChannel&&) = delete;
    BaseProtocolDetectingAsyncChannel& operator=(BaseProtocolDetectingAsyncChannel&&) = delete;

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) override;

    void registerDefaultProtocol(ProtocolProcessorFactoryFunc delegateFactoryFunc);
    void registerProtocol(
        std::unique_ptr<AbstractProtocolRule> detectionRule,
        ProtocolProcessorFactoryFunc delegateFactoryFunc);

    void detectProtocol(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> completionHandler);

protected:
    /**
     * NOTE: setDataSource call MUST follow this initilizer.
     */
    template<typename ... BaseInitializeArgs>
    BaseProtocolDetectingAsyncChannel(
        BaseInitializeArgs&& ... baseInitializeArgs);

    void setDataSource(std::unique_ptr<AsyncChannelInterface> dataSource);

    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

    void cleanUp();
    bool isProtocolDetected() const;
    /**
     * @return true if protocol has been detected and processor created. False otherwise.
     */
    bool analyzeMoreData(nx::Buffer data);

    AsyncChannelInterface& actualDataChannel();
    const AsyncChannelInterface& actualDataChannel() const;

    virtual std::unique_ptr<AsyncChannelInterface> installProtocolHandler(
        ProtocolProcessorFactoryFunc& factoryFunc,
        std::unique_ptr<AsyncChannelInterface> dataSource,
        nx::Buffer readDataCache) = 0;

private:
    std::unique_ptr<AsyncChannelInterface> m_dataSource;
    ProtocolProcessorFactoryFunc m_defaultProtocolProcessorFactory;
    nx::utils::AtomicUniquePtr<AsyncChannelInterface> m_delegate;
    ProtocolDetector<ProtocolProcessorFactoryFunc> m_protocolDetector;
    std::tuple<nx::Buffer*, IoCompletionHandler> m_pendingAsyncRead;
    nx::Buffer m_readBuffer;
    nx::Buffer m_readDataCache;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>
        m_protocolDetectionCompletionHandler;

    void completePendingAsyncRead(SystemError::ErrorCode protocolDetectionResultCode);

    std::unique_ptr<AsyncChannelInterface> useDataSourceDirectly(
        std::unique_ptr<AsyncChannelInterface> dataSource);

    void readProtocolHeaderAsync();

    void analyzeReadResult(
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead);
};

//-------------------------------------------------------------------------------------------------
// BaseProtocolDetectingAsyncChannel implementation.

template<typename Base, typename AsyncChannelInterface>
template<typename ... BaseInitializeArgs>
BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::BaseProtocolDetectingAsyncChannel(
    std::unique_ptr<AsyncChannelInterface> dataSource,
    BaseInitializeArgs&& ... baseInitializeArgs)
    :
    base_type(std::forward<BaseInitializeArgs>(baseInitializeArgs)...),
    m_defaultProtocolProcessorFactory(std::bind(
        &BaseProtocolDetectingAsyncChannel::useDataSourceDirectly, this,
        std::placeholders::_1))
{
    setDataSource(std::move(dataSource));
}

template<typename Base, typename AsyncChannelInterface>
template<typename ... BaseInitializeArgs>
BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::BaseProtocolDetectingAsyncChannel(
    BaseInitializeArgs&& ... baseInitializeArgs)
    :
    base_type(std::forward<BaseInitializeArgs>(baseInitializeArgs)...),
    m_defaultProtocolProcessorFactory(std::bind(
        &BaseProtocolDetectingAsyncChannel::useDataSourceDirectly, this,
        std::placeholders::_1))
{
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::setDataSource(
    std::unique_ptr<AsyncChannelInterface> dataSource)
{
    m_dataSource.swap(dataSource);
    bindToAioThread(m_dataSource->getAioThread());
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::bindToAioThread(
    AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_dataSource)
        m_dataSource->bindToAioThread(aioThread);
    if (m_delegate)
        m_delegate->bindToAioThread(aioThread);
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    using namespace std::placeholders;

    if (m_delegate)
        return m_delegate->readSomeAsync(buffer, std::move(handler));

    this->post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_delegate)
                return m_delegate->readSomeAsync(buffer, std::move(handler));

            m_pendingAsyncRead = std::make_tuple(buffer, std::move(handler));
            detectProtocol(
                std::bind(&BaseProtocolDetectingAsyncChannel::completePendingAsyncRead,
                    this, _1));
        });
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::sendAsync(
    const nx::Buffer& buffer,
    IoCompletionHandler handler)
{
    if (m_delegate)
        return m_delegate->sendAsync(buffer, std::move(handler));

    this->post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (m_delegate)
                return m_delegate->sendAsync(buffer, std::move(handler));

            handler(SystemError::invalidData, (std::size_t)-1);
        });
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::registerDefaultProtocol(
    ProtocolProcessorFactoryFunc delegateFactoryFunc)
{
    m_defaultProtocolProcessorFactory.swap(delegateFactoryFunc);
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::registerProtocol(
    std::unique_ptr<AbstractProtocolRule> detectionRule,
    ProtocolProcessorFactoryFunc delegateFactoryFunc)
{
    m_protocolDetector.registerProtocol(
        std::move(detectionRule),
        std::move(delegateFactoryFunc));
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::detectProtocol(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> completionHandler)
{
    this->post(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_ASSERT(!m_delegate);
            m_protocolDetectionCompletionHandler = std::move(completionHandler);
            readProtocolHeaderAsync();
        });
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::completePendingAsyncRead(
    SystemError::ErrorCode protocolDetectionResultCode)
{
    if (protocolDetectionResultCode != SystemError::noError)
    {
        return nx::utils::swapAndCall(
            std::get<1>(m_pendingAsyncRead), protocolDetectionResultCode, -1);
    }

    NX_ASSERT(m_delegate);

    m_delegate->readSomeAsync(
        std::get<0>(m_pendingAsyncRead),
        std::move(std::get<1>(m_pendingAsyncRead)));
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::cleanUp()
{
    m_dataSource.reset();
    m_delegate.reset();
}

template<typename Base, typename AsyncChannelInterface>
bool BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::isProtocolDetected() const
{
    return m_delegate != nullptr;
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::cancelIoInAioThread(
    nx::network::aio::EventType eventType)
{
    if (m_delegate)
        m_delegate->cancelIOSync(eventType);
    else if (m_dataSource)
        m_dataSource->cancelIOSync(eventType);
}

template<typename Base, typename AsyncChannelInterface>
std::unique_ptr<AsyncChannelInterface>
    BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::useDataSourceDirectly(
        std::unique_ptr<AsyncChannelInterface> dataSource)
{
    return dataSource;
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::readProtocolHeaderAsync()
{
    using namespace std::placeholders;

    constexpr int readBufSize = 4*1024;

    m_readBuffer.reserve(readBufSize);
    m_dataSource->readSomeAsync(
        &m_readBuffer,
        std::bind(&BaseProtocolDetectingAsyncChannel::analyzeReadResult, this, _1, _2));
}

template<typename Base, typename AsyncChannelInterface>
void BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::analyzeReadResult(
    SystemError::ErrorCode osErrorCode,
    std::size_t /*bytesRead*/)
{
    if (osErrorCode != SystemError::noError)
    {
        nx::utils::swapAndCall(
            m_protocolDetectionCompletionHandler,
            osErrorCode);
        return;
    }

    decltype(m_readBuffer) readBuffer;
    m_readBuffer.swap(readBuffer);

    if (!analyzeMoreData(std::move(readBuffer)))
    {
        readProtocolHeaderAsync();
        return;
    }

    nx::utils::swapAndCall(
        m_protocolDetectionCompletionHandler,
        SystemError::noError);
}

template<typename Base, typename AsyncChannelInterface>
bool BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::analyzeMoreData(
    nx::Buffer data)
{
    m_readDataCache += std::move(data);

    const auto[resultCode, descriptor] = m_protocolDetector.match(m_readDataCache);
    switch (resultCode)
    {
        case ProtocolMatchResult::detected:
            m_delegate = installProtocolHandler(
                *descriptor,
                std::move(m_dataSource),
                std::move(m_readDataCache));
            break;

        case ProtocolMatchResult::needMoreData:
            return false;

        case ProtocolMatchResult::unknownProtocol:
            m_delegate = installProtocolHandler(
                m_defaultProtocolProcessorFactory,
                std::move(m_dataSource),
                std::move(m_readDataCache));
            break;
    }

    return true;
}

template<typename Base, typename AsyncChannelInterface>
AsyncChannelInterface&
    BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::actualDataChannel()
{
    if (m_delegate)
        return *m_delegate;
    return *m_dataSource;
}

template<typename Base, typename AsyncChannelInterface>
const AsyncChannelInterface&
    BaseProtocolDetectingAsyncChannel<Base, AsyncChannelInterface>::actualDataChannel() const
{
    if (m_delegate)
        return *m_delegate;
    return *m_dataSource;
}

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ProtocolDetectingAsyncChannel:
    public BaseProtocolDetectingAsyncChannel<AbstractAsyncChannel, AbstractAsyncChannel>
{
    using base_type =
        BaseProtocolDetectingAsyncChannel<AbstractAsyncChannel, AbstractAsyncChannel>;

public:
    ProtocolDetectingAsyncChannel(
        std::unique_ptr<AbstractAsyncChannel> dataSource);

protected:
    virtual std::unique_ptr<AbstractAsyncChannel> installProtocolHandler(
        ProtocolProcessorFactoryFunc& factoryFunc,
        std::unique_ptr<AbstractAsyncChannel> dataSource,
        nx::Buffer readDataCache) override;

    virtual void stopWhileInAioThread() override;
};

} // namespace aio
} // namespace network
} // namespace nx
