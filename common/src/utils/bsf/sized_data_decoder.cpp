/**********************************************************
* 26 may 2015
* a.kolesnikov
***********************************************************/

#include "sized_data_decoder.h"


namespace nx_bsf
{
    bool SizedDataDecodingFilter::processData( const QnByteArrayConstRef& data )
    {
        //TODO #ak make streaming implementation

        QnByteArrayConstRef::size_type pos = 0;
        for( ; pos < data.size(); )
        {
            uint32_t dataSize = 0;
            if( data.size() - pos < sizeof(dataSize) )
                return false;
            memcpy( &dataSize, data.constData()+pos, sizeof(dataSize) );
            pos += sizeof(dataSize);
            dataSize = ntohl( dataSize );

            if( data.size() - pos < dataSize )
                return false;
            if( !nextFilter()->processData( data.mid( pos, dataSize ) ) )
                return false;
            pos += dataSize;
        }

        return pos == data.size();
    }
}
