/**********************************************************
* 26 may 2015
* a.kolesnikov
***********************************************************/

#ifndef SIZED_DATA_DECODER_H
#define SIZED_DATA_DECODER_H

#include <utils/media/abstract_byte_stream_filter.h>


namespace nx_bsf
{
    /*!
        Decodes stream of following blocks:\n
        - 4 bytes. size in network byte order
        - n bytes of data

        Next filter receives one block per \a SizedDataDecodingFilter::processData call
    */
    class SizedDataDecodingFilter
    :
        public AbstractByteStreamFilter
    {
    public:
        //!Implementation of \a AbstractByteStreamFilter::processData
        virtual bool processData( const QnByteArrayConstRef& data ) override;
    };
}

#endif  //SIZED_DATA_DECODER_H
