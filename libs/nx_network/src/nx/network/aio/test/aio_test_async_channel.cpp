#include "aio_test_async_channel.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

AsyncChannel::AsyncChannel(
    utils::bstream::AbstractInput* input,
    utils::bstream::AbstractOutput* output,
    InputDepletionPolicy inputDepletionPolicy)
    :
    m_input(input),
    m_output(output),
    m_inputDepletionPolicy(inputDepletionPolicy),
    m_totalBytesRead(0),
    m_readPaused(false),
    m_readBuffer(nullptr),
    m_sendPaused(false),
    m_sendBuffer(nullptr),
    m_readScheduled(false),
    m_readSequence(0),
    m_readErrorsReported(0),
    m_sendErrorsReported(0)
{
    bindToAioThread(getAioThread());
}

AsyncChannel::~AsyncChannel()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void AsyncChannel::bindToAioThread(AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_reader.bindToAioThread(aioThread);
    m_writer.bindToAioThread(aioThread);
}

void AsyncChannel::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    NX_ASSERT(buffer->capacity() > buffer->size());

    QnMutexLocker lock(&m_mutex);
    m_readHandler = std::move(handler);
    m_readBuffer = buffer;
    if (m_readPaused)
        return;

    performAsyncRead(lock);
}

void AsyncChannel::sendAsync(
    const nx::Buffer& buffer,
    IoCompletionHandler handler)
{
    QnMutexLocker lock(&m_mutex);

    m_sendHandler = std::move(handler);
    m_sendBuffer = &buffer;
    if (m_sendPaused)
        return;

    performAsyncSend(lock);
}

void AsyncChannel::cancelIoInAioThread(EventType eventType)
{
    if (eventType == EventType::etRead ||
        eventType == EventType::etNone)
    {
        m_reader.cancelPostedCallsSync();
        m_readScheduled = false;
        m_readHandler = nullptr;
        m_readBuffer = nullptr;
    }

    if (eventType == EventType::etWrite ||
        eventType == EventType::etNone)
    {
        m_writer.cancelPostedCallsSync();
        m_sendHandler = nullptr;
        m_sendBuffer = nullptr;
    }
}

void AsyncChannel::pauseReadingData()
{
    QnMutexLocker lock(&m_mutex);
    m_readPaused = true;
}

void AsyncChannel::resumeReadingData()
{
    QnMutexLocker lock(&m_mutex);
    m_readPaused = false;
    if (m_readHandler)
        performAsyncRead(lock);
}

void AsyncChannel::pauseSendingData()
{
    QnMutexLocker lock(&m_mutex);
    m_sendPaused = true;
}

void AsyncChannel::resumeSendingData()
{
    QnMutexLocker lock(&m_mutex);
    m_sendPaused = false;
    if (m_sendHandler)
        performAsyncSend(lock);
}

void AsyncChannel::waitForSomeDataToBeRead()
{
    auto totalBytesReadBak = m_totalBytesRead.load();
    while (totalBytesReadBak == m_totalBytesRead)
        std::this_thread::yield();
}

void AsyncChannel::waitForReadSequenceToBreak()
{
    auto readSequenceBak = m_readSequence.load();
    while (readSequenceBak == m_readSequence)
        std::this_thread::yield();
}

QByteArray AsyncChannel::dataRead() const
{
    QnMutexLocker lock(&m_mutex);
    return m_totalDataRead;
}

void AsyncChannel::setErrorState()
{
    QnMutexLocker lock(&m_mutex);
    m_readErrorState = std::make_tuple(SystemError::connectionReset, (size_t) -1);
    m_sendErrorState = std::make_tuple(SystemError::connectionReset, (size_t) -1);
}

void AsyncChannel::setSendErrorState(std::optional<IoState> sendErrorCode)
{
    QnMutexLocker lock(&m_mutex);
    m_sendErrorState = sendErrorCode;
}

void AsyncChannel::setSendErrorState(SystemError::ErrorCode errorCode)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_sendErrorState = std::make_tuple(errorCode, (size_t) -1);
}

void AsyncChannel::setReadErrorState(std::optional<IoState> sendErrorCode)
{
    QnMutexLocker lock(&m_mutex);
    m_readErrorState = sendErrorCode;
}

void AsyncChannel::setReadErrorState(SystemError::ErrorCode errorCode)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_readErrorState = std::make_tuple(errorCode, (size_t) -1);
}

void AsyncChannel::waitForAnotherReadErrorReported()
{
    const auto readErrorsReported = m_readErrorsReported.load();
    while (m_readErrorsReported == readErrorsReported)
        std::this_thread::yield();
}

void AsyncChannel::waitForAnotherSendErrorReported()
{
    const auto sendErrorsReported = m_sendErrorsReported.load();
    while (m_sendErrorsReported == sendErrorsReported)
        std::this_thread::yield();
}

bool AsyncChannel::isReadScheduled() const
{
    QnMutexLocker lock(&m_mutex);
    return m_readScheduled;
}

bool AsyncChannel::isWriteScheduled() const
{
    QnMutexLocker lock(&m_mutex);
    return m_sendHandler != nullptr;
}

void AsyncChannel::stopWhileInAioThread()
{
    m_reader.pleaseStopSync();
    m_writer.pleaseStopSync();
}

void AsyncChannel::handleInputDepletion()
{
    switch (m_inputDepletionPolicy)
    {
        case InputDepletionPolicy::sendConnectionReset:
            m_readBuffer = nullptr;
            reportIoCompletion(&m_readHandler, SystemError::connectionReset, (size_t)-1);
            break;

        case InputDepletionPolicy::ignore:
            break;

        case InputDepletionPolicy::retry:
        {
            QnMutexLocker lock(&m_mutex);
            performAsyncRead(lock);
            break;
        }
    }
}

void AsyncChannel::performAsyncRead(const QnMutexLockerBase& /*lock*/)
{
    m_readScheduled = true;

    m_reader.post(
        [this]()
        {
            std::optional<IoState> readErrorState;
            {
                QnMutexLocker lock(&m_mutex);
                readErrorState = m_readErrorState;
            }

            if (readErrorState)
            {
                ++m_readErrorsReported;
                reportIoCompletion(
                    &m_readHandler,
                    std::get<0>(*readErrorState),
                    std::get<1>(*readErrorState));
                return;
            }

            int bytesRead = m_input->read(
                m_readBuffer->data() + m_readBuffer->size(),
                m_readBuffer->capacity() - m_readBuffer->size());
            if (bytesRead > 0)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_totalDataRead.append(m_readBuffer->data() + m_readBuffer->size(), bytesRead);
                    m_totalBytesRead += bytesRead;
                }
                m_readBuffer->resize(m_readBuffer->size() + bytesRead);
                m_readBuffer = nullptr;

                nx::utils::ObjectDestructionFlag::Watcher thisWatcher(&m_destructionFlag);
                reportIoCompletion(&m_readHandler, SystemError::noError, bytesRead);
                if (thisWatcher.objectDestroyed())
                    return;

                if (!m_readScheduled)
                    ++m_readSequence;
                return;
            }

            if (bytesRead == utils::bstream::StreamIoError::osError)
            {
                reportIoCompletion(&m_readHandler, SystemError::connectionReset, (size_t)-1);
                return;
            }

            handleInputDepletion();
        });
}

void AsyncChannel::reportIoCompletion(
    IoCompletionHandler* ioCompletionHandler,
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesTransferred)
{
    if (ioCompletionHandler == &m_readHandler)
        m_readScheduled = false;

    nx::utils::swapAndCall(*ioCompletionHandler, sysErrorCode, bytesTransferred);
}

void AsyncChannel::performAsyncSend(const QnMutexLockerBase&)
{
    auto buffer = m_sendBuffer;
    m_sendBuffer = nullptr;

    m_writer.post(
        [this, buffer]()
        {
            decltype(m_sendHandler) handler;
            {
                QnMutexLocker lock(&m_mutex);
                m_sendHandler.swap(handler);
            }

            std::optional<IoState> sendErrorState;
            {
                QnMutexLocker lock(&m_mutex);
                sendErrorState = m_sendErrorState;
            }
            if (sendErrorState)
            {
                std::apply(handler, *sendErrorState);
                ++m_sendErrorsReported;
                return;
            }

            int bytesWritten = m_output->write(buffer->constData(), buffer->size());
            handler(
                bytesWritten >= 0 ? SystemError::noError : SystemError::connectionReset,
                bytesWritten);
        });
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
