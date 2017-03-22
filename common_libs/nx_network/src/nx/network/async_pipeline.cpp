#include "async_pipeline.h"

namespace nx {
namespace network {

//class AbstractAsynchronousReaderWrapper
//{
//public:
//    virtual ~AbstractAsynchronousReaderWrapper() = default;
//
//    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) = 0;
//    virtual void readSomeAsync(
//        nx::Buffer* const buffer,
//        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;
//    virtual void sendAsync(
//        const nx::Buffer& buffer,
//        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;
//    virtual void cancelIOSync(nx::network::aio::EventType eventType) = 0;
//};
//
//template<typename ActualReaderPtr>
//class AsynchronousReaderWrapper:
//    public AbstractAsynchronousReaderWrapper
//{
//public:
//    AsynchronousReaderWrapper(ActualReaderPtr reader):
//        m_reader(std::move(reader))
//    {
//    }
//
//    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
//    {
//        m_reader->bindToAioThread(aioThread);
//    }
//
//    virtual void readSomeAsync(
//        nx::Buffer* const buffer,
//        std::function<void(SystemError::ErrorCode, size_t)> handler) override
//    {
//        m_reader->readSomeAsync(buffer, std::move(handler));
//    }
//
//    virtual void sendAsync(
//        const nx::Buffer& buffer,
//        std::function<void(SystemError::ErrorCode, size_t)> handler) override
//    {
//        m_reader->sendAsync(buffer, std::move(handler));
//    }
//
//    virtual void cancelIOSync(nx::network::aio::EventType eventType) override
//    {
//        m_reader->cancelIOSync(eventType);
//    }
//
//private:
//    ActualReaderPtr m_reader;
//};

//AsyncPipeline::~AsyncPipeline()
//{
//    if (isInSelfAioThread())
//        stopWhileInAioThread();
//}
//
//void AsyncPipeline::stopWhileInAioThread()
//{
//}

} // namespace network
} // namespace nx
