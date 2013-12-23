/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "httptypes.h"

#include <stdio.h>
#include <cstring>
#include <memory>

#ifdef _WIN32
static int strcasecmp(const char * str1, const char * str2) { return strcmpi(str1, str2); }
static int strncasecmp(const char * str1, const char * str2, size_t n) { return strnicmp(str1, str2, n); }
#endif


namespace nx_http
{
    int strcasecmp( const StringType& one, const StringType& two )
    {
        if( one.size() < two.size() )
            return -1;
        if( one.size() > two.size() )
            return 1;
#ifdef _WIN32
        return _strnicmp( one.constData(), two.constData(), one.size() );
#else
        return strncasecmp( one.constData(), two.constData(), one.size() );
#endif
    }


    StringType getHeaderValue( const HttpHeaders& headers, const StringType& headerName )
    {
        HttpHeaders::const_iterator it = headers.find( headerName );
        return it == headers.end() ? StringType() : it->second;
    }
    
    ////////////////////////////////////////////////////////////
    //// parse utils
    ////////////////////////////////////////////////////////////

/*
    //split string str (starting at \a offset, checking \a count elements) into substrings using any element of \a separatorStr as separator
    template<class T, class Func>
    void split(
        const T& str,
        const typename T::value_type* separatorStr,
        const Func& func,
        typename T::size_type offset = 0,
        typename T::size_type count = T::npos )
    {
        if( count == T::npos )
            count = str.size();
        typename T::size_type currentTokenStart = offset;
        for( typename T::size_type i = offset; i < count; ++i )
        {
            if( strchr( separatorStr, str[i] ) == NULL )
                continue;
            if( i > currentTokenStart ) //skipping empty tokens
                func( str, currentTokenStart, i-currentTokenStart );
            currentTokenStart = i+1;
        }
        if( currentTokenStart < count )
            func( str, currentTokenStart, count-currentTokenStart );
    }

    template<class Handler, class Str>
    struct SplitHandlerStruct
    {
        Handler* obj;
        void (Handler::*func)( const Str&, size_t, size_t );

        SplitHandlerStruct( Handler* _obj, void (Handler::*_func)( const Str&, size_t, size_t ) )
        :
            obj( _obj ),
            func( _func )
        {
        }

        void operator()( const Str& str, size_t offset, size_t count )
        {
            obj->func( str, offset, count );
        }
    };

    template<class Handler, class Str>
        typename SplitHandlerStruct<Handler, Str> split_handler(
            Handler* obj,
            void (Handler::*func)( const Str&, size_t, size_t ) )
    {
        return typename SplitHandlerStruct<Handler, Str>( obj, func );
    }
*/


    static size_t estimateSerializedDataSize( const HttpHeaders& headers )
    {
        size_t requiredSize = 0;
        for( HttpHeaders::const_iterator
            it = headers.begin();
            it != headers.end();
            ++it )
        {
            requiredSize += it->first.size() + 1 + it->second.size() + 2;
        }
        return requiredSize;
    }

    void serializeHeaders( const HttpHeaders& headers, BufferType* const dstBuffer )
    {
        for( HttpHeaders::const_iterator
            it = headers.begin();
            it != headers.end();
            ++it )
        {
            *dstBuffer += it->first;
            *dstBuffer += ": ";
            *dstBuffer += it->second;
            *dstBuffer += "\r\n";
        }
    }

    bool parseHeader(
        StringType* const headerName,
        StringType* const headerValue,
        const ConstBufferRefType& data )
    {
        ConstBufferRefType headerNameRef;
        ConstBufferRefType headerValueRef;
        if( !parseHeader( &headerNameRef, &headerValueRef, data ) )
            return false;
        *headerName = headerNameRef;
        *headerValue = headerValueRef;
        return true;
    }

    bool parseHeader(
        ConstBufferRefType* const headerName,
        ConstBufferRefType* const headerValue,
        const ConstBufferRefType& data )
    {
        //skipping whitespace at start
        const size_t headerNameStart = find_first_not_of( data, " " );
        if( headerNameStart == BufferNpos )
            return false;
        const size_t headerNameEnd = find_first_of( data, " :", headerNameStart );
        if( headerNameEnd == BufferNpos )
            return false;

        const size_t headerSepPos = data[headerNameEnd] == ':'
            ? headerNameEnd
            : find_first_of( data, ":", headerNameEnd );
        if( headerSepPos == BufferNpos )
            return false;
        const size_t headerValueStart = find_first_not_of( data, ": ", headerSepPos );
        if( headerValueStart == BufferNpos ) {
            *headerName = data.mid( headerNameStart, headerNameEnd-headerNameStart );
            return true;
        }

        //skipping whitespaces after headerValue
        const size_t headerValueEnd = find_last_not_of( data, " ", headerValueStart );
        if( headerValueEnd == BufferNpos )
            return false;

        *headerName = data.mid( headerNameStart, headerNameEnd-headerNameStart );
        *headerValue = data.mid( headerValueStart, headerValueEnd+1-headerValueStart );
        return true;
    }

    namespace StatusCode
    {
        StringType toString( int val )
        {
            return toString(Value(val));
        }

        StringType toString( Value val )
        {
            switch( val )
            {
                case _continue:
                    return StringType("Continue");
                case ok:
                    return StringType("OK");
                case noContent:
                    return StringType("No Content");
                case multipleChoices:
                    return StringType("Multiple Choices");
                case moved:
                    return StringType("Moved");
                case badRequest:
                    return StringType("Bad Request");
                case unauthorized:
                    return StringType("Unauthorized");
                case notFound:
                    return StringType("Not Found");
                case internalServerError:
                    return StringType("Internal Server Error");
                case notImplemented:
                    return StringType("Not Implemented");
                default:
                    return QString::fromLatin1( "Unknown_%1" ).arg(val).toLatin1();
            }
        }
    }


    namespace Method
    {
        const StringType GET( "GET" );
        const StringType PUT( "PUT" );
    }

    namespace Version
    {
        const StringType http_1_0( "HTTP/1.0" );
        const StringType http_1_1( "HTTP/1.1" );
    }


    ////////////////////////////////////////////////////////////
    //// class RequestLine
    ////////////////////////////////////////////////////////////
    static size_t estimateSerializedDataSize( const RequestLine& rl )
    {
        return rl.method.size() + 1 + rl.url.toString().size() + 1 + rl.version.size() + 2;
    }

    bool RequestLine::parse( const ConstBufferRefType& data )
    {
        const BufferType& str = data;
        const QList<QByteArray>& elems = str.split( ' ' );
        if( elems.size() != 3 )
            return false;

        method = elems[0];
        url = QUrl( QLatin1String(elems[1]) );
        version = elems[2];
        return true;
    }

    void RequestLine::serialize( BufferType* const dstBuffer ) const
    {
        *dstBuffer += method;
        *dstBuffer += " ";
        *dstBuffer += url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
        *dstBuffer += " ";
        *dstBuffer += version;
        *dstBuffer += "\r\n";
    }


    ////////////////////////////////////////////////////////////
    //// class StatusLine
    ////////////////////////////////////////////////////////////
    static const int MAX_DIGITS_IN_STATUS_CODE = 5;
    static size_t estimateSerializedDataSize( const StatusLine& sl )
    {
        return sl.version.size() + 1 + MAX_DIGITS_IN_STATUS_CODE + 1 + sl.reasonPhrase.size() + 2;
    }

    StatusLine::StatusLine()
    :
        statusCode( StatusCode::undefined )
    {
    }

    bool StatusLine::parse( const ConstBufferRefType& data )
    {
        const size_t versionStart = 0;
        const size_t versionEnd = find_first_of( data, " ", versionStart );
        if( versionEnd == BufferNpos )
            return false;
        version = data.mid( versionStart, versionEnd-versionStart );

        const size_t statusCodeStart = find_first_not_of( data, " ", versionEnd );
        if( statusCodeStart == BufferNpos )
            return false;
        const size_t statusCodeEnd = find_first_of( data, " ", statusCodeStart );
        if( statusCodeEnd == BufferNpos )
            return false;
        statusCode = ((BufferType)data.mid( statusCodeStart, statusCodeEnd-statusCodeStart )).toUInt();

        const size_t reasonPhraseStart = find_first_not_of( data, " ", statusCodeEnd );
        if( reasonPhraseStart == BufferNpos )
            return false;
        const size_t reasonPhraseEnd = find_first_of( data, "\r\n", reasonPhraseStart );
        reasonPhrase = data.mid(
            reasonPhraseStart,
            reasonPhraseEnd == BufferNpos ? BufferNpos : reasonPhraseEnd-reasonPhraseStart );

        return true;
    }

    void StatusLine::serialize( BufferType* const dstBuffer ) const
    {
        *dstBuffer += version;
        *dstBuffer += " ";
        char buf[11];
#ifdef _WIN32
        _snprintf( buf, sizeof(buf), "%d", statusCode );
#else
        snprintf( buf, sizeof(buf), "%d", statusCode );
#endif
        *dstBuffer += buf;
        *dstBuffer += " ";
        *dstBuffer += reasonPhrase;
        *dstBuffer += "\r\n";
    }


    ////////////////////////////////////////////////////////////
    //// class HttpRequest
    ////////////////////////////////////////////////////////////
    bool HttpRequest::parse( const ConstBufferRefType& data )
    {
        enum ParseState
        {
            readingRequestLine,
            readingHeaders,
            readingMessageBody
        }
        state = readingRequestLine;

        int lineNumber = 0;
        for( size_t curPos = 0; curPos < data.size(); ++lineNumber )
        {
            if( state == readingMessageBody )
            {
                messageBody = data.mid( curPos );
                break;
            }

            //breaking into lines
            const size_t lineSepPos = find_first_of( data, "\r\n", curPos, data.size()-curPos );
            const ConstBufferRefType currentLine = data.mid( curPos, lineSepPos == BufferNpos ? lineSepPos : lineSepPos-curPos );
            switch( state )
            {
                case readingRequestLine:
                    if( !requestLine.parse( currentLine ) )
                        return false;
                    state = readingHeaders;
                    break;

                case readingHeaders:
                {
                    if( !currentLine.isEmpty() )
                    {
                        StringType headerName;
                        StringType headerValue;
                        if( !parseHeader( &headerName, &headerValue, currentLine ) )
                            return false;
                        headers.insert( std::make_pair( headerName, headerValue ) );
                        break;
                    }
                    else
                    {
                        state = readingMessageBody;
                    }
                }

                case readingMessageBody:
                    break;
            }

            if( lineSepPos == BufferNpos )
                break;  //no more data to parse
            curPos = lineSepPos;
            ++curPos;   //skipping separator
            if( curPos < data.size() && (data[curPos] == '\r' || data[curPos] == '\n') )
                ++curPos;   //skipping complex separator (\r\n)
        }

        return true;
    }

    void HttpRequest::serialize( BufferType* const dstBuffer ) const
    {
        //estimating required buffer size
        dstBuffer->clear();
        dstBuffer->reserve( estimateSerializedDataSize(requestLine) + estimateSerializedDataSize(headers) + 2 + messageBody.size() );

        //serializing
        requestLine.serialize( dstBuffer );
        serializeHeaders( headers, dstBuffer );
        dstBuffer->append( (const BufferType::value_type*)"\r\n" );
        dstBuffer->append( messageBody );
    }


    ////////////////////////////////////////////////////////////
    //// class HttpResponse
    ////////////////////////////////////////////////////////////
    void HttpResponse::serialize( BufferType* const dstBuffer ) const
    {
        //estimating required buffer size
        dstBuffer->clear();
        dstBuffer->reserve( estimateSerializedDataSize(statusLine) + estimateSerializedDataSize(headers) + 2 + messageBody.size() );

        //serializing
        statusLine.serialize( dstBuffer );
        serializeHeaders( headers, dstBuffer );
        dstBuffer->append( (const BufferType::value_type*)"\r\n" );
        dstBuffer->append( messageBody );
    }

    BufferType HttpResponse::toString() const
    {
        BufferType str;
        serialize( &str );
        return str;
    }



    ////////////////////////////////////////////////////////////
    //// class HttpMessage
    ////////////////////////////////////////////////////////////
    HttpMessage::HttpMessage( MessageType::Value _type )
    :
        type( _type )
    {
        if( type == MessageType::request )
            request = new HttpRequest();
        else if( type == MessageType::response )
            response = new HttpResponse();
    }

    HttpMessage::HttpMessage( const HttpMessage& right )
    :
        type( right.type )
    {
        if( type == MessageType::request )
            request = new HttpRequest( *right.request );
        else if( type == MessageType::response )
            response = new HttpResponse( *right.response );
        else
            request = NULL;
    }

    HttpMessage::~HttpMessage()
    {
        clear();
    }

    HttpMessage& HttpMessage::operator=( const HttpMessage& right )
    {
        clear();
        type = right.type;
        if( type == MessageType::request )
            request = new HttpRequest( *right.request );
        else if( type == MessageType::response )
            response = new HttpResponse( *right.response );
        return *this;
    }

    void HttpMessage::clear()
    {
        if( type == MessageType::request )
            delete request;
        else if( type == MessageType::response )
            delete response;
        request = NULL;
        type = MessageType::none;
    }


	namespace Header
	{
		namespace AuthScheme
		{
			const char* toString( Value val )
			{
				switch( val )
				{
					case basic:
						return "Basic";
					case digest:
						return "Digest";
					default:
						return "None";
				}
			}

			Value fromString( const char* str )
			{
				if( ::strcasecmp( str, "Basic" ) == 0 )
					return basic;
				if( ::strcasecmp( str, "Digest" ) == 0 )
					return digest;
				return none;
			}

			Value fromString( const ConstBufferRefType& str )
            {
				if( str == "Basic" )
					return basic;
				if( str == "Digest" )
					return digest;
				return none;
            }
        }

		///////////////////////////////////////////////////////////////////
		//  Authorization
		///////////////////////////////////////////////////////////////////
		bool BasicCredentials::parse( const BufferType& /*str*/ )
		{
            //TODO/IMPL
            Q_ASSERT( false );
            return false;
		}

		void BasicCredentials::serialize( BufferType* const dstBuffer ) const
		{
            BufferType serializedCredentials;
            serializedCredentials.append(userid);
            serializedCredentials.append(':');
            serializedCredentials.append(password);
            *dstBuffer += serializedCredentials.toBase64();
		}

		bool DigestCredentials::parse( const BufferType& /*str*/ )
		{
			//TODO/IMPL implementation
            Q_ASSERT( false );
            return false;
		}

		void DigestCredentials::serialize( BufferType* const dstBuffer ) const
		{
            for( QMap<BufferType, BufferType>::const_iterator
                it = params.begin();
                it != params.end();
                ++it )
            {
                if( it != params.begin() )
                    dstBuffer->append( "," );
                dstBuffer->append( it.key());
                dstBuffer->append( "=\"" );
                dstBuffer->append( it.value() );
                dstBuffer->append( "\"" );
            }
		}


        //////////////////////////////////////////////
        //   Authorization
        //////////////////////////////////////////////

        const StringType Authorization::NAME("Authorization");

		Authorization::Authorization()
		:
			authScheme( AuthScheme::none )
		{
			basic = NULL;
		}

		Authorization::Authorization( const AuthScheme::Value& authSchemeVal )
		:
			authScheme( authSchemeVal )
		{
			switch( authScheme )
			{
				case AuthScheme::basic:
					basic = new BasicCredentials();
					break;
				
				case AuthScheme::digest:
					digest = new DigestCredentials();
					break;

				default:
					basic = NULL;
					break;
			}
		}

		Authorization::~Authorization()
		{
			switch( authScheme )
			{
				case AuthScheme::basic:
					delete basic;
                    basic = NULL;
					break;
				
				case AuthScheme::digest:
					delete digest;
                    digest = NULL;
					break;

				default:
					break;
			}
		}

		bool Authorization::parse( const BufferType& /*str*/ )
		{
			//TODO/IMPL implementation
            Q_ASSERT( false );
			return false;
		}

		void Authorization::serialize( BufferType* const dstBuffer ) const
		{
            dstBuffer->append( AuthScheme::toString( authScheme ) );
            dstBuffer->append( " " );
			if( authScheme == AuthScheme::basic )
				basic->serialize( dstBuffer );
			else if( authScheme == AuthScheme::digest )
				digest->serialize( dstBuffer );
		}

        StringType Authorization::toString() const
        {
            BufferType dest;
            serialize( &dest );
            return dest;
        }

        BasicAuthorization::BasicAuthorization( const StringType& userName, const StringType& userPassword )
        :
            Authorization( AuthScheme::basic )
        {
            basic->userid = userName;
            basic->password = userPassword;
        }


        DigestAuthorization::DigestAuthorization()
        :
            Authorization( AuthScheme::digest )
        {
        }

        void DigestAuthorization::addParam( const BufferType& name, const BufferType& value )
        {
            if( name == "username" )
                digest->userid = value;

            digest->params.insert( name, value );
        }


        //////////////////////////////////////////////
        //   WWWAuthenticate
        //////////////////////////////////////////////

        WWWAuthenticate::WWWAuthenticate()
        :
            authScheme( AuthScheme::none )
        {
        }

        bool WWWAuthenticate::parse( const BufferType& str )
        {
            int authSchemeEndPos = str.indexOf( " " );
            if( authSchemeEndPos == -1 )
                return false;

            authScheme = AuthScheme::fromString( ConstBufferRefType(str, 0, authSchemeEndPos) );

            const BufferType& authenticateParamsStr = ConstBufferRefType(str, authSchemeEndPos+1).toByteArrayWithRawData();
            const QList<BufferType>& paramsList = authenticateParamsStr.split(',');
            for( QList<BufferType>::const_iterator
                it = paramsList.begin();
                it != paramsList.end();
                ++it )
            {
                QList<BufferType> nameAndValue = it->trimmed().split('=');
                if( nameAndValue.empty() )
                    continue;
                BufferType value = nameAndValue.size() > 1 ? nameAndValue[1] : BufferType();
                if( value.size() >= 2 )
                {
                    if( value[0] == '\"' )
                        value.remove( 0, 1 );
                    if( value[value.size()-1] == '\"' )
                        value.remove( value.size()-1, 1 );
                }
                params.insert( nameAndValue[0].trimmed(), value );
            }

            return true;
        }
    }
}
