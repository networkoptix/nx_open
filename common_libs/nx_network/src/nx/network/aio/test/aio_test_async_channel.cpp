#include "aio_test_async_channel.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

AsyncChannel::AsyncChannel(
    utils::pipeline::AbstractInput* input,
    utils::pipeline::AbstractOutput* output,
    InputDepletionPolicy inputDepletionPolicy)
    :
    m_input(input),
    m_output(output),
    m_inputDepletionPolicy(inputDepletionPolicy),
    m_errorState(false),
    m_totalBytesRead(0),
    m_readPaused(false),
    m_readBuffer(nullptr),
    m_sendPaused(false),
    m_sendBuffer(nullptr),
    m_readPosted(false),
    m_readSequence(0)
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
    std::function<void(SystemError::ErrorCode, size_t)> handler)
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
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    QnMutexLocker lock(&m_mutex);

    m_sendHandler = std::move(handler);
    m_sendBuffer = &buffer;
    if (m_sendPaused)
        return;

    performAsyncSend(lock);
}

void AsyncChannel::cancelIOSync(EventType eventType)
{
    if (eventType == EventType::etRead ||
        eventType == EventType::etNone)
    {
        m_reader.pleaseStopSync();
    }

    if (eventType == EventType::etWrite ||
        eventType == EventType::etNone)
    {
        m_writer.pleaseStopSync();
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
    m_errorState = true;
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
    m_readPosted = true;

    m_reader.post(
        [this]()
        {
            m_readPosted = false;

            if (m_errorState)
                return reportIoCompletion(&m_readHandler, SystemError::connectionReset, (size_t)-1);

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

                reportIoCompletion(&m_readHandler, SystemError::noError, bytesRead);

                if (!m_readPosted)
                    ++m_readSequence;
                return;
            }

            if (bytesRead == utils::pipeline::StreamIoError::osError)
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
    IoCompletionHandler handler;
    handler.swap(*ioCompletionHandler);
    handler(sysErrorCode, bytesTransferred);
}

void AsyncChannel::performAsyncSend(const QnMutexLockerBase&)
{
    decltype(m_sendHandler) handler;
    m_sendHandler.swap(handler);
    auto buffer = m_sendBuffer;
    m_sendBuffer = nullptr;

    m_writer.post(
        [this, buffer, handler = std::move(handler)]()
        {
            if (m_errorState)
            {
                handler(SystemError::connectionReset, (size_t)-1);
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
