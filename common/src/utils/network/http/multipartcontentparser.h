/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef MULTIPARTCONTENTPARSER_H
#define MULTIPARTCONTENTPARSER_H

#include "httptypes.h"

#include "linesplitter.h"


namespace nx_http
{
    /*!
        Parses multipart http content (http://en.wikipedia.org/wiki/MIME#Multipart_messages)
    */
    class MultipartContentParser
    {
    public:
        enum State
        {
            readingBoundary,
            skippingCurrentLine,
            readingHeaders,
            readingData
        };

        enum ResultCode
        {
            needMoreData,
            headersAvailable,
            someDataAvailable,
            partDataDone,
            eof,
            parseError
        };

        MultipartContentParser( const StringType& boundary = "boundary" );
        ~MultipartContentParser();

        //!Parses maximum \a count bytes of \a data starting at \a offset
        /*!
            Reads maximum one content part
        */
        ResultCode parseBytes(
            const ConstBufferRefType& data,
            size_t* bytesProcessed );
        //!
        /*!
            \return Content data region in previous \a data supplied to \a parseBytes method
        */
        ConstBufferRefType prevFoundData() const;
        /*!
            \note Only valid after \a parseBytes returned \a partRead. Next \a parseBytes call can invalidate it
        */
        StringType partContentType() const;
        /*!
            \note Only valid after \a parseBytes returned \a partRead. Next \a parseBytes call can invalidate it
        */
        const HttpHeaders& partHeaders() const;
        void setBoundary( const StringType& boundary );

    private:
        StringType m_boundary;
        StringType m_startBoundaryLine;
        StringType m_endBoundaryLine;
        HttpHeaders m_headers;
        StringType m_partContentType;
        State m_state;
        BufferType m_searchBoundaryCache;
        LineSplitter m_lineSplitter;
        ConstBufferRefType m_prevFoundData;
    };
}

#endif  //MULTIPARTCONTENTPARSER_H
