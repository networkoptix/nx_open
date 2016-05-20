/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_abstract_msg_body_source_h
#define libcommon_abstract_msg_body_source_h

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <utils/common/systemerror.h>

#include "httptypes.h"
#include "../aio/abstract_pollable.h"


namespace nx_http
{
    /**
        If \a AbstractMsgBodySource::contentLength returns existing value then exactly 
            specified number of bytes is fetched using \a AbstractMsgBodySource::readAsync.
        Otherwise, data is fetched until empty buffer is received or error occurs
    */
    class NX_NETWORK_API AbstractMsgBodySource
    :
        public nx::network::aio::AbstractPollable
    {
    public:
        virtual ~AbstractMsgBodySource() {}

        //TODO #ak introduce convenient type for MIME type
        /** Returns full MIME type of content. E.g., application/octet-stream */
        virtual StringType mimeType() const = 0;
        /** Returns length of content, provided by this data source.
            MUST be non-blocking and have constant complexity!
        */
        virtual boost::optional<uint64_t> contentLength() const = 0;
        /**
            \note \a completionHandler can be invoked from within this call
            \note End-of-data is signalled with (SystemError::noError, {empty buffer})
        */
        virtual void readAsync(
            nx::utils::MoveOnlyFunc<
                void(SystemError::ErrorCode, BufferType)
            > completionHandler) = 0;
    };
}

#endif  //libcommon_abstract_msg_body_source_h
