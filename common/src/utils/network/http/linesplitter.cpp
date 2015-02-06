/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#include "linesplitter.h"

#include <algorithm>


namespace nx_http
{
    LineSplitter::LineSplitter()
    :
        m_clearCurrentLineBuf( false ),
        m_prevLineEnding( 0 )
    {
    }

    LineSplitter::~LineSplitter()
    {
    }

    /*!
        If source buffer \a data contains whole line then reference to \a data is returned, otherwise - data is cached and returned reference to internal buffer.
        \a *lineBuffer, \a lineBufferOffset, \a lineLen are used to return found line.
        \a *lineBuffer can be pointer to \a data or to an internal buffer
    */
    bool LineSplitter::parseByLines(
        const ConstBufferRefType& data,
        QnByteArrayConstRef* const lineBuffer,
        size_t* const bytesRead )
    {
        if( bytesRead )
            *bytesRead = 0;

        if( m_clearCurrentLineBuf )
        {
            m_currentLine.clear();
            m_clearCurrentLineBuf = false;
        }

        //searching line end in data
        //const size_t lineEnd = data.find_first_of( "\r\n", currentDataPos );
        static const char CRLF[] = "\r\n";
        const char* lineEnd = std::find_first_of( data.data(), data.data()+data.size(), CRLF, CRLF+sizeof(CRLF)-1 );
        if( lineEnd == data.data()+data.size() )
        {
            //not found, caching input data
            m_currentLine.append( data.data(), (int) data.size() );
            if( bytesRead )
                *bytesRead += data.size();
            return false;
        }

        if( (m_prevLineEnding == '\r') && (*lineEnd == '\n') && (lineEnd == data.data()) )    //found LF just after CR
        {
            //reading trailing line ending
            m_prevLineEnding = *lineEnd;
            if( bytesRead )
                *bytesRead += 1;
            return false;
        }

        //line feed found
        if( m_currentLine.isEmpty() )
        {
            //current line not cached, we're able to return reference to input data
            *lineBuffer = data.mid( 0, lineEnd-data.data() );
        }
        else
        {
            m_currentLine.append( data.data(), lineEnd-data.data() );
            *lineBuffer = QnByteArrayConstRef( m_currentLine );
            m_clearCurrentLineBuf = true;
        }
        m_prevLineEnding = *lineEnd;

        //TODO #ak skip \n in case when it comes with next buffer
        if( *lineEnd == '\r' && (lineEnd+1) < data.data()+data.size() && *(lineEnd+1) == '\n' )
            ++lineEnd;

        //currentDataPos = lineEnd + 1;
        if( bytesRead )
            *bytesRead += lineEnd-data.data() + 1;
        return true;
    }

    void LineSplitter::finishCurrentLineEnding(
        const ConstBufferRefType& data,
        size_t* const bytesRead )
    {
        if( bytesRead )
            *bytesRead = 0;
        if( data.isEmpty() )
            return;

        //trying to read trailing line ending
        if( (m_prevLineEnding == '\r') && (data[0] == '\n') )    //found LF just after CR
        {
            //reading trailing line ending
            m_prevLineEnding = data[0];
            if( bytesRead )
                *bytesRead += 1;
            return;
        }
    }

    void LineSplitter::reset()
    {
        m_currentLine.clear();
        m_clearCurrentLineBuf = false;
        m_prevLineEnding = 0;
    }
}
