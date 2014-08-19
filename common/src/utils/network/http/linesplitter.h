/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef LINESPLITTER_H
#define LINESPLITTER_H

#include "httptypes.h"


namespace nx_http
{
    //!Splits source bytes stream to lines. Accepts any line ending: CR, LF, CRLF
    class LineSplitter
    {
    public:
        LineSplitter();
        ~LineSplitter();

        /*!
            If source buffer \a data contains whole line then reference to \a data is returned, otherwise - data is cached and returned reference to internal buffer.
            \a *lineBuffer is used to return found line.
            \a *lineBuffer can be pointer to \a data or to an internal buffer. It is valid to next \a parseByLines call or to object destruction
            \return true, if line read (it can be empty). false, if no line yet
            \note If \a data contains multiple lines, this method returns first line and does not cache anything!
        */
        bool parseByLines(
            const ConstBufferRefType& data,
            QnByteArrayConstRef* const lineBuffer,
            size_t* const bytesRead );
        void reset();

    private:
        //lined text parsing
        BufferType m_currentLine;
        bool m_clearCurrentLineBuf;
        char m_prevLineEnding;
    };
}

#endif  //LINESPLITTER_H
