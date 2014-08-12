#ifndef __CHUNKED_TRANSFER_ENCODER_H_
#define __CHUNKED_TRANSFER_ENCODER_H_

#include <vector>

#include <utils/network/http/httptypes.h>


namespace ec2
{
    /**
     * This class encodes data into a standard-compliant byte array.
     */
    class QnChunkedTransferEncoder {
    public:
        static QByteArray serializedTransaction(
            const QByteArray& data,
            const std::vector<nx_http::ChunkExtension>& chunkExtensions );
    };

}

#endif // __CHUNKED_TRANSFER_ENCODER_H_
