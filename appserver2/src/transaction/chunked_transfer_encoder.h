#ifndef __CHUNKED_TRANSFER_ENCODER_H_
#define __CHUNKED_TRANSFER_ENCODER_H_

namespace ec2
{
    /**
     * This class encodes data into a standard-compliant byte array.
     */
    class QnChunkedTransferEncoder {
    public:
        static QByteArray serializedTransaction(const QByteArray &data);
    };

}

#endif // __CHUNKED_TRANSFER_ENCODER_H_
