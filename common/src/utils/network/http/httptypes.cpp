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
    
    HttpHeaders::iterator insertOrReplaceHeader( HttpHeaders* const headers, const HttpHeader& newHeader )
    {
        HttpHeaders::iterator existingHeaderIter = headers->lower_bound( newHeader.first );
        if( (existingHeaderIter != headers->end()) &&
            (strcasecmp( existingHeaderIter->first, newHeader.first ) == 0) )
        {
            existingHeaderIter->second = newHeader.second;  //replacing header
            return existingHeaderIter;
        }
        else
        {
            return headers->insert( existingHeaderIter, newHeader );
        }
    }

    void removeHeader( HttpHeaders* const headers, const StringType& headerName )
    {
        HttpHeaders::iterator itr = headers->lower_bound( headerName );
        while (itr != headers->end() && itr->first == headerName)
            itr = headers->erase(itr);
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

    HttpHeader parseHeader( const ConstBufferRefType& data )
    {
        ConstBufferRefType headerNameRef;
        ConstBufferRefType headerValueRef;
        parseHeader( &headerNameRef, &headerValueRef, data );
        return HttpHeader( headerNameRef, headerValueRef );
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
                case partialContent:
                    return StringType("Partial Content");
                case multipleChoices:
                    return StringType("Multiple Choices");
                case moved:
                    return StringType("Moved");
                case moved_permanently:
                    return StringType("Moved Permanently");
                case badRequest:
                    return StringType("Bad Request");
                case unauthorized:
                    return StringType("Unauthorized");
                case forbidden:
                    return StringType("Forbidden");
                case notFound:
                    return StringType("Not Found");
                case notAllowed:
                    return StringType("Not Allowed");
                case proxyAuthenticationRequired:
                    return StringType("Proxy Authentication Required");
                case internalServerError:
                    return StringType("Internal Server Error");
                case notImplemented:
                    return StringType("Not Implemented");
                case serviceUnavailable:
                    return StringType("Service Unavailable");
                default:
                    return StringType("Unknown_") + StringType::number(val);
            }
        }
    }


    namespace Method
    {
        const StringType GET( "GET" );
        const StringType POST( "POST" );
        const StringType PUT( "PUT" );
    }

    //namespace Version
    //{
    //    const StringType http_1_0( "HTTP/1.0" );
    //    const StringType http_1_1( "HTTP/1.1" );
    //}


    ////////////////////////////////////////////////////////////
    //// class MimeProtoVersion
    ////////////////////////////////////////////////////////////
    static size_t estimateSerializedDataSize( const MimeProtoVersion& val )
    {
        return val.protocol.size() + 1 + val.version.size();
    }

    bool MimeProtoVersion::parse( const ConstBufferRefType& data )
    {
        int sepPos = data.indexOf( '/' );
        //const ConstBufferRefType& trimmedData = data.trimmed();
        const ConstBufferRefType& trimmedData = data;
        if( sepPos == -1 )
            return false;
        protocol = trimmedData.mid( 0, sepPos );
        version = trimmedData.mid( sepPos+1 );
        return true;
    }

    void MimeProtoVersion::serialize( BufferType* const dstBuffer ) const
    {
        dstBuffer->append( protocol );
        dstBuffer->append( "/" );
        dstBuffer->append( version );
    }


    ////////////////////////////////////////////////////////////
    //// class RequestLine
    ////////////////////////////////////////////////////////////
    static size_t estimateSerializedDataSize( const RequestLine& rl )
    {
        return rl.method.size() + 1 + rl.url.toString().size() + 1 + estimateSerializedDataSize(rl.version) + 2;
    }

    bool RequestLine::parse( const ConstBufferRefType& data )
    {
        const BufferType& str = data.toByteArrayWithRawData();
        const QList<QByteArray>& elems = str.split( ' ' );
        if( elems.size() != 3 )
            return false;

        method = elems[0];
        url = QUrl( QLatin1String(elems[1]) );
        if( !version.parse(elems[2]) )
            return false;
        return true;
    }

    void RequestLine::serialize( BufferType* const dstBuffer ) const
    {
        *dstBuffer += method;
        *dstBuffer += " ";
        *dstBuffer += url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
        *dstBuffer += " ";
        version.serialize( dstBuffer );
        *dstBuffer += "\r\n";
    }


    ////////////////////////////////////////////////////////////
    //// class StatusLine
    ////////////////////////////////////////////////////////////
    static const int MAX_DIGITS_IN_STATUS_CODE = 5;
    static size_t estimateSerializedDataSize( const StatusLine& sl )
    {
        return estimateSerializedDataSize(sl.version) + 1 + MAX_DIGITS_IN_STATUS_CODE + 1 + sl.reasonPhrase.size() + 2;
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
        if( !version.parse(data.mid( versionStart, versionEnd-versionStart )) )
            return false;

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
        version.serialize( dstBuffer );
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
    //// class Request
    ////////////////////////////////////////////////////////////
    template<class MessageType, class MessageLineType>
    bool parseRequestOrResponse(
        const ConstBufferRefType& data,
        MessageType* message,
        MessageLineType MessageType::*messageLine )
    {
        enum ParseState
        {
            readingMessageLine, //request line or status line
            readingHeaders,
            readingMessageBody
        }
        state = readingMessageLine;

        int lineNumber = 0;
        for( size_t curPos = 0; curPos < data.size(); ++lineNumber )
        {
            if( state == readingMessageBody )
            {
                message->messageBody = data.mid( curPos );
                break;
            }

            //breaking into lines
            const size_t lineSepPos = find_first_of( data, "\r\n", curPos, data.size()-curPos );
            const ConstBufferRefType currentLine = data.mid( curPos, lineSepPos == BufferNpos ? lineSepPos : lineSepPos-curPos );
            switch( state )
            {
                case readingMessageLine:
                    if( !(message->*messageLine).parse( currentLine ) )
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
                        message->headers.insert( std::make_pair( headerName, headerValue ) );
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

    bool Request::parse( const ConstBufferRefType& data )
    {
        return parseRequestOrResponse( data, this, &Request::requestLine );
    }

    void Request::serialize( BufferType* const dstBuffer ) const
    {
        //estimating required buffer size
        dstBuffer->reserve( dstBuffer->size() + estimateSerializedDataSize(requestLine) + estimateSerializedDataSize(headers) + 2 + messageBody.size() );

        //serializing
        requestLine.serialize( dstBuffer );
        serializeHeaders( headers, dstBuffer );
        dstBuffer->append( (const BufferType::value_type*)"\r\n" );
        dstBuffer->append( messageBody );
    }


    ////////////////////////////////////////////////////////////
    //// class Response
    ////////////////////////////////////////////////////////////
    bool Response::parse( const ConstBufferRefType& data )
    {
        return parseRequestOrResponse( data, this, &Response::statusLine );
    }

    void Response::serialize( BufferType* const dstBuffer ) const
    {
        //estimating required buffer size
        dstBuffer->reserve( dstBuffer->size() + estimateSerializedDataSize(statusLine) + estimateSerializedDataSize(headers) + 2 + messageBody.size() );

        //serializing
        statusLine.serialize( dstBuffer );
        serializeHeaders( headers, dstBuffer );
        dstBuffer->append( (const BufferType::value_type*)"\r\n" );
        dstBuffer->append( messageBody );
    }

    BufferType Response::toString() const
    {
        BufferType buf;
        serialize( &buf );
        return buf;
    }


    namespace MessageType
    {
        QLatin1String toString( Value val )
        {
            switch( val )
            {
                case request:
                {
                    static QLatin1String requestStr("request");
                    return requestStr;
                }
                case response:
                {
                    static QLatin1String responseStr("response");
                    return responseStr;
                }
                default:
                {
                    static QLatin1String unknownStr("unknown");
                    return unknownStr;
                }
            }
        }
    }



    ////////////////////////////////////////////////////////////
    //// class Message
    ////////////////////////////////////////////////////////////
    Message::Message( MessageType::Value _type )
    :
        type( _type )
    {
        if( type == MessageType::request )
            request = new Request();
        else if( type == MessageType::response )
            response = new Response();
    }

    Message::Message( const Message& right )
    :
        type( right.type )
    {
        if( type == MessageType::request )
            request = new Request( *right.request );
        else if( type == MessageType::response )
            response = new Response( *right.response );
        else
            request = NULL;
    }

    Message::Message( Message&& right )
    :
        type( right.type ),
        request( right.request )
    {
        right.type = MessageType::none;
        right.request = nullptr;
    }

    Message::~Message()
    {
        clear();
    }

    Message& Message::operator=( const Message& right )
    {
        clear();
        type = right.type;
        if( type == MessageType::request )
            request = new Request( *right.request );
        else if( type == MessageType::response )
            response = new Response( *right.response );
        return *this;
    }

    Message& Message::operator=(Message&& right)
    {
        clear();

        type = right.type;
        right.type = MessageType::none;
        request = right.request;
        right.request = nullptr;
        return *this;
    }

    void Message::serialize( BufferType* const dstBuffer ) const
    {
        switch( type )
        {
            case MessageType::request:
                return request->serialize( dstBuffer );
            case MessageType::response:
                return response->serialize( dstBuffer );
            default:
                return /*false*/;
        }
    }

    void Message::clear()
    {
        if( type == MessageType::request )
            delete request;
        else if( type == MessageType::response )
            delete response;
        request = NULL;
        type = MessageType::none;
    }

    BufferType Message::toString() const
    {
        BufferType str;
        switch( type )
        {
            case MessageType::request:
                request->serialize( &str );
                break;
            case MessageType::response:
                response->serialize( &str );
                break;
            default:
                break;
        }
        return str;
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
                    dstBuffer->append( ", " );
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


    ChunkHeader::ChunkHeader()
    :
        chunkSize( 0 )
    {
    }

    void ChunkHeader::clear()
    {
        chunkSize = 0;
        extensions.clear();
    }

    /*!
        \return bytes read from \a buf
    */
    int ChunkHeader::parse( const ConstBufferRefType& buf )
    {
        const char* curPos = buf.constData();
        const char* dataEnd = curPos + buf.size();

        enum State
        {
            readingChunkSize,
            readingExtName,
            readingExtVal,
            readingCRLF
        }
        state = readingChunkSize;
        chunkSize = 0;
        const char* extNameStart = 0;
        const char* extSepPos = 0;
        const char* extValStart = 0;

        for( ; curPos < dataEnd; ++curPos )
        {
            const char ch = *curPos;
            switch( ch )
            {
                case ';':
                    if( state == readingExtName )
                    {
                        if( curPos == extNameStart )
                            return -1;  //empty extension
                        extensions.push_back( std::make_pair( BufferType(extNameStart, curPos-extNameStart), BufferType() ) );
                    }
                    else if( state == readingExtVal )
                    {
                        if( extSepPos == extNameStart )
                            return -1;  //empty extension

                        if( *extValStart == '"' )
                            ++extValStart;
                        int extValSize = curPos - extValStart;
                        if( extValSize > 0 && *(extValStart + extValSize - 1) == '"' )
                            --extValSize;

                        extensions.push_back( std::make_pair( BufferType( extNameStart, extSepPos - extNameStart ), BufferType( extValStart, extValSize ) ) );
                    }

                    state = readingExtName;
                    extNameStart = curPos+1;
                    continue;

                case '=':
                    if( state == readingExtName )
                    {
                        state = readingExtVal;
                        extSepPos = curPos;
                        extValStart = curPos + 1;
                    }
                    else
                    {
                        //unexpected token
                        return -1;
                    }
                    break;

                case '\r':
                    if( state == readingExtName )
                    {
                        if( curPos == extNameStart )
                            return -1;  //empty extension
                        extensions.push_back( std::make_pair( BufferType(extNameStart, curPos-extNameStart), BufferType() ) );
                    }
                    else if( state == readingExtVal )
                    {
                        if( extSepPos == extNameStart )
                            return -1;  //empty extension

                        if( *extValStart == '"' )
                            ++extValStart;
                        int extValSize = curPos - extValStart;
                        if( extValSize > 0 && *(extValStart + extValSize - 1) == '"' )
                            --extValSize;

                        extensions.push_back( std::make_pair( BufferType( extNameStart, extSepPos - extNameStart ), BufferType( extValStart, extValSize ) ) );
                    }

                    if( (dataEnd - curPos < 2) || *(curPos+1) != '\n' )
                        return -1;
                    return curPos + 2 - buf.constData();

                default:
                    if( state == readingChunkSize )
                    {
                        if (ch >= '0' && ch <= '9')
                            chunkSize = (chunkSize << 4) | (ch - '0');
                        else if (ch >= 'a' && ch <= 'f')
                            chunkSize = (chunkSize << 4) | (ch - 'a' + 10);
                        else if (ch >= 'A' && ch <= 'F')
                            chunkSize = (chunkSize << 4) | (ch - 'A' + 10);
                        else
                            return -1;  //unexpected token
                    }
                    break;
            }
        }

        return -1;
    }

    int ChunkHeader::serialize( BufferType* const /*dstBuffer*/ ) const
    {
        //TODO #ak
        return -1;
    }

    /*
    int ChunkHeader_parse_test()
    {
        nx_http::ChunkHeader chunkHeader;
        int bytesRead = chunkHeader.parse( QByteArray("1f5c;her=hren\r\n") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("0001f;\r\n") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("0001f\r\n") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("1000;lonely;one=two\r\n") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("1000;lonely\r") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("1000;lonely\n") );
        chunkHeader = nx_http::ChunkHeader();
        bytesRead = chunkHeader.parse( QByteArray("10") );
        chunkHeader = nx_http::ChunkHeader();
        return 0;
    }
    */
}
