/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#include "gzip_uncompressor.h"


static const int OUTPUT_BUFFER_SIZE = 16*1024;

GZipUncompressor::GZipUncompressor( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter )
:
    AbstractByteStreamConverter( nextFilter ),
    m_state( State::init )
{
    memset( &m_zStream, 0, sizeof(m_zStream) );

    m_outputBuffer.resize( OUTPUT_BUFFER_SIZE );
}

GZipUncompressor::~GZipUncompressor()
{
    inflateEnd( &m_zStream );
}

void GZipUncompressor::processData( const QnByteArrayConstRef& data )
{
    if( data.isEmpty() )
        return;

    int zFlushMode = Z_NO_FLUSH;

    m_zStream.next_in = (Bytef*)data.constData();
    m_zStream.avail_in = (uInt)data.size();
    m_zStream.next_out = (Bytef*)m_outputBuffer.data();
    m_zStream.avail_out = (uInt)m_outputBuffer.size();

    for( ;; )
    {
        int zResult = Z_OK;
        switch( m_state )
        {
            case State::init:
                zResult = inflateInit2(&m_zStream, 16+MAX_WBITS);
                if( zResult != Z_OK )
                {
                    assert( false );
                }
                m_state = State::inProgress;

            case State::inProgress:
            {
                //NOTE assuming that inflate always eat all input stream if output buffer is available
                const uInt availInBak = m_zStream.avail_in;
                zResult = inflate(&m_zStream, zFlushMode);
                const uInt inBytesConsumed = availInBak - m_zStream.avail_in;
                switch( zResult )
                {
                    case Z_OK:
                    case Z_STREAM_END:
                    {
                        if( zResult == Z_STREAM_END )
                            m_state = State::done;

                        if( (m_zStream.avail_out == 0) && (m_zStream.avail_in > 0) )
                        {
                            //setting new output buffer and calling inflate once again
                            m_nextFilter->processData( m_outputBuffer );
                            m_zStream.next_out = (Bytef*)m_outputBuffer.data();
                            m_zStream.avail_out = (uInt)m_outputBuffer.size();
                            continue;
                        }
                        else if( m_zStream.avail_in == 0 )
                        {
                            //input depleted
                            m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
                            return;
                        }
                        else    //m_zStream.avail_out > 0 && m_zStream.avail_in > 0
                        {
                            if( m_zStream.avail_out < (unsigned int)m_outputBuffer.size() )
                            {
                                m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
                                m_zStream.next_out = (Bytef*)m_outputBuffer.data();
                                m_zStream.avail_out = (uInt)m_outputBuffer.size();
                            }
                            else if( inBytesConsumed == 0 ) //&& m_zStream.avail_out == m_outputBuffer.size()
                            {
                                //zlib does not consume any input data and does not write anything to output
                                m_state = State::failed;    //to avoid inifinite loop
                            }
                            continue;
                        }

                        break;
                    }

                    case Z_BUF_ERROR:
                        //this error is recoverable
                        if( m_zStream.avail_in > 0 )
                        {
                            //may be some more out buf is required?
                            if( m_zStream.avail_out < (unsigned int)m_outputBuffer.size() )
                            {
                                m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
                                m_zStream.next_out = (Bytef*)m_outputBuffer.data();
                                m_zStream.avail_out = (uInt)m_outputBuffer.size();
                                continue;   //trying with more output buffer
                            }
                            else if( zFlushMode == Z_NO_FLUSH )
                            {
                                //trying inflate with Z_SYNC_FLUSH
                                zFlushMode = Z_SYNC_FLUSH;
                                continue;
                            }
                            else
                            {
                                //TODO/IMPL we SHOULD cache left input data and give it to inflate along with next input data portion
                            }
                        }
                        else    //m_zStream.avail_in == 0
                        {
                            if( m_zStream.avail_out > 0 )
                               m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
                            return;
                        }

                    default:
                        m_state = State::failed;
                        break;
                }
                break;
            }

            case State::failed:
            case State::done:
                break;

            default:
                assert( false );
        }

        break;
    }
}

size_t GZipUncompressor::flush()
{
    m_zStream.next_in = 0;
    m_zStream.avail_in = 0;
    m_zStream.next_out = (Bytef*)m_outputBuffer.data();
    m_zStream.avail_out = (uInt)m_outputBuffer.size();

    int zResult = inflate(&m_zStream, Z_SYNC_FLUSH);
    if( (zResult == Z_OK || zResult == Z_STREAM_END) && ((unsigned int)m_outputBuffer.size() > m_zStream.avail_out) )
    {
        m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
        return m_outputBuffer.size() - m_zStream.avail_out;
    }
    return 0;
}
