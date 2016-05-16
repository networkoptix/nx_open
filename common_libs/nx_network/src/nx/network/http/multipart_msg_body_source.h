/**********************************************************
* May 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include "abstract_msg_body_source.h"

#include "nx/network/aio/timer.h"

#include "multipart_body_serializer.h"


namespace nx_http {

    /**
        Used to generate and stream Http multipart message body
    */
    class NX_NETWORK_API MultipartMessageBodySource
    :
        public AbstractMsgBodySource
    {
    public:
        MultipartMessageBodySource(StringType boundary);
        virtual ~MultipartMessageBodySource();

        virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

        virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
        virtual void bindToAioThread(
            nx::network::aio::AbstractAioThread* aioThread) override;
        virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
        virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

        virtual StringType mimeType() const override;
        virtual boost::optional<uint64_t> contentLength() const override;
        virtual void readAsync(
            nx::utils::MoveOnlyFunc<
                void(SystemError::ErrorCode, BufferType)
            > completionHandler) override;

        void setOnBeforeDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler);

        MultipartBodySerializer* serializer();

    private:
        nx::network::aio::Timer m_aioThreadBinder;
        MultipartBodySerializer m_multipartBodySerializer;
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)
        > m_readCompletionHandler;
        BufferType m_dataBuffer;
        nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
        bool m_eof;

        void onSomeDataAvailable(const QnByteArrayConstRef& data);
        void doCleanup();
    };

}   //namespace nx_http
