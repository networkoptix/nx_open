/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#include "gzip_uncompressor.h"


static const int OUTPUT_BUFFER_SIZE = 16*1024;

GZipUncompressor::GZipUncompressor()
:
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
                //TODO/IMPL does inflate always eat all input stream?
                zResult = inflate(&m_zStream, Z_NO_FLUSH);
                if( zResult == Z_OK || zResult == Z_STREAM_END )
                {
                    if( zResult == Z_STREAM_END )
                        m_state = State::done;

                    if( m_zStream.avail_out == 0 )
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
                    else
                    {
                        continue;
                    }

                    break;
                }
                else 
                {
                    m_state = State::failed;
                    break;
                }
                break;

            case State::failed:
            case State::done:
                break;

            default:
                assert( false );
        }

        break;
    }
}

void GZipUncompressor::flush()
{
    m_zStream.next_in = 0;
    m_zStream.avail_in = 0;
    m_zStream.next_out = (Bytef*)m_outputBuffer.data();
    m_zStream.avail_out = (uInt)m_outputBuffer.size();

    int zResult = inflate(&m_zStream, Z_SYNC_FLUSH);
    m_nextFilter->processData( QnByteArrayConstRef(m_outputBuffer, 0, m_outputBuffer.size()-m_zStream.avail_out) );
    //TODO/IMPL
}
