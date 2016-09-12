/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_buffer_source_h
#define libcommon_buffer_source_h

#include "abstract_msg_body_source.h"


namespace nx_http
{
    class NX_NETWORK_API BufferSource
    :
        public AbstractMsgBodySource
    {
    public:
        BufferSource(
            StringType mimeType,
            BufferType msgBody);
        ~BufferSource();

        virtual void stopWhileInAioThread() override;

        //!Implementation of AbstractMsgBodySource::mimeType
        virtual StringType mimeType() const override;
        //!Implementation of AbstractMsgBodySource::contentLength
        virtual boost::optional<uint64_t> contentLength() const override;
        //!Implementation of AbstractMsgBodySource::readAsync
        virtual void readAsync(
            nx::utils::MoveOnlyFunc<
                void(SystemError::ErrorCode, BufferType)
            > completionHandler) override;

    private:
        const StringType m_mimeType;
        BufferType m_msgBody;
    };
}

#endif  //libcommon_buffer_source_h
