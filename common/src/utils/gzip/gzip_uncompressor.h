/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#ifndef GZIP_UNCOMPRESSOR_H
#define GZIP_UNCOMPRESSOR_H

#ifdef Q_OS_MACX
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

#include "../media/abstract_byte_stream_filter.h"


//!Deflates gzip-compressed stream. Suitable for decoding gzip http content encoding
class GZipUncompressor
:
    public AbstractByteStreamConverter
{
public:
    GZipUncompressor( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = std::shared_ptr<AbstractByteStreamFilter>() );
    virtual ~GZipUncompressor();

    //!Implementation of \a AbstractByteStreamFilter::processData
    virtual void processData( const QnByteArrayConstRef& data ) override;
    //!Implementation of \a AbstractByteStreamFilter::flush
    virtual size_t flush() override;

private:
    // TODO: #Elric #enum
    enum class State
    {
        init,
        inProgress,
        done,
        failed
    };

    State m_state;
    z_stream m_zStream;
    QByteArray m_outputBuffer;
};

#endif   //GZIP_UNCOMPRESSOR_H
