#pragma once

#include <nx/network/abstract_socket.h>

class MockStreamSocket: public nx::network::AbstractStreamSocket
{
public:
    virtual bool setNoDelay(bool /*value*/) override { return true; }
    virtual bool getNoDelay(bool* /*value*/) const override { return true; }
    virtual bool toggleStatisticsCollection(bool /*val*/) override { return true; }
    virtual bool getConnectionStatistics(nx::network::StreamSocketInfo* /*info*/) override { return true; }
    virtual bool setKeepAlive(std::optional<nx::network::KeepAliveOptions> /*info*/) override { return true; }
    virtual bool getKeepAlive(std::optional<nx::network::KeepAliveOptions>* /*result*/) const override { return true; }

    virtual bool connect(
        const nx::network::SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds /*timeout*/) override
    {
        return true;
    }

    virtual int recv(void* /*buffer*/, unsigned int bufferLen, int /*flags*/ = 0) override
    {
        return bufferLen;
    }

    virtual int send(const void* /*buffer*/, unsigned int bufferLen) override
    {
        return bufferLen;
    }

    virtual nx::network::SocketAddress getForeignAddress() const override
    {
        return nx::network::SocketAddress();
    }

    virtual bool isConnected() const { return true; };

    virtual void connectAsync(
        const nx::network::SocketAddress& /*address*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/) override
    {
    }

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        nx::network::IoCompletionHandler /*handler*/) override
    {
    }

    virtual void sendAsync(
        const nx::Buffer& /*buffer*/,
        nx::network::IoCompletionHandler /*handler*/) override
    {
    }

    virtual void registerTimer(
        std::chrono::milliseconds /*timeout*/,
        nx::utils::MoveOnlyFunc<void()> /*handler*/) override
    {
    }

    virtual void cancelIoInAioThread(nx::network::aio::EventType /*eventType*/) override {}

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> /*handler*/) override {}

    virtual void pleaseStopSync() override {}

    virtual bool bind(const nx::network::SocketAddress& localAddress) override
    {
        m_localAddress = localAddress;
        return true;
    }

    virtual nx::network::SocketAddress getLocalAddress() const override
    {
        return m_localAddress;
    }

    virtual bool close() override { return true; }
    virtual bool isClosed() const override { return true; }
    virtual bool shutdown() override { return true; }

    virtual bool setReuseAddrFlag(bool reuseAddr) override
    {
        m_reuseAddrFlag = reuseAddr;
        return true;
    }

    virtual bool getReuseAddrFlag(bool* val) const override
    {
        if (val)
            *val = m_reuseAddrFlag;
        return true;
    }

    virtual bool setReusePortFlag(bool value) override
    {
        m_reusePortFlag = value;
        return true;
    }

    virtual bool getReusePortFlag(bool* value) const override
    {
        if (value)
            *value = m_reusePortFlag;
        return true;
    }

    virtual bool setNonBlockingMode(bool val) override
    {
        m_nonBlockingMode = val;
        return true;
    }

    virtual bool getNonBlockingMode(bool* val) const override
    {
        if (val)
            *val = m_nonBlockingMode;
        return true;
    }

    virtual bool getMtu(unsigned int* mtuValue) const override
    {
        *mtuValue = 0;
        return true;
    }

    virtual bool setSendBufferSize(unsigned int buffSize) override
    {
        m_sendBufferSize = buffSize;
        return true;
    }

    virtual bool getSendBufferSize(unsigned int* buffSize) const override
    {
        if (buffSize)
            *buffSize = m_sendBufferSize;
        return true;
    }

    virtual bool setRecvBufferSize(unsigned int buffSize) override
    {
        m_recvBufferSize = buffSize;
        return true;
    }

    virtual bool getRecvBufferSize(unsigned int* buffSize) const override
    {
        if (buffSize)
            *buffSize = m_recvBufferSize;
        return true;
    }

    virtual bool setRecvTimeout(unsigned int millis) override
    {
        m_recvTimeoutMs = millis;
        return true;
    }

    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        if (millis)
            *millis = m_recvTimeoutMs;
        return true;
    }

    virtual bool setSendTimeout(unsigned int ms) override
    {
        m_sendTimeoutMs = ms;
        return true;
    }

    virtual bool getSendTimeout(unsigned int* millis) const override
    {
        if (millis)
            *millis = m_sendTimeoutMs;;
        return true;
    }

    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override
    {
        if (errorCode)
            *errorCode = SystemError::noError;
        return true;
    }

    virtual bool setIpv6Only(bool /*val*/) override
    {
        return true;
    }

    virtual SOCKET_HANDLE handle() const override
    {
        return SOCKET_HANDLE();
    }

    virtual nx::network::Pollable* pollable()
    {
        return nullptr;
    }

    virtual void post(nx::utils::MoveOnlyFunc<void()> /*handler*/) override {}

    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> /*handler*/) override {}

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override
    {
        return m_aioThread;
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
    {
        m_aioThread = aioThread;
    }

private:
    bool m_reuseAddrFlag = false;
    bool m_reusePortFlag = false;
    bool m_nonBlockingMode = false;
    int m_sendBufferSize = 0;
    int m_recvBufferSize = 0;
    unsigned int m_recvTimeoutMs = 0;
    unsigned int m_sendTimeoutMs = 0;
    nx::network::aio::AbstractAioThread* m_aioThread = nullptr;
    nx::network::SocketAddress m_localAddress;
};
