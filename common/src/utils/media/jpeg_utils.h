/**********************************************************
* 27 may 2014
* a.kolesnikov
***********************************************************/

#include <QtCore/QString>


namespace nx_jpg
{
    class ImageInfo
    {
    public:
        int precision;
        int width;
        int height;

        ImageInfo()
        :
            precision( -1 ),
            width( -1 ),
            height( -1 )
        {
        }
    };
    
    enum ResultCode
    {
        succeeded,
        interrupted,
        badData,
        failed
    };

    QString toString( ResultCode resCode );

    //!Event-driven jpg parser. Quite simple due to performance considerations and current use case
    class JpegParser
    {
    public:
        //!bool( int markerType, size_t markerLength, size_t currentOffset )
        /*!
            \param markerLength Length of marker, including 2-byte marker length field, but not including entropy-coded data
            \param currentOffset Offset of start of marker (points to 0xff)
            If handler returns \a false, parsing stops and \a JpegParser::parse returns \a nx_jpg::interrupted
        */
        typedef bool ParseHandlerFuncType( int, size_t, size_t );
        //!void( int error, size_t currentOffset )
        /*!
            \param currentOffset Offset of start of marker (points to 0xff)
        */
        typedef void ErrorHandlerFuncType( int, size_t );

        JpegParser();
        /*!
            \code{cpp}
            bool( int markerType, size_t markerLength, size_t currentOffset )
            \endcode
            If \a parseHandler returns \a true
        */
        template<class T>
        void setParseHandler( const T& parseHandler ) {
            m_parseHandler = std::function<ParseHandlerFuncType>( parseHandler );
        }
        /*!
            \code{cpp}
            void( int error, size_t currentOffset )
            \endcode
        */
        template<class T>
        void setErrorHandler( const T& errorHandler ) {
            m_errorHandler = std::function<ErrorHandlerFuncType>( errorHandler );
        }

        ResultCode parse( const quint8* data, size_t size );

    private:
        enum ParsingState
        {
            waitingStartOfMarker,
            readingMarkerCode,
            readingMarkerLength,
            skippingMarkerBody
        };

        std::function<ParseHandlerFuncType> m_parseHandler;
        std::function<ErrorHandlerFuncType> m_errorHandler;
        ParsingState m_state;
        int m_currentMarkerCode;
    };

    bool readJpegImageInfo( const quint8* data, size_t size, ImageInfo* const imgInfo );
}
