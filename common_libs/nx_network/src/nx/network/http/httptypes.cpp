/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "httptypes.h"

#include <stdio.h>
#include <cstring>
#include <memory>

#include "version.h"

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

    HttpHeaders::iterator insertHeader( HttpHeaders* const headers, const HttpHeader& newHeader )
    {
        HttpHeaders::iterator itr = headers->lower_bound( newHeader.first );
        return headers->insert( itr, newHeader );
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

        //skipping separators after headerValue
        const size_t headerValueEnd = find_last_not_of( data, " \n\r", headerValueStart );
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
                case notAcceptable:
                    return StringType("Not Acceptable");
                case proxyAuthenticationRequired:
                    return StringType("Proxy Authentication Required");
                case rangeNotSatisfiable:
                    return StringType("Requested range not satisfiable");
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

    const StringType Method::GET( "GET" );
    const StringType Method::HEAD( "HEAD" );
    const StringType Method::POST( "POST" );
    const StringType Method::PUT( "PUT" );

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
        protocol.clear();
        version.clear();

        const int sepPos = data.indexOf( '/' );
        if( sepPos == -1 )
            return false;
        protocol.append( data.constData(), sepPos );
        version.append( data.constData()+sepPos+1, data.size()-(sepPos+1) );
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
        enum ParsingState
        {
            psMethod,
            psUrl,
            psVersion,
            psDone
        }
        parsingState = psMethod;

        const char* str = data.constData();
        const char* strEnd = str + data.size();
        const char* tokenStart = nullptr;
        bool waitingNextToken = true;
        for( ; str <= strEnd; ++str )
        {
            if( (*str == ' ') || (str == strEnd) )
            {
                if( !waitingNextToken ) //waiting end of token
                {
                    //found new token [tokenStart, str)
                    switch( parsingState )
                    {
                        case psMethod:
                            method.append( tokenStart, str-tokenStart );
                            parsingState = psUrl;
                            break;
                        case psUrl:
                            url.setUrl( QLatin1String( tokenStart, str-tokenStart ) );
                            parsingState = psVersion;
                            break;
                        case psVersion:
                            version.parse( data.mid( tokenStart-data.constData(), str-tokenStart ) );
                            parsingState = psDone;
                            break;
                        default:
                            return false;
                    }

                    waitingNextToken = true;
                }
            }
            else
            {
                if( waitingNextToken )
                {
                    tokenStart = str;
                    waitingNextToken = false;   //waiting token end
                }
            }
        }

        return parsingState == psDone;
    }

    void RequestLine::serialize( BufferType* const dstBuffer ) const
    {
        *dstBuffer += method;
        *dstBuffer += " ";
        QByteArray path = url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
        *dstBuffer += path.isEmpty() ? "/" : path;
        *dstBuffer += urlPostfix;
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

    BufferType Request::serialized() const
    {
        BufferType buf;
        serialize( &buf );
        return buf;
    }

    BufferType Request::getCookieValue(const BufferType& name) const
    {
        nx_http::HttpHeaders::const_iterator cookieIter = headers.find( "cookie" );
        if (cookieIter == headers.end())
            return BufferType();

        //TODO #ak optimize string operations here
        for(const BufferType& value: cookieIter->second.split(';'))
        {
            QList<BufferType> params = value.split('=');
            if (params.size() > 1 && params[0].trimmed() == name)
                return params[1];
        }

        return BufferType();
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

    void Response::serializeMultipartResponse( BufferType* const dstBuffer, const ConstBufferRefType& boundary  ) const
    {
        //estimating required buffer size
        dstBuffer->reserve( dstBuffer->size() + boundary.size() + 2 + estimateSerializedDataSize(headers) + 2 + messageBody.size() + 2);

        //serializing
        dstBuffer->append(boundary);
        dstBuffer->append("\r\n");
        serializeHeaders( headers, dstBuffer );
        dstBuffer->append( (const BufferType::value_type*)"\r\n" );
        dstBuffer->append( messageBody );
        dstBuffer->append("\r\n");
    }


    BufferType Response::toString() const
    {
        BufferType buf;
        serialize( &buf );
        return buf;
    }

    BufferType Response::toMultipartString(const ConstBufferRefType& boundary) const
    {
        BufferType buf;
        serializeMultipartResponse( &buf, boundary );
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


    namespace header
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

        namespace 
        {
            std::vector<QnByteArrayConstRef> splitQuotedString( const QnByteArrayConstRef& src, char sep )
            {
                std::vector<QnByteArrayConstRef> result;
                QnByteArrayConstRef::size_type curTokenStart = 0;
                bool quoted = false;
                for( QnByteArrayConstRef::size_type
                    pos = 0;
                    pos < src.size();
                    ++pos )
                {
                    const char ch = src[pos];
                    if( !quoted && (ch == sep) )
                    {
                        result.push_back( src.mid( curTokenStart, pos - curTokenStart ) );
                        curTokenStart = pos + 1;
                    }
                    else if( ch == '"' )
                    {
                        quoted = !quoted;
                    }
                }
                result.push_back( src.mid( curTokenStart ) );

                return result;
            }
        }

        void parseDigestAuthParams(
            const ConstBufferRefType& authenticateParamsStr,
            QMap<BufferType, BufferType>* const params,
            char sep )
        {
            const std::vector<ConstBufferRefType>& paramsList = splitQuotedString( authenticateParamsStr, sep );
            for( const ConstBufferRefType& token : paramsList )
            {
                const auto& nameAndValue = splitQuotedString( token.trimmed(), '=' );
                if( nameAndValue.empty() )
                    continue;
                ConstBufferRefType value = nameAndValue.size() > 1 ? nameAndValue[1] : ConstBufferRefType();
                params->insert( nameAndValue[0].trimmed(), value.trimmed( "\"" ) );
            }
        }

        void serializeAuthParams(
            BufferType* const dstBuffer,
            const QMap<BufferType, BufferType>& params )
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

        ///////////////////////////////////////////////////////////////////
        //  Authorization
        ///////////////////////////////////////////////////////////////////
        bool BasicCredentials::parse( const BufferType& str )
        {
            const auto decodedBuf = BufferType::fromBase64( str );
            const int sepIndex = decodedBuf.indexOf( ':' );
            if( sepIndex == -1 )
                return false;
            userid = decodedBuf.mid( 0, sepIndex );
            password = decodedBuf.mid( sepIndex+1 );
            
            return true;
        }

        void BasicCredentials::serialize( BufferType* const dstBuffer ) const
        {
            BufferType serializedCredentials;
            serializedCredentials.append(userid);
            serializedCredentials.append(':');
            serializedCredentials.append(password);
            *dstBuffer += serializedCredentials.toBase64();
        }

        bool DigestCredentials::parse( const BufferType& str, char separator )
        {
            parseDigestAuthParams( str, &params, separator );
            auto usernameIter = params.find( "username" );
            if( usernameIter != params.cend() )
                userid = usernameIter.value();
            return true;
        }

        void DigestCredentials::serialize( BufferType* const dstBuffer ) const
        {
            serializeAuthParams( dstBuffer, params );
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

        Authorization::Authorization( Authorization&& right )
        :
            authScheme( right.authScheme ),
            basic( right.basic )
        {
            right.authScheme = AuthScheme::none;
            right.basic = nullptr;
        }

        Authorization::Authorization(const Authorization& right)
        :
            authScheme( right.authScheme )
        {
            switch (authScheme)
            {
                case AuthScheme::basic:
                    basic = new BasicCredentials(*right.basic);
                    break;
                case AuthScheme::digest:
                    digest = new DigestCredentials(*right.digest);
                    break;
                default:
                    break;
            }
        }
        
        Authorization::~Authorization()
        {
            clear();
        }

        Authorization& Authorization::operator=( Authorization&& right )
        {
            clear();

            authScheme = right.authScheme;
            basic = right.basic;

            right.authScheme = AuthScheme::none;
            right.basic = nullptr;

            return *this;
        }

        bool Authorization::parse( const BufferType& str )
        {
            clear();

            int authSchemeEndPos = str.indexOf( " " );
            if( authSchemeEndPos == -1 )
                return false;

            authScheme = AuthScheme::fromString( ConstBufferRefType( str, 0, authSchemeEndPos ) );
            const ConstBufferRefType authParamsData( str, authSchemeEndPos + 1 );
            switch( authScheme )
            {
                case AuthScheme::basic:
                    basic = new BasicCredentials();
                    return basic->parse( authParamsData.toByteArrayWithRawData() );
                case AuthScheme::digest:
                    digest = new DigestCredentials();
                    return digest->parse( authParamsData.toByteArrayWithRawData() );
                default:
                    return false;
            }
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

        BufferType Authorization::serialized() const
        {
            BufferType dest;
            serialize( &dest );
            return dest;
        }

        void Authorization::clear()
        {
            switch( authScheme )
            {
                case AuthScheme::basic:
                    delete basic;
                    basic = nullptr;
                    break;
                
                case AuthScheme::digest:
                    delete digest;
                    digest = nullptr;
                    break;

                default:
                    break;
            }
            authScheme = AuthScheme::none;
        }

        StringType Authorization::userid() const
        {
            switch( authScheme )
            {
                case AuthScheme::basic:
                    return basic->userid;
                case AuthScheme::digest:
                    return digest->userid;
                default:
                    return StringType();
            }
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

        DigestAuthorization::DigestAuthorization( DigestAuthorization&& right )
        :
            Authorization( std::move(right) )
        {
        }

        DigestAuthorization::DigestAuthorization(const DigestAuthorization& right)
        :
            Authorization(right)
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

        const StringType WWWAuthenticate::NAME("WWW-Authenticate");

        WWWAuthenticate::WWWAuthenticate(AuthScheme::Value authScheme)
        :
            authScheme( authScheme )
        {
        }

        bool WWWAuthenticate::parse( const BufferType& str )
        {
            int authSchemeEndPos = str.indexOf( " " );
            if( authSchemeEndPos == -1 )
                return false;

            authScheme = AuthScheme::fromString( ConstBufferRefType(str, 0, authSchemeEndPos) );

            parseDigestAuthParams(
                ConstBufferRefType( str, authSchemeEndPos + 1 ),
                &params );

            return true;
        }

        void WWWAuthenticate::serialize( BufferType* const dstBuffer ) const
        {
            dstBuffer->append( AuthScheme::toString( authScheme ) );
            dstBuffer->append( " " );
            serializeAuthParams( dstBuffer, params );
        }

        BufferType WWWAuthenticate::serialized() const
        {
            BufferType dest;
            serialize( &dest );
            return dest;
        }


        //////////////////////////////////////////////
        //   Accept-Encoding
        //////////////////////////////////////////////

        //const nx_http::StringType IDENTITY_CODING( "identity" );
        //const nx_http::StringType ANY_CODING( "*" );

        AcceptEncodingHeader::AcceptEncodingHeader( const nx_http::StringType& strValue )
        {
            parse( strValue );
        }

        void AcceptEncodingHeader::parse( const nx_http::StringType& str )
        {
            m_anyCodingQValue.reset();

            //TODO #ak this function is very slow. Introduce some parsing without allocations and copyings..
            auto codingsStr = str.split( ',' );
            for( const nx_http::StringType& contentCodingStr: codingsStr )
            {
                auto tokens = contentCodingStr.split( ';' );
                if( tokens.isEmpty() )
                    continue;
                double qValue = 1.0;
                if( tokens.size() > 1 )
                {
                    const nx_http::StringType& qValueStr = tokens[1].trimmed();
                    if( !qValueStr.startsWith("q=") )
                        continue;   //bad token, ignoring...
                    qValue = qValueStr.mid(2).toDouble();
                }
                const nx_http::StringType& contentCoding = tokens.front().trimmed();
                if( contentCoding == ANY_CODING )
                    m_anyCodingQValue = qValue;
                else
                    m_codings[contentCoding] = qValue;
            }
        }

        bool AcceptEncodingHeader::encodingIsAllowed( const nx_http::StringType& encodingName, double* q ) const
        {
            auto codingIter = m_codings.find( encodingName );
            if( codingIter == m_codings.end() )
            {
                //encoding is not explicitly specified
                if( m_anyCodingQValue )
                {
                    if( q )
                        *q = m_anyCodingQValue.get();
                    return m_anyCodingQValue.get() > 0.0;
                }

                return encodingName == IDENTITY_CODING;
            }

            if( q )
                *q = codingIter->second;
            return codingIter->second > 0.0;
        }


        //////////////////////////////////////////////
        //   Range
        //////////////////////////////////////////////

        Range::Range()
        {
        }

        bool Range::parse( const nx_http::StringType& strValue )
        {
            auto simpleRangeList = strValue.split(',');
            rangeSpecList.reserve( simpleRangeList.size() );
            for( const StringType& simpleRangeStr: simpleRangeList )
            {
                if( simpleRangeStr.isEmpty() )
                    return false;
                RangeSpec rangeSpec;
                const int sepPos = simpleRangeStr.indexOf('-');
                if( sepPos == -1 )
                {
                    rangeSpec.start = simpleRangeStr.toULongLong();
                    rangeSpec.end = rangeSpec.start;
                }
                else
                {
                    rangeSpec.start = StringType::fromRawData(simpleRangeStr.constData(), sepPos).toULongLong();
                    if( sepPos < simpleRangeStr.size()-1 )  //range end is not empty
                       rangeSpec.end = StringType::fromRawData(simpleRangeStr.constData()+sepPos+1, simpleRangeStr.size()-sepPos-1).toULongLong();
                }
                if( rangeSpec.end && rangeSpec.end < rangeSpec.start )
                    return false;
                rangeSpecList.push_back( std::move(rangeSpec) );
            }

            return true;
        }

        bool Range::validateByContentSize( size_t contentSize ) const
        {
            for( const RangeSpec& rangeSpec: rangeSpecList )
            {
                if( (rangeSpec.start >= contentSize) || (rangeSpec.end && rangeSpec.end.get() >= contentSize) )
                    return false;
            }

            return true;
        }

        bool Range::empty() const
        {
            return rangeSpecList.empty();
        }

        bool Range::full( size_t contentSize ) const
        {
            if( contentSize == 0 )
                return true;

            //map<start, end>
            std::map<quint64, quint64> rangesSorted;
            for( const RangeSpec& rangeSpec: rangeSpecList )
                rangesSorted.emplace( rangeSpec.start, rangeSpec.end ? rangeSpec.end.get() : contentSize );

            quint64 curPos = 0;
            for( const std::pair<quint64, quint64>& range: rangesSorted )
            {
                if( range.first > curPos )
                    return false;
                if( range.second >= curPos )
                    curPos = range.second+1;
            }

            return curPos >= contentSize;
        }

        quint64 Range::totalRangeLength( size_t contentSize ) const
        {
            if( contentSize == 0 || rangeSpecList.empty() )
                return 0;

            //map<start, end>
            std::map<quint64, quint64> rangesSorted;
            for( const RangeSpec& rangeSpec: rangeSpecList )
                rangesSorted.emplace( rangeSpec.start, rangeSpec.end ? rangeSpec.end.get() : contentSize );

            quint64 curPos = 0;
            quint64 totalLength = 0;
            for( const std::pair<quint64, quint64>& range: rangesSorted )
            {
                if( curPos < range.first )
                    curPos = range.first;
                if( range.second < curPos )
                    continue;
                const quint64 endPos = std::min<quint64>( contentSize-1, range.second );
                totalLength += endPos - curPos + 1;
                curPos = endPos + 1;
                if( curPos >= contentSize )
                    break;
            }

            return totalLength;
        }


        //////////////////////////////////////////////
        //   Content-Range
        //////////////////////////////////////////////
        ContentRange::ContentRange()
        :
            unitName( "bytes" )
        {
        }

        quint64 ContentRange::rangeLength() const
        {
            NX_ASSERT( !rangeSpec.end || (rangeSpec.end >= rangeSpec.start) );

            if( rangeSpec.end )
                return rangeSpec.end.get() - rangeSpec.start + 1;   //both boundaries are inclusive
            else if( instanceLength )
                return instanceLength.get() - rangeSpec.start;
            else
                return 1;   //since both boundaries are inclusive, 0-0 means 1 byte (the first one)
        }

        StringType ContentRange::toString() const
        {
            NX_ASSERT( !rangeSpec.end || (rangeSpec.end >= rangeSpec.start) );

            StringType str = unitName;
            str += " ";
            str += StringType::number( rangeSpec.start ) + "-";
            if( rangeSpec.end )
                str += StringType::number( rangeSpec.end.get() );
            else if( instanceLength )
                str += StringType::number( instanceLength.get()-1 );
            else
                str += StringType::number( rangeSpec.start );

            if( instanceLength )
                str += "/" + StringType::number( instanceLength.get() );
            else
                str += "/*";

            return str;
        }


        //////////////////////////////////////////////
        //   Via
        //////////////////////////////////////////////

        bool Via::parse( const nx_http::StringType& strValue )
        {
            if( strValue.isEmpty() )
                return true;

            //introducing loop counter to guarantee method finiteness in case of bug in code
            for( size_t curEntryEnd = nx_http::find_first_of(strValue, ","), curEntryStart = 0, i = 0;
                curEntryStart != nx_http::BufferNpos && (i < 1000);
                curEntryStart = (curEntryEnd == nx_http::BufferNpos ? curEntryEnd : curEntryEnd+1), curEntryEnd = nx_http::find_first_of(strValue, ",", curEntryEnd+1), ++i )
            {
                ProxyEntry entry;

                //skipping spaces at the start of entry
                while( (curEntryStart < (curEntryEnd == nx_http::BufferNpos ? strValue.size() : curEntryEnd)) &&
                       (strValue.at(curEntryStart) == ' ') )
                {
                    ++curEntryStart;
                }

                //curEntryStart points first char after comma
                size_t receivedProtoEnd = nx_http::find_first_of( strValue, " ", curEntryStart );
                if( receivedProtoEnd == nx_http::BufferNpos )
                    return false;
                ConstBufferRefType protoNameVersion( strValue, curEntryStart, receivedProtoEnd-curEntryStart );
                size_t nameVersionSep = nx_http::find_first_of( protoNameVersion, "/" );
                if( nameVersionSep == nx_http::BufferNpos )
                {
                    //only version present
                    entry.protoVersion = protoNameVersion;
                }
                else
                {
                    entry.protoName = protoNameVersion.mid( 0, nameVersionSep );
                    entry.protoVersion = protoNameVersion.mid( nameVersionSep+1 );
                }

                size_t receivedByStart = nx_http::find_first_not_of( strValue, " ", receivedProtoEnd+1 );
                if( receivedByStart == nx_http::BufferNpos || receivedByStart > curEntryEnd )
                    return false;   //no receivedBy field

                size_t receivedByEnd = nx_http::find_first_of( strValue, " ", receivedByStart );
                if( receivedByEnd == nx_http::BufferNpos || (receivedByEnd > curEntryEnd) )
                {
                    receivedByEnd = curEntryEnd;
                }
                else
                {
                    //comment present
                    size_t commentStart = nx_http::find_first_not_of( strValue, " ", receivedByEnd+1 );
                    if( commentStart != nx_http::BufferNpos && commentStart < curEntryEnd )
                        entry.comment = strValue.mid( commentStart, curEntryEnd == nx_http::BufferNpos ? -1 : (curEntryEnd-commentStart) ); //space are allowed in comment
                }
                entry.receivedBy = strValue.mid( receivedByStart, receivedByEnd-receivedByStart );

                entries.push_back( entry );
            }

            return true;
        }

        StringType Via::toString() const
        {
            StringType result;

            //TODO #ak estimate required buffer size and allocate in advance

            for( const ProxyEntry& entry: entries )
            {
                if( !result.isEmpty() )
                    result += ", ";

                if( entry.protoName )
                {
                    result += entry.protoName.get();
                    result += "/";
                }
                result += entry.protoVersion;
                result += ' ';
                result += entry.receivedBy;
                if( !entry.comment.isEmpty() )
                {
                    result += ' ';
                    result += entry.comment;
                }
            }
            return result;
        }


        //////////////////////////////////////////////
        //   Keep-Alive
        //////////////////////////////////////////////

        KeepAlive::KeepAlive()
        :
            timeout(0)
        {
        }

        KeepAlive::KeepAlive(
            std::chrono::seconds _timeout,
            boost::optional<int> _max)
        :
            timeout(std::move(_timeout)),
            max(std::move(_max))
        {
        }

        bool KeepAlive::parse(const nx_http::StringType& strValue)
        {
            max.reset();

            const auto params = strValue.split(',');
            bool timeoutFound = false;
            for (auto param: params)
            {
                param = param.trimmed();
                const int sepPos = param.indexOf('=');
                if (sepPos == -1)
                    continue;
                const auto paramName = ConstBufferRefType(param, 0, sepPos);
                const auto paramValue = ConstBufferRefType(param, sepPos + 1).trimmed();

                if (paramName == "timeout")
                {
                    timeout = std::chrono::seconds(paramValue.toUInt());
                    timeoutFound = true;
                }
                else if(paramName == "max")
                {
                    max = paramValue.toUInt();
                }
            }

            return timeoutFound;
        }

        StringType KeepAlive::toString() const
        {
            StringType result = "timeout="+StringType::number((unsigned int)timeout.count());
            if (max)
                result += ", max="+StringType::number(*max);
            return result;
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


#if defined(_WIN32)
#   define COMMON_SERVER "Apache/2.4.16 (MSWin)"
#   if defined(_WIN64)
#       define COMMON_USER_AGENT "Mozilla/5.0 (Windows NT 6.1; WOW64)"
#   else
#       define COMMON_USER_AGENT "Mozilla/5.0 (Windows NT 6.1; Win32)"
#   endif
#elif defined(__linux__)
#   define COMMON_SERVER "Apache/2.4.16 (Unix)"
#   ifdef __LP64__
#       define COMMON_USER_AGENT "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:36.0)"
#   else
#       define COMMON_USER_AGENT "Mozilla/5.0 (X11; Ubuntu; Linux x86; rv:36.0)"
#   endif
#elif defined(__APPLE__)
#   define COMMON_SERVER "Apache/2.4.16 (Unix)"
#   define COMMON_USER_AGENT "Mozilla/5.0 (Macosx x86_64)"
#else   //some other unix (e.g., FreeBSD)
#   define COMMON_SERVER "Apache/2.4.16 (Unix)"
#   define COMMON_USER_AGENT "Mozilla/5.0"
#endif

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#   define PRODUCT_NAME_SUFFIX " Mobile"
#else
#   define PRODUCT_NAME_SUFFIX
#endif

    static const StringType defaultUserAgentString = QN_PRODUCT_NAME_LONG PRODUCT_NAME_SUFFIX "/" QN_APPLICATION_VERSION " (" QN_ORGANIZATION_NAME ") " COMMON_USER_AGENT;

    StringType userAgentString()
    {
        return defaultUserAgentString;
    }

    static const StringType defaultServerString = QN_PRODUCT_NAME "/" QN_APPLICATION_VERSION " (" QN_ORGANIZATION_NAME ") " COMMON_SERVER;

    StringType serverString()
    {
        return defaultServerString;
    }
}
