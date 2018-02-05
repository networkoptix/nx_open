
#include "ssl_socket.h"

#ifdef ENABLE_SSL
#include <mutex>
#include <errno.h>
#include <list>
#include <memory>
#include <atomic>
#include <thread>

#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>

#include <QDir>

#include <nx/network/socket_global.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/system_error.h>

#include "ssl/ssl_static_data.h"

#ifdef max
    #undef max
#endif

#ifdef min
    #undef min
#endif

static const std::size_t kSslAsyncRecvBufferSize(1024 * 64);

namespace nx {
namespace network {
namespace deprecated {

class SslAsyncOperation
{
public:
    enum
    {
        PENDING,
        EXCEPTION,
        SUCCESS,
        END_OF_STREAM
    };

    virtual void perform(int* sslReturn, int* sslError) = 0;

    void increasePendingIOCount()
    {
        ++m_pendingIoCount;
    }

    void decreasePendingIOCount()
    {
        NX_ASSERT(m_pendingIoCount > 0);
        --m_pendingIoCount;
        if (m_pendingIoCount == 0) {
            // Check if we can invoke the IO operations, if the IO
            // status is not PENDING, then we can invoke such async
            // SSL operation's user callback function here
            if (m_exitStatus != PENDING)
                invokeUserCallback();
        }
    }

    void setExitStatus(int exitStatus, SystemError::ErrorCode errorCode)
    {
        if (m_exitStatus == PENDING) {
            m_exitStatus = exitStatus;
            m_errorCode = errorCode;
            if (m_pendingIoCount == 0) {
                invokeUserCallback();
            }
        }
    }

    SslAsyncOperation(SSL* ssl) :
        m_pendingIoCount(0),
        m_exitStatus(PENDING),
        m_errorCode(SystemError::noError),
        m_ssl(ssl)
    {}

    virtual ~SslAsyncOperation() {}

protected:
    virtual void invokeUserCallback() = 0;

    void reset()
    {
        m_pendingIoCount = 0;
        m_exitStatus = PENDING;
        m_errorCode = SystemError::noError;
    }

    int m_pendingIoCount;
    int m_exitStatus;
    SystemError::ErrorCode m_errorCode;
    SSL* m_ssl;
};

class SslAsyncRead : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        int old_size = m_readBuffer->size();
        int read_size = m_readBuffer->capacity() - old_size;
        m_readBuffer->resize(m_readBuffer->capacity());
        *sslReturn = SSL_read(m_ssl,m_readBuffer->data()+old_size,read_size);
        *sslError = SSL_get_error(m_ssl,*sslReturn);
        if (*sslReturn<=0) {
            m_readBuffer->resize(old_size);
        } else {
            m_readBuffer->resize(old_size+*sslReturn);
            m_readBytes += *sslReturn;
        }

        NX_VERBOSE(this, lm("return %1, error %2").arg(*sslReturn).arg(*sslError));
    }

    void reset(
        nx::Buffer* buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)>&& handler)
    {
        m_readBuffer = buffer;
        m_handler = std::move(handler);
        m_readBytes = 0;
        SslAsyncOperation::reset();
    }

    SslAsyncRead(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        NX_ASSERT(handler != nullptr);
        NX_VERBOSE(this, lm("invokeUserCallback, status: %1").arg(m_errorCode));
        switch(m_exitStatus)
        {
            case SslAsyncOperation::EXCEPTION:
                return handler(m_errorCode, -1);
            case SslAsyncOperation::SUCCESS:
                return handler(SystemError::noError, m_readBytes);
            case SslAsyncOperation::END_OF_STREAM:
                NX_ASSERT(m_readBytes == 0);
                return handler(SystemError::noError, 0);
            default:
                NX_ASSERT(false);
        }
    }

private:
    nx::Buffer* m_readBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)> m_handler;
    std::size_t m_readBytes;
};

class SslAsyncWrite : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        NX_ASSERT(!m_writeBuffer->isEmpty());
        *sslReturn = SSL_write(
            m_ssl,m_writeBuffer->constData(),m_writeBuffer->size());
        *sslError = SSL_get_error(m_ssl,*sslReturn);

        NX_VERBOSE(this, lm("return %1, error %2").arg(*sslReturn).arg(*sslError));
    }

    void reset(
        const nx::Buffer* buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)>&& handler)
    {
        m_writeBuffer = buffer;
        m_handler = std::move(handler);
        SslAsyncOperation::reset();
    }

    SslAsyncWrite(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        NX_VERBOSE(this, lm("invokeUserCallback, status: %1").arg(m_errorCode));
        switch(m_exitStatus)
        {
            case SslAsyncOperation::EXCEPTION:
                return handler(SystemError::connectionAbort, -1);
            case SslAsyncOperation::END_OF_STREAM:
                return handler(SystemError::noError, 0);
            case SslAsyncOperation::SUCCESS:
                return handler(SystemError::noError, m_writeBuffer->size());
            default:
                NX_ASSERT(false);
        }
    }

private:
    const nx::Buffer* m_writeBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)> m_handler;
};

class SslAsyncHandshake : public SslAsyncOperation
{
public:
    virtual void perform(int* sslReturn, int* sslError)
    {
        *sslReturn = SSL_do_handshake(m_ssl);
        *sslError = SSL_get_error(m_ssl,*sslReturn);

        NX_VERBOSE(this, lm("return %1, error %2").arg(*sslReturn).arg(*sslError));
    }

    void reset(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>&& handler)
    {
        m_handler = std::move(handler);
        SslAsyncOperation::reset();
    }

    SslAsyncHandshake(SSL* ssl) : SslAsyncOperation(ssl) {}

protected:
    virtual void invokeUserCallback()
    {
        const auto handler = std::move(m_handler);
        m_handler = nullptr;

        NX_VERBOSE(this, lm("ivokeUserCallback, status: %1").arg(m_errorCode));
        switch (m_exitStatus)
        {
            case SslAsyncOperation::EXCEPTION:
                // In case of SSL-error we do not have system error code,
                // but connection is still broken.
                return handler(m_errorCode == SystemError::noError
                    ? SystemError::connectionAbort
                    : m_errorCode);

            case SslAsyncOperation::END_OF_STREAM:
                return handler(SystemError::connectionReset);

            case SslAsyncOperation::SUCCESS:
                return handler(SystemError::noError);

            default:
                NX_ASSERT(false);
        }
    }

private:
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_handler;
};

class SslAsyncBioHelper
{
public:
    SslAsyncBioHelper(
        SSL* ssl, AbstractStreamSocket* underly_socket, bool is_server);

    virtual ~SslAsyncBioHelper();

    virtual bool isSsl() const { return true; }

public:
    // BIO operation function. These 2 BIO operation function will simulate BIO memory
    // type. For read, it will simply checks the input read buffer, and for write operations
    // it will write the data into the write buffer and then the upper will detect what
    // needs to send out. This style makes the code easier and simpler and also since we
    // have PendingIOCount, all the user callback function will be invoked in proper order
    std::size_t bioWrite(const void* data, std::size_t size);
    std::size_t bioRead(void* data, std::size_t size);

    bool eof() const {
        return m_eof;
    }

    void asyncSend(
        const nx::Buffer& buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler);

    void asyncRecv(
        nx::Buffer* buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler);

    void clear()
    {
        NX_VERBOSE(this, lm("Clear read/write queues"));
        m_readQueue.clear();
        m_writeQueue.clear();
    }

private:
    void asyncPerform(SslAsyncOperation* operation);

    // Perform the corresponding SSL operations that is outstanding here.
    // Since all the operation will be serialized inside of a single thread.
    // We can simply pass all the parameter all around as a function parameter.
    void perform(SslAsyncOperation* operation);

    // Check whether SSL has been shutdown or not for this situations.
    void checkShutdown(int sslReturn, int sslError);

    // Handle SSL internal error
    void handleSslError(int sslReturn, int sslError);

    // These 2 functions will issue the read/write operations directly to the underlying
    // socket. It will use context variable outstanding_read_/outstanding_write_ operations
    void enqueueRead(SslAsyncOperation* operation);
    void doRead();
    void doWrite();
    void enqueueWrite();
    void doHandshake(SslAsyncOperation* next_op);

    void onRecv(SystemError::ErrorCode errorCode, std::size_t transferred);
    void onSend(SystemError::ErrorCode errorCode, std::size_t transferred);
    void onConnect(SystemError::ErrorCode errorCode);

    void continueRead();
    void continueWrite();

protected:
    AbstractStreamSocket* socket()
    {
        return m_underlySocket;
    }

    void injectSnifferData(const nx::Buffer& buffer)
    {
        m_recvBuffer.append(buffer);
    }

    enum
    {
        HANDSHAKE_NOT_YET,
        HANDSHAKE_PENDING,
        HANDSHAKE_DONE
    };

    void setHandshakeStage(int handshake_stage)
    {
        m_handshakeStage = handshake_stage;
    }

private:
    // Operations
    std::unique_ptr<SslAsyncRead> m_read;
    std::unique_ptr<SslAsyncWrite> m_write;
    std::unique_ptr<SslAsyncHandshake> m_handshake;

    // Handshake
    std::list<SslAsyncOperation*> m_handshakeQueue;

    // Read
    bool m_allowBioRead;
    nx::Buffer m_recvBuffer;
    std::size_t m_recvBufferReadPos;
    std::list<SslAsyncOperation*> m_readQueue;
    SslAsyncOperation* m_outstandingRead;

    // Write
    struct PendingWrite
    {
        nx::Buffer writeBuffer;
        SslAsyncOperation* operation;

        PendingWrite(PendingWrite&& write)
        {
            writeBuffer = std::move(write.writeBuffer);
            operation = write.operation;
        }

        PendingWrite():operation(NULL){}

        bool needWrite() const
        {
            return !writeBuffer.isEmpty();
        }

        void reset(SslAsyncOperation* op) {
            operation = op;
            writeBuffer.clear();
        }
    };

    std::list<PendingWrite> m_writeQueue;
    PendingWrite* m_outstandingWrite;
    PendingWrite m_currentWrite;

    // SSL related
    SSL* m_ssl;
    bool m_eof;
    bool m_isServer;
    int m_handshakeStage;
    AbstractStreamSocket* m_underlySocket;

    // This class is used to notify the caller that the user has deleted the
    // object. Since it is very likely that the invocation will spawn a chain
    // of member function in AsyncSSL, eg: OnUnderlySocketRecv --> sendAsync
    // --> OnUnderlySocketSend --> delete the object, for each function, there
    // will be a such class on the stack, however, the inner most nested class
    // will get notification, since this class set the deletion_flag at last.
    // This will result in the outer caller not get any notification afterwards.
    // A simpler and elegant way should use std::shared_ptr/std::weak_ptr, however
    // since our code has performance issue, we'd like to invent some easier way
    // to do so. This class will cascade the deletion information to the out most
    // callee until no more DeletionFlag is used then. The mechanism is simple,it
    // will check the AsyncSSL's deletion_flag_ pointer, if it is not NULL, which
    // means a DelegionFlag on stack is watching for the deletion operations, so
    // it will cache this pointer and set it to the corresponding value, by this
    // means we are able to cascade the deletion operation internally.
    class DeletionFlag
    {
    public:
        DeletionFlag(SslAsyncBioHelper* ssl) :
            m_flag(false),
            m_ssl(ssl)
        {
            m_previousFlag = ssl->m_deletionFlag;
            ssl->m_deletionFlag = &m_flag;
        }
        ~DeletionFlag()
        {
            if (!m_flag) {
                // Restoring the previous flag of this objects
                m_ssl->m_deletionFlag = m_previousFlag;
            } else {
                if (m_previousFlag != NULL)
                    *m_previousFlag = true;
            }
        }

        operator bool () const
        {
            return m_flag;
        }

    private:
        bool m_flag;
        bool* m_previousFlag;
        SslAsyncBioHelper* m_ssl;
    };
    bool* m_deletionFlag;
    friend class DeletionFlag;
};

SslAsyncBioHelper::SslAsyncBioHelper(
        SSL* ssl, AbstractStreamSocket* underly_socket, bool is_server)
:
    m_read(new SslAsyncRead(ssl)),
    m_write(new SslAsyncWrite(ssl)),
    m_handshake(new SslAsyncHandshake(ssl)),
    m_allowBioRead(false),
    m_recvBufferReadPos(0),
    m_ssl(ssl),
    m_eof(false),
    m_isServer(is_server),
    m_handshakeStage(HANDSHAKE_NOT_YET),
    m_underlySocket(underly_socket),
    m_deletionFlag(NULL)
{
        m_recvBuffer.reserve(static_cast<int>(kSslAsyncRecvBufferSize));
}

SslAsyncBioHelper::~SslAsyncBioHelper()
{
    if (m_deletionFlag != NULL) {
        *m_deletionFlag = true;
    }
}

void SslAsyncBioHelper::perform(SslAsyncOperation* operation)
{
    int sslReturn, sslError ;
    // Setting up the write buffer here
    m_currentWrite.reset(operation);
    // Perform the underlying SSL operations
    operation->perform(&sslReturn,&sslError);
    // Check the EOF/SHUTDOWN status of the ssl
    checkShutdown(sslReturn,sslError);
    // Handle existed write here
    if (m_currentWrite.needWrite()) {
        enqueueWrite();
    }

    switch(sslError) {
    case SSL_ERROR_NONE:
        // The SSL operation has been finished here
        operation->setExitStatus(
                SslAsyncOperation::SUCCESS,SystemError::noError);
        break;
    case SSL_ERROR_WANT_READ:
        enqueueRead(operation);
        break;
    case SSL_ERROR_WANT_WRITE:
        break;
    default:
        handleSslError(sslReturn,sslError);
        if (eof() && sslError == SSL_ERROR_SYSCALL) {
            // For end of the file, we just tell them that we are done here
            operation->setExitStatus(
                SslAsyncOperation::END_OF_STREAM, SystemError::noError);
        } else {
            operation->setExitStatus(
                SslAsyncOperation::EXCEPTION, SystemError::connectionAbort);
        }
        break;
    }
}

void SslAsyncBioHelper::checkShutdown(int sslReturn, int sslError)
{
    Q_UNUSED(sslReturn);
    if (SSL_get_shutdown(m_ssl) == SSL_RECEIVED_SHUTDOWN ||
        sslError == SSL_ERROR_ZERO_RETURN) {
            // This should be the normal shutdown which means the
            // peer at least call SSL_shutdown once
            NX_VERBOSE(this, "normal shutdown");
            m_eof = true;
    } else if (sslError == SSL_ERROR_SYSCALL) {
        // Brute shutdown for SSL connection
        if (ERR_get_error() == 0) {
            NX_VERBOSE(this, "brute shutdown");
            m_eof = true;
        }
    }
}

const char* SSLErrorStr(int sslError)
{
    switch(sslError)
    {
        case SSL_ERROR_SSL: return "SSL_ERROR_SSL";
        case SSL_ERROR_SYSCALL: return "SSL_ERROR_SYSCALL";
        default: return "<ssl not a real error>";
    }
}

void SslAsyncBioHelper::handleSslError(int sslReturn, int sslError)
{
    QStringList errorStack;
    while (int err = ERR_get_error())
    {
        char err_str[1024];
        ERR_error_string_n(err, err_str, 1024);
        errorStack << QLatin1String(err_str);
    }

    NX_VERBOSE(this, lm("SSL returns %1, error %2, stack: %3")
        .args(sslReturn, sslError).container(errorStack));
}

void SslAsyncBioHelper::onRecv(
    SystemError::ErrorCode errorCode, std::size_t transferred)
{
    NX_VERBOSE(this, lm("transport read %1: %2").args(transferred, SystemError::toString(errorCode)));
    if (m_readQueue.empty()) return;
    if (errorCode != SystemError::noError) {
        DeletionFlag deleted(this);
        m_outstandingRead->setExitStatus(SslAsyncOperation::EXCEPTION,errorCode);
        if (!deleted) {
            m_outstandingRead->decreasePendingIOCount();
            if (!deleted)
                continueRead();
        }
        return;
    } else {
        if (transferred == 0) {
            m_eof = true;
        }
        // Since we gonna invoke user's callback here, a deletion flag will us
        // to avoid reuse member object once the user deleted such object
        DeletionFlag deleted(this);
        // Set up the flag to let the user runs into the read operation's returned buffer
        m_allowBioRead = true;
        perform(m_outstandingRead);
        if (!deleted) {
            m_outstandingRead->decreasePendingIOCount();
            if (!deleted) {
                m_allowBioRead = false;
                continueRead();
            }
        }
    }
}

void SslAsyncBioHelper::enqueueRead(SslAsyncOperation* operation)
{
    m_readQueue.push_back(operation);
    NX_VERBOSE(this, lm("there are %1 reads in queue").arg(m_readQueue.size()));
    if (m_readQueue.size() == 1) {
        m_outstandingRead = m_readQueue.front();
        doRead();
    }
}

void SslAsyncBioHelper::continueRead()
{
    if (m_readQueue.empty()) return;
    m_readQueue.pop_front();
    if (!m_readQueue.empty()) {
        m_outstandingRead = m_readQueue.front();
        doRead();
    }
}

void SslAsyncBioHelper::doRead()
{
    // Setting up the outstanding read here
    m_outstandingRead = m_readQueue.front();
    // Checking if we have some data lefts inside of the read buffer, if so
    // we could just call SSL operation right here. And since this function
    // is executed inside of AIO thread, no recursive lock will happened in
    // user's callback function.
    if (static_cast<int>(m_recvBufferReadPos) < m_recvBuffer.size()) {
        m_outstandingRead->increasePendingIOCount();
        onRecv(SystemError::noError, m_recvBuffer.size() - m_recvBufferReadPos);
    } else {
        // We have to issue our operations right just here since we have no
        // left data inside of the buffer and our user needs to read all the
        // data out in the buffer here.
        NX_VERBOSE(this, lm("transport try read some"));
        m_outstandingRead->increasePendingIOCount();
        m_underlySocket->readSomeAsync(
            &m_recvBuffer,
            std::bind(
            &SslAsyncBioHelper::onRecv,
            this,
            std::placeholders::_1,
            std::placeholders::_2));
    }
}

void SslAsyncBioHelper::onSend(
        SystemError::ErrorCode errorCode,
        std::size_t transferred)
{
    NX_VERBOSE(this, lm("transport sent %1: %2").args(transferred, SystemError::toString(errorCode)));
    Q_UNUSED(transferred);
    if (m_writeQueue.empty()) return;
    if (errorCode != SystemError::noError) {
        DeletionFlag deleted(this);
        m_outstandingWrite->operation->setExitStatus(
                SslAsyncOperation::EXCEPTION, errorCode);
        if (!deleted) {
            m_outstandingWrite->operation->decreasePendingIOCount();
            if (!deleted)
                continueWrite();
        }
        return;
    } else {
        // For read operations, if it succeeded, we don't need to call Perform
        // since this operation has been go ahead for that SSL_related operations.
        // What we need to do is just decrease user's pending IO count value.
        DeletionFlag deleted(this);
        m_outstandingWrite->operation->decreasePendingIOCount();
        if (!deleted) {
            continueWrite();
        }
    }
}

void SslAsyncBioHelper::doWrite()
{
    NX_VERBOSE(this, lm("transport try send %1").arg(m_outstandingWrite->writeBuffer.size()));
    m_outstandingWrite->operation->increasePendingIOCount();
    m_underlySocket->sendAsync(
        m_outstandingWrite->writeBuffer,
        std::bind(
        &SslAsyncBioHelper::onSend,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
}

void SslAsyncBioHelper::enqueueWrite()
{
    m_writeQueue.push_back(std::move(m_currentWrite));
    if (m_writeQueue.size() == 1) {
        m_outstandingWrite = &(m_writeQueue.front());
        doWrite();
    }
}

void SslAsyncBioHelper::continueWrite()
{
    if (m_writeQueue.empty()) return;
    m_writeQueue.pop_front();
    if (!m_writeQueue.empty()) {
        m_outstandingWrite = &(m_writeQueue.front());
        doWrite();
    }
}

std::size_t SslAsyncBioHelper::bioRead(void* data, std::size_t size)
{
    if (m_allowBioRead) {
        if (m_recvBuffer.size() == static_cast<int>(m_recvBufferReadPos)) {
            return 0;
        } else {
            std::size_t digest_size =
                std::min(size,
                    static_cast<int>(m_recvBuffer.size()) - m_recvBufferReadPos);
            memcpy(data,
                   m_recvBuffer.constData() + m_recvBufferReadPos,
                   digest_size);
            m_recvBufferReadPos += digest_size;
            if (static_cast<int>(m_recvBufferReadPos) == m_recvBuffer.size()) {
                m_recvBufferReadPos = 0;
                m_recvBuffer.clear();
                m_recvBuffer.reserve(static_cast<int>(kSslAsyncRecvBufferSize));
            }
            return digest_size;
        }
    }
    return 0;
}

std::size_t SslAsyncBioHelper::bioWrite(const void* data, std::size_t size)
{
    m_currentWrite.writeBuffer.append(
        static_cast<const char*>(data), static_cast<int>(size));

    return size;
}

void SslAsyncBioHelper::onConnect(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError) {
        m_handshakeStage = HANDSHAKE_NOT_YET;
        // No, we cannot connect to the peer sides, so we just tell our user
        // that we have expired whatever we've got currently
        for (auto op : m_handshakeQueue) {
            DeletionFlag deleted(this);
            op->setExitStatus(SslAsyncOperation::EXCEPTION, errorCode);
            if (deleted)
                return;
        }
        m_handshakeQueue.clear();
    } else {
        m_handshakeStage = HANDSHAKE_DONE;
        for (auto op : m_handshakeQueue) {
            DeletionFlag deleted(this);
            perform(op);
            if (deleted)
                return;
        }
        m_handshakeQueue.clear();
    }
}

void SslAsyncBioHelper::doHandshake(SslAsyncOperation* next_op)
{
    m_handshakeQueue.push_back(next_op);
    if (m_isServer) {
        SSL_set_accept_state(m_ssl);
    } else {
        SSL_set_connect_state(m_ssl);
    }
    m_handshake->reset(
        std::bind(
        &SslAsyncBioHelper::onConnect,
        this,
        std::placeholders::_1));
    perform(m_handshake.get());
}

void SslAsyncBioHelper::asyncPerform(SslAsyncOperation* operation)
{
    switch(m_handshakeStage) {
        case HANDSHAKE_NOT_YET:
            m_handshakeStage = HANDSHAKE_PENDING;
            doHandshake(operation);
            break;
        case HANDSHAKE_PENDING:
            m_handshakeQueue.push_back(operation);
            break;
        case HANDSHAKE_DONE:
            perform(operation);
            break;
        default:
            NX_ASSERT(0);
            break;
    }
}

void SslAsyncBioHelper::asyncSend(
    const nx::Buffer& buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    m_write->reset(&buffer, std::move(handler));
    asyncPerform(m_write.get());
}

void SslAsyncBioHelper::asyncRecv(
    nx::Buffer* buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    m_read->reset(buffer, std::move(handler));
    asyncPerform(m_read.get());
}

class MixedSslAsyncBioHelper : public SslAsyncBioHelper
{
public:
    bool isSsl() const override { return m_isSsl; }

    struct SnifferData
    {
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)> completionHandler;
        nx::Buffer* buffer;

        SnifferData(
            nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)>&& completionHandler,
            nx::Buffer* buf)
        :
            completionHandler(std::move(completionHandler)),
            buffer(buf)
        {}
    };

public:
    static const int kSnifferDataHeaderLength = 2;
    static const int kSnifferBufferLength = 512;

    MixedSslAsyncBioHelper(SSL* ssl, AbstractStreamSocket* socket)
        :SslAsyncBioHelper(ssl,socket,true),
        m_isInitialized(false),
        m_isSsl(false)
    {}

    void asyncSend(
        const nx::Buffer& buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)>&& op)
    {
        // We have to receive some data at first to be able to understand if this connection
        // is secure or not by analyzing client's request.
        NX_ASSERT(m_isInitialized);
        NX_ASSERT(m_isSsl);
        SslAsyncBioHelper::asyncSend(buffer,std::move(op));
    }

    void asyncRecv(
        nx::Buffer* buffer,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode,std::size_t)>&& completionHandler)
    {
        if (!m_isInitialized) {
            m_snifferBuffer.reserve(kSnifferBufferLength);
            socket()->readSomeAsync(
                &m_snifferBuffer,
                [this, data = SnifferData(std::move(completionHandler), buffer)](
                    SystemError::ErrorCode systemErrorCode, std::size_t bytesRead) mutable
                {
                    Sniffer(systemErrorCode, bytesRead, std::move(data));
                });
        } else {
            NX_ASSERT(m_isSsl);
            SslAsyncBioHelper::asyncRecv(buffer,std::move(completionHandler));
        }
    }

    // When blocking version detects it is an SSL, it has to notify me
    void set_ssl(bool ssl)
    {
        m_isInitialized = true;
        m_isSsl = ssl;
        SslAsyncBioHelper::setHandshakeStage(SslAsyncBioHelper::HANDSHAKE_DONE);
    }

    bool is_initialized() const
    {
        return m_isInitialized;
    }

    bool is_ssl() const
    {
        return m_isSsl;
    }

private:
    void Sniffer(
        SystemError::ErrorCode ec,
        std::size_t transferred,
        SnifferData data)
    {
        NX_VERBOSE(this, lm("Transport read %1: %2").args(transferred, SystemError::toString(ec)));
        // We have the data in our buffer right now
        if (ec) {
            data.completionHandler(ec,0);
            return;
        } else {
            if (transferred == 0) {
                data.completionHandler(ec,0);
                return;
            } else if (m_snifferBuffer.size() < kSnifferDataHeaderLength) {
                socket()->readSomeAsync(
                    &m_snifferBuffer,
                    [this, data = std::move(data)](
                        SystemError::ErrorCode systemErrorCode, std::size_t bytesRead) mutable
                    {
                        Sniffer(systemErrorCode, bytesRead, std::move(data));
                    });
                return;
            }
            // Fix for the bug that always false in terms of comparison of 0x80
            const unsigned char* buf = reinterpret_cast<unsigned char*>(
                m_snifferBuffer.data());

            if (buf[0] == 0x80 || (buf[0] == 0x16 && buf[1] == 0x03)) {
                m_isSsl = true;
                m_isInitialized = true;
            } else {
                m_isSsl = false;
                m_isInitialized = true;
            }

            // If it is an SSL we still need to continue our async operation
            // otherwise we call the failed callback for the upper usage class
            // it should be MixedSslSocket class
            if (m_isSsl) {
                SslAsyncBioHelper::injectSnifferData(m_snifferBuffer);
                m_snifferBuffer.clear();
                SslAsyncBioHelper::asyncRecv(data.buffer, std::move(data.completionHandler));
            } else {
                data.buffer->append(m_snifferBuffer);
                m_snifferBuffer.clear();
                data.completionHandler(SystemError::noError, data.buffer->size());
            }
        }
    }

private:
    bool m_isInitialized;
    bool m_isSsl;
    nx::Buffer m_snifferBuffer;

};

class SslSocketPrivate
{
public:
    typedef utils::promise<std::pair<SystemError::ErrorCode, size_t>> AsyncPromise;

    std::unique_ptr<AbstractStreamSocket> wrappedSocket;
    std::unique_ptr<SSL, decltype(&SSL_free)> ssl;
    bool isServerSide;
    quint8 extraBuffer[32];
    int extraBufferLen;
    //!Socket works as regular socket (without encryption until this flag is set to true)
    bool encryptionEnabled;
    std::atomic<bool> nonBlockingMode;
    std::unique_ptr<SslAsyncBioHelper> asyncSslHelper;

    QnMutex syncIoMutex;
    bool isShutdown;
    std::atomic<AsyncPromise*> syncRecvPromise;
    std::atomic<AsyncPromise*> syncSendPromise;

    std::atomic<int> bioReadFlags;
    std::atomic<bool> isSendInProgress;
    std::atomic<bool> isRecvInProgress;

    SslSocketPrivate():
        ssl(nullptr, SSL_free),
        isServerSide(false),
        extraBufferLen(0),
        encryptionEnabled(false),
        nonBlockingMode(false),
        isShutdown(false),
        syncRecvPromise(nullptr),
        syncSendPromise(nullptr),
        bioReadFlags(0),
        isSendInProgress(false),
        isRecvInProgress(false)
    {
    }
};

SslSocket::SslSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket,
    bool isServerSide, bool encriptionEnforced)
    :
    SslSocket(
        new SslSocketPrivate(),
        std::move(wrappedSocket),
        isServerSide,
        encriptionEnforced)
{
    Q_D(SslSocket);
    d->asyncSslHelper.reset(new SslAsyncBioHelper(
        d->ssl.get(), d->wrappedSocket.get(), isServerSide));
}

SslSocket::SslSocket(
    SslSocketPrivate* priv,
    std::unique_ptr<AbstractStreamSocket> wrappedSocket,
    bool isServerSide,
    bool encriptionEnforced)
:
    base_type(
        [this]()
        {
            Q_D(SslSocket);
            return d->wrappedSocket.get();
        }),
    d_ptr(priv)
{
    Q_D(SslSocket);

    d->wrappedSocket = std::move(wrappedSocket);
    d->isServerSide = isServerSide;
    d->extraBufferLen = 0;
    d->encryptionEnabled = encriptionEnforced;
    initSsl();
}

void SslSocket::initSsl()
{
    Q_D(SslSocket);
    static BIO_METHOD kBioMethods =
    {
        BIO_TYPE_SOCKET,
        "proxy server socket",
        bioWrite,
        bioRead,
        bioPuts,
        NULL, // bgets
        bioCtrl,
        bioNew,
        bioFree,
        NULL, // callback_ctrl
    };

    BIO* rbio = BIO_new(&kBioMethods);
    BIO_set_nbio(rbio, 1);
    BIO_set_app_data(rbio, this);

    BIO* wbio = BIO_new(&kBioMethods);
    BIO_set_app_data(wbio, this);
    BIO_set_nbio(wbio, 1);

    auto context = d->isServerSide
        ? ssl::SslStaticData::instance()->serverContext()
        : ssl::SslStaticData::instance()->clientContext();

    NX_ASSERT(context);
    d->ssl.reset(SSL_new(context)); // get new SSL state with context

    SSL_set_verify(d->ssl.get(), SSL_VERIFY_NONE, NULL);
    SSL_set_session_id_context(
        d->ssl.get(),
        reinterpret_cast<const unsigned char*>(
            ssl::SslStaticData::instance()->sslSessionId().constData()),
        ssl::SslStaticData::instance()->sslSessionId().size());

    SSL_set_bio(d->ssl.get(), rbio, wbio);  //d->ssl will free bio when freed
}

bool SslSocket::initializeUnderlyingSocketIfNeeded()
{
    Q_D(SslSocket);

    if (m_isUnderlyingSocketInitialized)
        return true;

    // NOTE: Currently all I/O operations are implemented over async because current SSL
    //     implementation is too twisted to support runtime mode switches.
    // TODO: SSL sockets should be reimplemented to simplify overall approach and support
    //     runtime mode switches.
    bool nonBlockingMode = false;
    if (!d->wrappedSocket->getNonBlockingMode(&nonBlockingMode))
        return false;

    d->nonBlockingMode = nonBlockingMode;
    if (!nonBlockingMode)
    {
        if (!d->wrappedSocket->setNonBlockingMode(true))
            return false;
    }

    m_isUnderlyingSocketInitialized = true;

    return true;
}

int SslSocket::bioRead(BIO* b, char* out, int outl)
{
    SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(b));
    int ret = sslSock->asyncRecvInternal(out, outl);
    if (ret == -1)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_read(b);
    }

    return ret;
}

int SslSocket::bioWrite(BIO* b, const char* in, int inl)
{
    SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(b));
    int ret = sslSock->asyncSendInternal(in, inl);
    if (ret == -1)
    {
        BIO_clear_retry_flags(b);
        BIO_set_retry_write(b);
    }

    return ret;
}

int SslSocket::bioPuts(BIO* bio, const char* str)
{
    return bioWrite(bio, str, strlen(str));
}

long SslSocket::bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/)
{
    long ret = 1;

    switch (cmd)
    {
        case BIO_C_SET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_C_GET_FD:
            NX_ASSERT(false, "Invalid proxy socket use!");
            break;
        case BIO_CTRL_GET_CLOSE:
            ret = bio->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            bio->shutdown = (int)num;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        default:
            ret = 0;
            break;
    }
    return(ret);
}

int SslSocket::bioNew(BIO* bio)
{
    bio->init = 1;
    bio->num = 0;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

int SslSocket::bioFree(BIO* bio)
{
    if (bio == NULL) return(0);
    if (bio->shutdown)
    {
        if (bio->init)
        {
            SslSocket* sslSock = static_cast<SslSocket*>(BIO_get_app_data(bio));
            if (sslSock)
                sslSock->close();
        }

        bio->init = 0;
        bio->flags = 0;
    }

    return 1;
}

SslSocket::~SslSocket()
{
    Q_D(SslSocket);
    if (!d->nonBlockingMode)
        d->wrappedSocket->pleaseStopSync(false);

    delete d_ptr;
}


bool SslSocket::doHandshake()
{
    Q_D(SslSocket);
    if (d->isServerSide)
        SSL_set_accept_state(d->ssl.get());
    else
        SSL_set_connect_state(d->ssl.get());

    int ret = SSL_do_handshake(d->ssl.get());
    if (ret != 1)
    {
        QByteArray e(1024, '\0');
        ERR_error_string_n(SSL_get_error(d->ssl.get(), ret), e.data(), e.size());
        NX_LOGX(lm("Handshake on %1 failed %2: %3")
            .args(d->isServerSide ? "server" : "client").arg(ret).arg(e), cl_logDEBUG1);
    }
    else
    {
        NX_LOGX(lm("Handshake on %1 success %2")
            .args(d->isServerSide ? "server" : "client", ret), cl_logDEBUG2);
    }

    return ret == 1;
}

int SslSocket::recvInternal(void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(SslSocket);
    if (d->extraBufferLen > 0)
    {
        int toReadLen = qMin((int)bufferLen, d->extraBufferLen);
        memcpy(buffer, d->extraBuffer, toReadLen);
        memmove(d->extraBuffer, d->extraBuffer + toReadLen, d->extraBufferLen - toReadLen);
        d->extraBufferLen -= toReadLen;
        int readRest = bufferLen - toReadLen;
        // Does this should be readreset ? -- DPENG
        if (toReadLen > 0) {
            int readed = d->wrappedSocket->recv((char*) buffer + toReadLen, readRest, flags);
            if (readed > 0)
                toReadLen += readed;
        }
        return toReadLen;
    }
    return d->wrappedSocket->recv(buffer, bufferLen, flags);
}

int SslSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    if (!initializeUnderlyingSocketIfNeeded())
        return -1;

    Q_D(SslSocket);
    unsigned int timeout = 0;
    if (d->nonBlockingMode || flags & MSG_DONTWAIT) //< Emulate non-blocking mode by a very small timeout.
    {
        if (!d->wrappedSocket->getRecvTimeout(&timeout))
            return -1;

        if (!d->wrappedSocket->setRecvTimeout(1))
            return -1;
    }

    const auto returnError =
        [d, timeout, flags](SystemError::ErrorCode code) -> int
        {
            if (d->nonBlockingMode || flags & MSG_DONTWAIT)
                d->wrappedSocket->setRecvTimeout(timeout);

            SystemError::setLastErrorCode(code);
            return -1;
        };

    SslSocketPrivate::AsyncPromise promise;
    {
        QnMutexLocker lock(&d->syncIoMutex);
        if (d->isShutdown)
            return returnError(SystemError::notConnected);

        if (d->syncRecvPromise.load())
        {
            NX_ASSERT(false);
            return returnError(SystemError::already);
        }

        d->syncRecvPromise = &promise;
    }

    nx::Buffer localBuffer;
    localBuffer.reserve((int) bufferLen);
    auto setPromise =
        [d](SystemError::ErrorCode code, size_t size)
        {
            d->wrappedSocket->post(
                [d, code, size]()
                {
                    if (auto promisePtr = d->syncRecvPromise.exchange(nullptr))
                        promisePtr->set_value({code, size});
                });
        };

    if (flags & MSG_WAITALL)
        readAsyncAtLeast(&localBuffer, bufferLen, std::move(setPromise));
    else
        readSomeAsync(&localBuffer, std::move(setPromise));

    auto result = promise.get_future().get();

    if (d->nonBlockingMode || flags & MSG_DONTWAIT) //< Emulate non-blocking mode by a very small timeout.
    {
        if (!d->wrappedSocket->setRecvTimeout(timeout))
            return -1;

        if (result.first == SystemError::timedOut)
            result.first = SystemError::wouldBlock;
    }

    if (result.first == SystemError::noError)
    {
        std::memcpy(buffer, localBuffer.data(), result.second);
        return result.second;
    }
    else
    {
        SystemError::setLastErrorCode(result.first);
        return -1;
    }
}

int SslSocket::sendInternal(const void* buffer, unsigned int bufferLen)
{
    Q_D(SslSocket);
    return d->wrappedSocket->send(buffer,bufferLen);
}

int SslSocket::send(const void* buffer, unsigned int bufferLen)
{
    if (!initializeUnderlyingSocketIfNeeded())
        return -1;

    Q_D(SslSocket);
    unsigned int timeout = 0;
    if (d->nonBlockingMode) //< Emulate non-blocking mode by a very small timeout.
    {
        if (!d->wrappedSocket->getSendTimeout(&timeout))
            return -1;

        if (!d->wrappedSocket->setSendTimeout(1))
            return -1;
    }

    const auto returnError =
        [d, timeout](SystemError::ErrorCode code) -> int
        {
            if (d->nonBlockingMode)
                d->wrappedSocket->setSendTimeout(timeout);

            SystemError::setLastErrorCode(code);
            return -1;
        };

    SslSocketPrivate::AsyncPromise promise;
    {
        QnMutexLocker lock(&d->syncIoMutex);
        if (d->isShutdown)
            return returnError(SystemError::notConnected);

        if (d->syncSendPromise.load())
        {
            NX_ASSERT(false);
            return returnError(SystemError::already);
        }

        d->syncSendPromise = &promise;
    }

    nx::Buffer localBuffer(static_cast<const char*>(buffer), bufferLen);
    sendAsync(
        localBuffer,
        [d](SystemError::ErrorCode code, size_t size)
        {
            d->wrappedSocket->post(
                [d, code, size]()
                {
                    if (auto promisePtr = d->syncSendPromise.exchange(nullptr))
                        promisePtr->set_value({code, size});
                });
        });

    auto result = promise.get_future().get();

    if (d->nonBlockingMode) //< Emulate non-blocking mode by a very small timeout.
    {
        if (!d->wrappedSocket->setSendTimeout(timeout))
            return -1;

        if (result.first  == SystemError::timedOut)
            result.first  = SystemError::wouldBlock;
    }

    if (result.first != SystemError::noError)
        SystemError::setLastErrorCode(result.first);

    return result.second;
}

bool SslSocket::setNoDelay(bool value)
{
    Q_D(SslSocket);
    return d->wrappedSocket->setNoDelay(value);
}

bool SslSocket::getNoDelay(bool* value) const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getNoDelay(value);
}

bool SslSocket::toggleStatisticsCollection(bool val)
{
    Q_D(SslSocket);
    return d->wrappedSocket->toggleStatisticsCollection(val);
}

bool SslSocket::getConnectionStatistics(StreamSocketInfo* info)
{
    Q_D(SslSocket);
    return d->wrappedSocket->getConnectionStatistics(info);
}

bool SslSocket::connect(
    const SocketAddress& remoteAddress,
    std::chrono::milliseconds timeout)
{
    if (!initializeUnderlyingSocketIfNeeded())
        return false;

    Q_D(SslSocket);
    d->encryptionEnabled = true;

    if (!d->nonBlockingMode)
    {
        if (!d->wrappedSocket->setNonBlockingMode(false))
            return false;
    }

    const auto result = d->wrappedSocket->connect(remoteAddress, timeout);

    if (!d->nonBlockingMode && result)
    {
        if (!d->wrappedSocket->setNonBlockingMode(true))
            return false;
    }

    return result;
}

SocketAddress SslSocket::getForeignAddress() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getForeignAddress();
}

QString SslSocket::getForeignHostName() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getForeignHostName();
}

bool SslSocket::isConnected() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->isConnected();
}

bool SslSocket::setKeepAlive(boost::optional< KeepAliveOptions > info)
{
    Q_D(const SslSocket);
    return d->wrappedSocket->setKeepAlive(info);
}

bool SslSocket::getKeepAlive(boost::optional< KeepAliveOptions >* result) const
{
    Q_D(const SslSocket);
    return d->wrappedSocket->getKeepAlive(result);
}

static void cancelIoFromAioThread(SslSocketPrivate* socket, aio::EventType eventType)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
        socket->isSendInProgress = false;
    if (eventType == aio::etRead || eventType == aio::etNone)
        socket->isRecvInProgress = false;

    if (socket->asyncSslHelper)
        socket->asyncSslHelper->clear();
}

void SslSocket::cancelIOAsync(aio::EventType eventType, utils::MoveOnlyFunc<void()> handler)
{
    Q_D(SslSocket);
    d->wrappedSocket->cancelIOAsync(
        eventType,
        [d, eventType, handler = move(handler)]()
        {
            cancelIoFromAioThread(d, eventType);
            handler();
        });
}

void SslSocket::cancelIOSync(nx::network::aio::EventType eventType)
{
    Q_D(SslSocket);
    if (isInSelfAioThread())
    {
        d->wrappedSocket->cancelIOSync(eventType);
        cancelIoFromAioThread(d, eventType);
    }
    else
    {
        utils::BarrierWaiter waiter;
        cancelIOAsync(eventType, waiter.fork());
    }
}

bool SslSocket::setNonBlockingMode(bool value)
{
    if (!initializeUnderlyingSocketIfNeeded())
        return false;

    Q_D(SslSocket);
    d->nonBlockingMode = value;
    return true;
}

bool SslSocket::getNonBlockingMode(bool* value) const
{
    if (!const_cast<SslSocket*>(this)->initializeUnderlyingSocketIfNeeded())
        return false;

    Q_D(const SslSocket);
    *value = d->nonBlockingMode;
    return true;
}

bool SslSocket::shutdown()
{
    Q_D(SslSocket);
    NX_VERBOSE(this, "Shutdown...");
    {
        QnMutexLocker lock(&d->syncIoMutex);
        d->isShutdown = true;
    }

    utils::promise<void> promise;
    d->wrappedSocket->pleaseStop(
        [this, d, &promise]()
        {
            NX_VERBOSE(this, "Shutdown: notify send");
            if (auto promisePtr = d->syncSendPromise.exchange(nullptr))
                promisePtr->set_value({SystemError::interrupted, 0});

            NX_VERBOSE(this, "Shutdown: notify recv");
            if (auto promisePtr = d->syncRecvPromise.exchange(nullptr))
                promisePtr->set_value({SystemError::interrupted, 0});

            NX_LOGX("Socket is shutdown", cl_logDEBUG2);
            promise.set_value();
        });

    promise.get_future().wait();
    return d->wrappedSocket->shutdown();
}

QString SslSocket::idForToStringFromPtr() const
{
    Q_D(const SslSocket);
    return d->wrappedSocket ? d->wrappedSocket->idForToStringFromPtr() : QString();
}

void SslSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    if (!initializeUnderlyingSocketIfNeeded())
    {
        auto sysErrorCode = SystemError::getLastOSErrorCode();
        return post([handler = std::move(handler), sysErrorCode]() { handler(sysErrorCode); });
    }

    Q_D(const SslSocket);
    return d->wrappedSocket->connectAsync(addr, std::move(handler));
}

static bool checkAsyncOperation(
    std::atomic<bool>* isInProgress,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)>* handler,
    AbstractStreamSocket* socket, const char* place)
{
    if (isInProgress->exchange(true))
    {
        NX_ASSERT(false, lm("Socket(%1): %2").arg(socket).arg(place));
        socket->post([handler = std::move(*handler)]{ handler(SystemError::already, (size_t) -1); });
        return false;
    }

    auto wrappedHandler =
        [isInProgress, handler = std::move(*handler)](SystemError::ErrorCode code, size_t size)
        {
            isInProgress->store(false);
            handler(code, size);
        };

    *handler = std::move(wrappedHandler);
    return true;
}

void SslSocket::readSomeAsync(
    nx::Buffer* const buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    if (!initializeUnderlyingSocketIfNeeded())
    {
        auto sysErrorCode = SystemError::getLastOSErrorCode();
        return post(
            [handler = std::move(handler), sysErrorCode]()
            {
                handler(sysErrorCode, (std::size_t)-1);
            });
    }

    Q_D(SslSocket);
    NX_ASSERT(d->nonBlockingMode.load() || d->syncRecvPromise.load());
    if (!checkAsyncOperation(&d->isRecvInProgress, &handler, d->wrappedSocket.get(), "SSL read"))
        return;

    d->wrappedSocket->post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            Q_D(SslSocket);
            if (!d->nonBlockingMode.load() && !d->syncRecvPromise.load())
                return handler(SystemError::invalidData, (size_t) -1);

            d->asyncSslHelper->asyncRecv(buffer, std::move(handler));
        });
}

void SslSocket::sendAsync(
    const nx::Buffer& buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    if (!initializeUnderlyingSocketIfNeeded())
    {
        auto sysErrorCode = SystemError::getLastOSErrorCode();
        return post(
            [handler = std::move(handler), sysErrorCode]()
            {
                handler(sysErrorCode, (std::size_t)-1);
            });
    }

    Q_D(SslSocket);
    NX_ASSERT(d->nonBlockingMode.load() || d->syncSendPromise.load());
    if (!checkAsyncOperation(&d->isSendInProgress, &handler, d->wrappedSocket.get(), "SSL send"))
        return;

    d->wrappedSocket->post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            Q_D(SslSocket);
            if (!d->nonBlockingMode.load() && !d->syncSendPromise.load())
                return handler(SystemError::invalidData, (size_t) -1);

            d->asyncSslHelper->asyncSend(buffer, std::move(handler));
        });
}

int SslSocket::asyncRecvInternal(void* buffer, unsigned int bufferLen)
{
    // For async operation here
    Q_D(SslSocket);
    const auto bioRead = [&]()
    {
        int ret = static_cast<int>(d->asyncSslHelper->bioRead(buffer, bufferLen));
        return ret == 0 ? -1 : ret;
    };

    int ret = d->asyncSslHelper->eof() ? 0 : bioRead();
    NX_VERBOSE(this, lm("BIO read %1 returned %2").arg(bufferLen).arg(ret));
    return ret;
}

int SslSocket::asyncSendInternal(
    const void* buffer,
    unsigned int bufferLen)
{
    Q_D(SslSocket);
    auto ret = static_cast<int>(d->asyncSslHelper->bioWrite(buffer, bufferLen));
    NX_VERBOSE(this, lm("BIO write %1 returned %2").arg(bufferLen).arg(ret));
    return ret;
}

void SslSocket::registerTimer(
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    Q_D(SslSocket);
    return d->wrappedSocket->registerTimer(timeoutMs, std::move(handler));
}

bool SslSocket::isEncryptionEnabled() const
{
    Q_D(const SslSocket);
    return d->encryptionEnabled || (d->asyncSslHelper && d->asyncSslHelper->isSsl());
}

class MixedSslSocketPrivate:
    public SslSocketPrivate
{
public:
    bool initState;
    bool useSSL;

    MixedSslSocketPrivate():
        initState(true),
        useSSL(false)
    {
    }
};

MixedSslSocket::MixedSslSocket(
    std::unique_ptr<AbstractStreamSocket> wrappedSocket)
    :
    SslSocket(new MixedSslSocketPrivate, std::move(wrappedSocket), true, false)
{
    Q_D(MixedSslSocket);
    d->initState = true;
    d->useSSL = false;
    d->asyncSslHelper.reset(new MixedSslAsyncBioHelper(
        d->ssl.get(), d->wrappedSocket.get()));
}

int MixedSslSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    Q_D(MixedSslSocket);
    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    if (helper)
    {
        if (helper->is_initialized() && !helper->is_ssl())
        {
            if (!updateInternalBlockingMode())
                return -1;
            d->asyncSslHelper.reset();
        }
        else
        {
            return SslSocket::recv(buffer, bufferLen, flags);
        }
    }
    return d->wrappedSocket->recv(buffer, bufferLen, flags);
}

int MixedSslSocket::send(const void* buffer, unsigned int bufferLen)
{
    Q_D(MixedSslSocket);
    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    if (helper)
    {
        if (helper->is_initialized() && !helper->is_ssl())
        {
            if (!updateInternalBlockingMode())
                return -1;
            d->asyncSslHelper.reset(); //< not a SSL mode
        }
        else
        {
            return SslSocket::send(buffer, bufferLen);
        }
    }
    return d->wrappedSocket->send(buffer, bufferLen);
}

bool MixedSslSocket::setNonBlockingMode(bool val)
{
    Q_D(MixedSslSocket);
    if (!SslSocket::setNonBlockingMode(val))
        return false;

    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    if (!helper || (helper->is_initialized() && !helper->is_ssl()))
        return updateInternalBlockingMode();

    return true;
}

void MixedSslSocket::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> cancellationDoneHandler)
{
    Q_D(MixedSslSocket);
    if (d->useSSL)
        SslSocket::cancelIOAsync(eventType, std::move(cancellationDoneHandler));
    else
        d->wrappedSocket->cancelIOAsync(eventType, std::move(cancellationDoneHandler));
}

void MixedSslSocket::connectAsync(
    const SocketAddress& addr,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    Q_D(MixedSslSocket);
    if (d->useSSL)
        return SslSocket::connectAsync(addr, std::move(handler));
    else
        return d->wrappedSocket->connectAsync(addr, std::move(handler));
}

void MixedSslSocket::readSomeAsync(
    nx::Buffer* const buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(MixedSslSocket);
    NX_ASSERT(d->nonBlockingMode.load() || d->syncRecvPromise.load());
    if (!checkAsyncOperation(&d->isRecvInProgress, &handler, d->wrappedSocket.get(), "Mixed SSL read"))
        return;

    if (!d->initState && !d->useSSL)
        return d->wrappedSocket->readSomeAsync(buffer, std::move(handler));

    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    if (!helper || (helper->is_initialized() && !helper->is_ssl()))
    {
        d->asyncSslHelper.reset();
        return d->wrappedSocket->readSomeAsync(buffer, std::move(handler));
    }

    d->wrappedSocket->dispatch(
        [d, helper, buffer, handler = std::move(handler)]() mutable
        {
            if (!d->nonBlockingMode.load() && !d->syncRecvPromise.load())
                return handler(SystemError::invalidData, (size_t) -1);

            if (!d->initState)
                helper->set_ssl(d->useSSL);

            if (helper->is_initialized() && !helper->is_ssl())
                return d->wrappedSocket->readSomeAsync(buffer, std::move(handler));

            d->wrappedSocket->post(
                [helper, buffer, handler = std::move(handler)]() mutable
                {
                    helper->asyncRecv(buffer, std::move(handler));
                });
        });
}

void MixedSslSocket::sendAsync(
    const nx::Buffer& buffer,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, std::size_t)> handler)
{
    Q_D(MixedSslSocket);
    NX_ASSERT(d->nonBlockingMode.load() || d->syncSendPromise.load());
    if (!checkAsyncOperation(&d->isSendInProgress, &handler, d->wrappedSocket.get(), "Mixed SSL send"))
        return;

    if (!d->initState && !d->useSSL)
        return d->wrappedSocket->sendAsync(buffer, std::move(handler));

    auto helper = static_cast<MixedSslAsyncBioHelper*>(d->asyncSslHelper.get());
    if (!helper)
        return d->wrappedSocket->sendAsync(buffer, std::move(handler));

    if (!helper->is_initialized())
    {
        NX_ASSERT(false, "SSL server is not supposed to send before recv");
        return d->wrappedSocket->post(
            [handler = std::move(handler)]() { handler(SystemError::notSupported, (size_t) -1); });
    }

    if (!helper->is_ssl())
    {
        d->asyncSslHelper.reset();
        return d->wrappedSocket->sendAsync(buffer, std::move(handler));
    }

    d->wrappedSocket->dispatch(
        [d, helper, &buffer, handler = std::move(handler)]() mutable
        {
            if (!d->nonBlockingMode.load() && !d->syncSendPromise.load())
                return handler(SystemError::invalidData, (size_t) -1);

            if (!d->initState)
                helper->set_ssl(d->useSSL);

            if (!helper->is_ssl())
                return d->wrappedSocket->sendAsync(buffer, std::move(handler));

            d->wrappedSocket->post(
                [helper, &buffer, handler = std::move(handler)]() mutable
                {
                    helper->asyncSend(buffer, std::move(handler));
                });
        });
}

bool MixedSslSocket::updateInternalBlockingMode()
{
    Q_D(MixedSslSocket);
    bool nonBlockingMode;
    if (!d->wrappedSocket->getNonBlockingMode(&nonBlockingMode))
        return false;

    if (d->nonBlockingMode != nonBlockingMode)
    {
        if (!d->wrappedSocket->setNonBlockingMode(d->nonBlockingMode))
            return false;
    }

    return true;
}

SslServerSocket::SslServerSocket(
    std::unique_ptr<AbstractStreamServerSocket> delegateSocket,
    bool allowNonSecureConnect)
:
    base_type([this](){ return m_delegateSocket.get(); }),
    m_allowNonSecureConnect(allowNonSecureConnect),
    m_delegateSocket(std::move(delegateSocket))
{
}

bool SslServerSocket::listen(int queueLen)
{
    return m_delegateSocket->listen(queueLen);
}

AbstractStreamSocket* SslServerSocket::accept()
{
    AbstractStreamSocket* acceptedSock = m_delegateSocket->accept();
    if (!acceptedSock)
        return nullptr;
    if (m_allowNonSecureConnect)
    {
        return new MixedSslSocket(
            std::unique_ptr<AbstractStreamSocket>(acceptedSock));
    }
    else
    {
        return new SslSocket(
            std::unique_ptr<AbstractStreamSocket>(acceptedSock),
            true,
            true);
    }
}

void SslServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    return m_delegateSocket->pleaseStop(std::move(handler));
}

void SslServerSocket::pleaseStopSync(bool assertIfCalledUnderLock)
{
    return m_delegateSocket->pleaseStopSync(assertIfCalledUnderLock);
}

void SslServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    using namespace std::placeholders;
    m_acceptHandler = std::move(handler);
    m_delegateSocket->acceptAsync(
        std::bind(&SslServerSocket::connectionAccepted, this, _1, _2));
}

void SslServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_delegateSocket->cancelIOAsync(std::move(handler));
}

void SslServerSocket::cancelIOSync()
{
    m_delegateSocket->cancelIOSync();
}

void SslServerSocket::connectionAccepted(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractStreamSocket> newSocket)
{
    if (newSocket)
    {
        if (m_allowNonSecureConnect)
            newSocket = std::make_unique<MixedSslSocket>(std::move(newSocket));
        else
            newSocket = std::make_unique<SslSocket>(std::move(newSocket), true, true);
    }

    auto handler = std::move(m_acceptHandler);
    handler(errorCode, std::move(newSocket));
}

} // namespace deprecated
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
