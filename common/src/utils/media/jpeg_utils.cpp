/**********************************************************
* 27 may 2014
* a.kolesnikov
***********************************************************/

#include "jpeg_utils.h"


namespace nx_jpg
{
    static const int MARKER_HEADER_SIZE = 2;
    static const int MARKER_LENGTH_SIZE = 2;

    QString toString( ResultCode resCode )
    {
        switch( resCode )
        {
            case succeeded:
                return lit("succeeded");
            case interrupted:
                return lit("interrupted");
            case badData:
                return lit("badData");
            case failed:
                return lit("failed");
            default:
                return lit("unknownError");
        }
    }

    static bool markerHasBody( int markerType )
    {
        return    !((markerType >= 0xD0 && markerType <= 0xD9)   //RSTn, SOI, EOI
                 || (markerType == 0x01)                         //TEM
                 );
    }

    JpegParser::JpegParser()
    :
        m_state( waitingStartOfMarker ),
        m_currentMarkerCode( 0 )
    {
    }

    ResultCode JpegParser::parse( const quint8* data, size_t size )
    {
        const quint8* dataEnd = data + size;
        for( const quint8* pos = data; pos < dataEnd; )
        {
            switch( m_state )
            {
                case waitingStartOfMarker:
                    if( *pos == 0xff )
                        m_state = readingMarkerCode;
                    ++pos;
                    continue;

                case readingMarkerCode:
                    m_currentMarkerCode = *pos;
                    ++pos;
                    if( markerHasBody(m_currentMarkerCode) )
                    {
                        m_state = readingMarkerLength;
                    }
                    else
                    {
                        if( m_parseHandler )
                            if( !m_parseHandler( m_currentMarkerCode, 0, pos - data - MARKER_HEADER_SIZE ) )
                            {
                                if( m_errorHandler )
                                    m_errorHandler( interrupted, pos - data - MARKER_HEADER_SIZE );
                                return interrupted;
                            }
                        m_state = waitingStartOfMarker;
                    }
                    continue;

                case readingMarkerLength:
                {
                    if( pos + 1 >= dataEnd )
                    {
                        if( m_errorHandler )
                            m_errorHandler( badData, pos - data - MARKER_HEADER_SIZE );
                        return badData;   //error
                    }
                    const size_t markerLength = (*pos << 8) | *(pos+1);
                    if( m_parseHandler )
                        if( !m_parseHandler( m_currentMarkerCode, markerLength, pos - data - MARKER_HEADER_SIZE ) )
                        {
                            if( m_errorHandler )
                                m_errorHandler( interrupted, pos - data - MARKER_HEADER_SIZE );
                            return interrupted;
                        }
                    //skipping marker body
                    pos += markerLength;
                    m_state = waitingStartOfMarker;
                    break;
                }

                default:
                    assert( false );
            }
        }
        return succeeded;
    }
    
    bool readJpegImageInfo( const quint8* data, size_t size, ImageInfo* const imgInfo )
    {
        auto parseHandler = [data, size, imgInfo]( int markerType, size_t markerLength, size_t currentOffset ) -> bool
        {
            static const size_t MIN_SOF_SIZE = 5;
            if( !(markerType == 0xC0 || markerType == 0xC2) )
                return true;   //not SOF marker, continue parsing
            if( (markerLength < MARKER_LENGTH_SIZE + MIN_SOF_SIZE) ||       //marker size not enough to read data. marker not recognized?
                (currentOffset + MARKER_HEADER_SIZE + MARKER_LENGTH_SIZE + MIN_SOF_SIZE >= size) )  //not enough data in source buffer
            {
                return false;
            }
            const quint8* sofDataStart = data + currentOffset + MARKER_HEADER_SIZE + MARKER_LENGTH_SIZE;
            imgInfo->precision = sofDataStart[0];
            imgInfo->height = (sofDataStart[1] << 8) | sofDataStart[2];
            imgInfo->width = (sofDataStart[3] << 8) | sofDataStart[4];
            return false;
        };

        JpegParser parser;
        parser.setParseHandler( parseHandler );
        ResultCode rc = parser.parse( data, size );
        return rc == succeeded || rc == interrupted;
    }
}
