/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#include "list_directory_operation.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtXml/QXmlDefaultHandler>
#include <QtXml/QXmlInputSource>
#include <QtXml/QXmlSimpleReader>

#include <utils/common/log.h>


namespace detail
{
    static const QString CONTENTS_FILE_NAME = "contents.xml";


    /*!
        Parses following xml:\n

        \code {*.xml}
        <?xml version="1.0" encoding="UTF-8"?>
        <contents totalSize="123456" validation="md5">
            <directory>client</directory>
            <directory>mediaserver</directory>
            <directory>appserver</directory>
            <file>help.hlp</file>
        </contents>
        \endcode
    */
    class ContentsXmlSaxHandler
    :
        public QXmlDefaultHandler
    {
    public:
        int64_t totalSize;

        ContentsXmlSaxHandler( std::list<detail::RDirEntry>* const entries )
        :
            totalSize( -1 ),
            m_readingContents( false ),
            m_entries( entries )
        {
        }

        //virtual bool startDocument();
        //virtual bool endDocument();

        virtual bool startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts ) override
        {
            if( qName == QLatin1String("contents") )
            {
                if( m_readingContents )
                    return false;
                int totalSizeArgPos = atts.index(QString::fromLatin1("totalSize"));
                if( totalSizeArgPos >= 0 )
                    totalSize = atts.value(totalSizeArgPos).toLongLong();
                m_readingContents = true;
                return true;
            }
            else if( qName == QLatin1String("directory") )
            {
                if( !m_readingContents )
                    return false;
                //TODO/IMPL analyze atts
                m_openedElement = qName;
                return true;
            }
            else if( qName == QLatin1String("file") )
            {
                if( !m_readingContents )
                    return false;
                m_openedElement = qName;
                return true;
            }
            else
            {
                return false;
            }
        }

        virtual bool endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName ) override
        {
            if( qName == QLatin1String("contents") )
            {
                if( !m_readingContents )
                    return false;
                m_readingContents = false;
                return true;
            }
            else if( qName == QLatin1String("directory") )
            {
                if( !m_readingContents )
                    return false;
                m_openedElement.clear();
                return true;
            }
            else if( qName == QLatin1String("file") )
            {
                if( !m_readingContents )
                    return false;
                m_openedElement.clear();
                return true;
            }
            else
            {
                return false;
            }
        }

        virtual bool characters( const QString& ch ) override
        {
            if( m_openedElement == QLatin1String("directory") )
                m_entries->push_back( RDirEntry(ch, RSyncOperationType::listDirectory) );
            else if( m_openedElement == QLatin1String("file") )
                m_entries->push_back( RDirEntry(ch, RSyncOperationType::getFile) );

            return true;
        }

        virtual QString errorString() const override
        {
            return m_errorDescription;
        }

        virtual bool error( const QXmlParseException& exception ) override
        {
            m_errorDescription = QString::fromLatin1("Parse error. line %1, col %2, parser message: %3").
                arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
            return false;
        }

        virtual bool fatalError( const QXmlParseException& exception ) override
        {
            m_errorDescription = QString::fromLatin1("Fatal parse error. line %1, col %2, parser message: %3").
                arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
            return false;
        }

    private:
        mutable QString m_errorDescription;
        bool m_readingContents;
        QString m_openedElement;
        std::list<detail::RDirEntry>* const m_entries;
    };



    ListDirectoryOperation::ListDirectoryOperation(
        int _id,
        const QUrl& baseUrl,
        const QString& _dirPath,
        const QString& localTargetDirPath,
        AbstractRDirSynchronizationEventHandler* _handler )
    :
        RDirSynchronizationOperation(
            RSyncOperationType::listDirectory,
            _id,
            baseUrl,
            _dirPath,
            _handler ),
        m_localTargetDirPath( localTargetDirPath ),
        m_httpClient( new nx_http::AsyncHttpClient() ),
        m_totalSize( -1 )
    {
        connect(
            m_httpClient.get(), SIGNAL(responseReceived(nx_http::AsyncHttpClientPtr)),
            this, SLOT(onResponseReceived(nx_http::AsyncHttpClientPtr)),
            Qt::DirectConnection );
        connect(
            m_httpClient.get(), SIGNAL(done(nx_http::AsyncHttpClientPtr)),
            this, SLOT(onHttpDone(nx_http::AsyncHttpClientPtr)),
            Qt::DirectConnection );
    }

    ListDirectoryOperation::~ListDirectoryOperation()
    {
        if( m_httpClient )
        {
            m_httpClient->terminate();
            m_httpClient.reset();
        }
    }

    void ListDirectoryOperation::pleaseStop()
    {
        if( m_httpClient )
            m_httpClient->terminate();
    }

    bool ListDirectoryOperation::startAsync()
    {
        //starting download
        m_downloadUrl = baseUrl;
        m_downloadUrl.setPath( baseUrl.path() + (baseUrl.path().endsWith('/') ? "" : "/") + entryPath + "/" + CONTENTS_FILE_NAME );
        if( !m_httpClient->doGet( m_downloadUrl ) )
        {
            m_httpClient->terminate();
            m_httpClient.reset();
            return false;
        }
        return true;
    }

    std::list<detail::RDirEntry> ListDirectoryOperation::entries() const
    {
        return m_entries;
    }

    int64_t ListDirectoryOperation::totalDirSize() const
    {
        return m_totalSize;
    }

    void ListDirectoryOperation::onResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
    {
        if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
        {
            setResult( ResultCode::downloadFailure );
            setErrorText( httpClient->response()->statusLine.reasonPhrase );
            m_httpClient->terminate();
            m_httpClient.reset();
            m_handler->operationDone( shared_from_this() );
            return;
        }
    }

    void ListDirectoryOperation::onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
    {
        //check message body download result
        if( httpClient->failed() )
        {
            //downloading has been interrupted unexpectedly
            setResult( ResultCode::downloadFailure );
            setErrorText( httpClient->response()->statusLine.reasonPhrase );
            m_httpClient->terminate();
            m_httpClient.reset();
            m_handler->operationDone( shared_from_this() );
            return;
        }

        nx_http::BufferType contentsXml = httpClient->fetchMessageBodyBuffer();
        m_httpClient->terminate();
        m_httpClient.reset();

        ContentsXmlSaxHandler xmlHandler( &m_entries );

        QXmlSimpleReader reader;
        reader.setContentHandler( &xmlHandler );
        reader.setErrorHandler( &xmlHandler );

        QBuffer xmlFile( &contentsXml );
        if( !xmlFile.open( QIODevice::ReadOnly ) )
        {
            NX_LOG( QString::fromLatin1( "Failed to open contents.xml buffer. %1" ).arg(xmlFile.errorString()), cl_logERROR );
            setResult( ResultCode::downloadFailure );
            m_handler->operationDone( shared_from_this() );
            return;
        }
        QXmlInputSource input( &xmlFile );
        if( !reader.parse( &input ) )
        {
            NX_LOG( QString::fromLatin1( "Failed to parse contents.xml from remote folder %1. %2" ).arg(m_downloadUrl.toString()).arg(xmlHandler.errorString()), cl_logERROR );
            setResult( ResultCode::downloadFailure );
            m_handler->operationDone( shared_from_this() );
            return;
        }

        m_totalSize = xmlHandler.totalSize;

        //TODO/IMPL asynchronously creating directory
        QDir(m_localTargetDirPath).mkdir( entryPath );

        if( entryPath != "/" )
            for( detail::RDirEntry& entry: m_entries )
                entry.entryPath = entryPath + "/" + entry.entryPath;

        setResult( ResultCode::success );
        m_handler->operationDone( shared_from_this() );
    }
}
