/**********************************************************
* 25 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef MIXED_TCP_UDT_SERVER_SOCKET_H
#define MIXED_TCP_UDT_SERVER_SOCKET_H

#include <atomic>
#include <vector>
#include <queue>

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include "abstract_socket.h"


//!Listens simultaneously tcp and udt (udp) ports and accepts connections by both protocols
class MixedTcpUdtServerSocket
:
    public AbstractStreamServerSocket
{
public:
    MixedTcpUdtServerSocket();
    virtual ~MixedTcpUdtServerSocket();

    //////////////////////////////////////////////////////////////////////
    ///////// Implementation of AbstractSocket methods
    //////////////////////////////////////////////////////////////////////

    //!Implementation of AbstractSocket::bind
    virtual bool bind(const SocketAddress& localAddress) override;
    //!Implementation of AbstractSocket::getLocalAddress
    virtual SocketAddress getLocalAddress() const override;
    //!Implementation of AbstractSocket::getPeerAddress
    virtual SocketAddress getPeerAddress() const override;
    //!Implementation of AbstractSocket::close
    virtual void close() override;
    //!Implementation of AbstractSocket::isClosed
    virtual bool isClosed() const override;
    //!Implementation of AbstractSocket::setReuseAddrFlag
    virtual bool setReuseAddrFlag(bool reuseAddr) override;
    //!Implementation of AbstractSocket::reuseAddrFlag
    virtual bool getReuseAddrFlag(bool* val) const override;
    //!Implementation of AbstractSocket::setNonBlockingMode
    virtual bool setNonBlockingMode(bool val) override;
    //!Implementation of AbstractSocket::getNonBlockingMode
    virtual bool getNonBlockingMode(bool* val) const override;
    //!Implementation of AbstractSocket::getMtu
    virtual bool getMtu(unsigned int* mtuValue) const override;
    //!Implementation of AbstractSocket::setSendBufferSize
    virtual bool setSendBufferSize(unsigned int buffSize) override;
    //!Implementation of AbstractSocket::getSendBufferSize
    virtual bool getSendBufferSize(unsigned int* buffSize) const override;
    //!Implementation of AbstractSocket::setRecvBufferSize
    virtual bool setRecvBufferSize(unsigned int buffSize) override;
    //!Implementation of AbstractSocket::getRecvBufferSize
    virtual bool getRecvBufferSize(unsigned int* buffSize) const override;
    //!Implementation of AbstractSocket::setRecvTimeout
    virtual bool setRecvTimeout(unsigned int ms) override;
    //!Implementation of AbstractSocket::getRecvTimeout
    virtual bool getRecvTimeout(unsigned int* millis) const override;
    //!Implementation of AbstractSocket::setSendTimeout
    virtual bool setSendTimeout(unsigned int ms) override;
    //!Implementation of AbstractSocket::getSendTimeout
    virtual bool getSendTimeout(unsigned int* millis) const override;
    //!Implementation of AbstractSocket::getLastError
    virtual bool getLastError(SystemError::ErrorCode* errorCode) const override;
    //!Implementation of AbstractSocket::handle
    virtual AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of SSLServerSocket::listen
    virtual bool listen(int queueLen) override;
    //!Implementation of SSLServerSocket::accept
    virtual AbstractStreamSocket* accept() override;
    //!Implementation of SSLServerSocket::cancelAsyncIO
    virtual void cancelAsyncIO(bool waitForRunningHandlerCompletion) override;
    //!Implementation of AbstractSocket::terminateAsyncIO
    virtual void terminateAsyncIO( bool waitForRunningHandlerCompletion ) override;

protected:
    //!Implementation of AbstractSocket::postImpl
    virtual bool postImpl( std::function<void()>&& handler ) override;
    //!Implementation of AbstractSocket::dispatchImpl
    virtual bool dispatchImpl( std::function<void()>&& handler ) override;
    //!Implementation of SSLServerSocket::acceptAsyncImpl
    virtual bool acceptAsyncImpl(std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)>&& handler) override;

private:
    std::vector<AbstractStreamServerSocket*> m_socketDelegates;
    std::vector<bool> m_acceptingFlags;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    std::queue<std::pair<SystemError::ErrorCode, AbstractStreamSocket*> > m_queue;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_asyncHandler;
    std::atomic<bool> m_inSyncAccept;

    void connectionAcceptedSync(
        AbstractStreamServerSocket* serverSocket,
        size_t serverSocketIndex,
        SystemError::ErrorCode errorCode,
        AbstractStreamSocket* newConnection );
    void connectionAcceptedAsync(
        AbstractStreamServerSocket* serverSocket,
        size_t serverSocketIndex,
        SystemError::ErrorCode errorCode,
        AbstractStreamSocket* newConnection );
};

#endif  //MIXED_TCP_UDT_SERVER_SOCKET_H
