/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPTYPES_H
#define HTTPTYPES_H

#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QUrl>

#include <nx/network/buffer.h>
#include <nx/utils/log/assert.h>

#include "qnbytearrayref.h"


/*!
    Holds http-implementation suitable for async/sync i/o.
    Common structures, parsers/serializers, http client.

    All classes use QByteArray for representing http headers and their values for following reasons:\n
        - HTTP headers are coded using 8-bit ASCII and since QString stores data as 16-bit chars we would need additional data
            conversion/copying to serialize header data to buffer, ready to be passed to socket
        - QByteArray is compatible with QString
*/
namespace nx_http
{
    const int DEFAULT_HTTP_PORT = 80;

    /*!
        TODO consider using another container.
        Need some buffer with:\n
            - implicit sharing
            - ability to hold reference to part of the source string (to perform substr with no allocation)
            - concatenate without relocation (hold list of buffers inside). This is suitable since socket write operation can accept array of buffers
            - pop_front operation with no relocation (just be moving internal offset)

        Using QByteArray or any STL container results in many memory copy/relocation operations which make implementation not effecient
    */
    typedef nx::Buffer BufferType;
    typedef QnByteArrayConstRef ConstBufferRefType;
    //TODO #ak not sure, if nx::Buffer is suitable for string type
    typedef nx::Buffer StringType;

    /*!
        \return < 0, if \a one < \a two. 0 if \a one == \a two. > 0 if \a one > \a two
    */
    int NX_NETWORK_API strcasecmp( const StringType& one, const StringType& two );

    /************************************************************************/
    /* Comparator for case-insensitive comparison in STL assos. containers  */
    /************************************************************************/
    struct NX_NETWORK_API ci_less
		: std::binary_function<QByteArray, QByteArray, bool>
    {
        // case-independent (ci) compare_less binary function
        bool operator() (const QByteArray& c1, const QByteArray& c2) const {
            return strcasecmp( c1, c2 ) < 0;
        }
    };

    //!Http header container
    /*!
        \warning This is multimap(!) to allow same header be present multiple times in single http message. To insert or replace use \a nx_http::insertOrReplace
    */
    typedef std::multimap<StringType, StringType, ci_less> HttpHeaders;
    typedef HttpHeaders::value_type HttpHeader;

    /*!
        This is convinient method for simplify transition from QHttp
        \return Value of header \a headerName (if found), empty string otherwise
    */
    StringType NX_NETWORK_API getHeaderValue( const HttpHeaders& headers, const StringType& headerName );

    //!convenient function for inserting or replacing header
    /*!
        \return iterator of added element
    */
    HttpHeaders::iterator NX_NETWORK_API insertOrReplaceHeader(
		HttpHeaders* const headers, const HttpHeader& newHeader );

    HttpHeaders::iterator NX_NETWORK_API insertHeader(
		HttpHeaders* const headers, const HttpHeader& newHeader );

    void NX_NETWORK_API removeHeader( HttpHeaders* const headers, const StringType& headerName );



    static const size_t BufferNpos = size_t(-1);


    /*!
        Searches first occurence of any element of \0-terminated string \a toSearch in \a count elements of \a str, starting with element \a offset
        \param toSearch \0-terminated string
        \param count number of characters of \a str to check, NOT a length of searchable characters string (\a toSearch)
        \note following algorithms differ from stl analogue in following: they limit number of characters checked during search
    */
    template<class Str>
        size_t find_first_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = str.data() + offset;
            curPos < strEnd;
            ++curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str>
        size_t find_first_not_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = str.data() + offset;
            curPos < strEnd;
            ++curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) == NULL )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str>
        size_t find_last_not_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = strEnd-1;
            curPos >= str.data();
            --curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) == NULL )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str, class StrValueType>
        size_t find_last_of(
            const Str& str,
            const StrValueType toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const StrValueType* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const StrValueType*
            curPos = strEnd-1;
            curPos >= str.data();
            --curPos )
        {
            if( toSearch == *curPos )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class MessageType, class MessageLineType>
    bool parseRequestOrResponse(
        const ConstBufferRefType& data,
        MessageType* message,
        MessageLineType MessageType::*messageLine,
        bool parseHeadersNonStrict = false);


    //!Parses \a data and saves header name and data to \a *headerName and \a *headerValue
    bool NX_NETWORK_API parseHeader(
        StringType* const headerName,
        StringType* const headerValue,
        const ConstBufferRefType& data );

	bool NX_NETWORK_API parseHeader(
        ConstBufferRefType* const headerName,
        ConstBufferRefType* const headerValue,
        const ConstBufferRefType& data );

    HttpHeader NX_NETWORK_API parseHeader( const ConstBufferRefType& data );

    namespace StatusCode
    {
        /*!
            enum has name "Value" for unification purpose only. Real enumeration name is StatusCode (name of namespace).
            Enum is referred to as StatusCode::Value, if assign real name to enum it will be StatusCode::StatusCode.
            Namespace is used to associate enum with corresponding functions like toString() and fromString().
            Namespace is required, since not all such functions can be put to global namespace.
            E.g. this namespace has convenience function toString( int httpStatusCode )
        */
        enum Value
        {
            undefined = 0,
            _continue = 100,
            upgrade = 101,

            ok = 200,
            noContent = 204,
            partialContent = 206,
            lastSuccessCode = 299,
            multipleChoices = 300,
            movedPermanently = 301,
            moved = 302,
            notModified = 304,

            badRequest = 400,
            unauthorized = 401,
            forbidden = 403,
            notFound = 404,
            notAllowed = 405,
            notAcceptable = 406,
            proxyAuthenticationRequired = 407,
            unsupportedMediaType = 415,
            rangeNotSatisfiable = 416,
            invalidParameter = 451,

            internalServerError = 500,
            notImplemented = 501,
            serviceUnavailable = 503
        };

        NX_NETWORK_API StringType toString(Value);
        NX_NETWORK_API StringType toString(int);
        /** Returns \a true if \a  statusCode is 2xx */
        NX_NETWORK_API bool isSuccessCode(Value statusCode);
        /** Returns \a true if \a  statusCode is 2xx */
        NX_NETWORK_API bool isSuccessCode(int statusCode);

        NX_NETWORK_API bool isMessageBodyAllowed(int statusCode);
    };

    class NX_NETWORK_API Method
    {
    public:
        typedef StringType ValueType;

        static const StringType GET;
        static const StringType HEAD;
        static const StringType POST;
        static const StringType PUT;
        static const StringType OPTIONS;
    };

    //!Represents string like HTTP/1.1, RTSP/1.0
    class NX_NETWORK_API MimeProtoVersion
    {
    public:
        StringType protocol;
        StringType version;

        MimeProtoVersion() = default;
        MimeProtoVersion(MimeProtoVersion&&) = default;
        MimeProtoVersion& operator=(MimeProtoVersion&&) = default;
        MimeProtoVersion(const MimeProtoVersion&) = default;
        MimeProtoVersion& operator=(const MimeProtoVersion&) = default;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;

        bool operator==( const MimeProtoVersion& right ) const
        {
            return protocol == right.protocol
                && version == right.version;
        }
        bool operator!=( const MimeProtoVersion& right ) const
        {
            return !(*this == right);
        }
    };

    static const MimeProtoVersion http_1_0 = { "HTTP", "1.0" };
    static const MimeProtoVersion http_1_1 = { "HTTP", "1.1" };

    class NX_NETWORK_API RequestLine
    {
    public:
        StringType method;
        QUrl url;
        MimeProtoVersion version;

        RequestLine() = default;
        RequestLine(RequestLine&&) = default;
        RequestLine& operator=(RequestLine&&) = default;
        RequestLine(const RequestLine&) = default;
        RequestLine& operator=(const RequestLine&) = default;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class NX_NETWORK_API StatusLine
    {
    public:
        MimeProtoVersion version;
        int statusCode;
        StringType reasonPhrase;

        StatusLine();
        StatusLine(StatusLine&&) = default;
        StatusLine& operator=(StatusLine&&) = default;
        StatusLine(const StatusLine&) = default;
        StatusLine& operator=(const StatusLine&) = default;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    void NX_NETWORK_API serializeHeaders( const HttpHeaders& headers, BufferType* const dstBuffer );

    class NX_NETWORK_API Request
    {
    public:
        RequestLine requestLine;
        HttpHeaders headers;
        BufferType messageBody;

        Request() = default;
        Request(Request&&) = default;
        Request& operator=(Request&&) = default;
        Request(const Request&) = default;
        Request& operator=(const Request&) = default;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        /*!
            \note Adds \r\n headers/body separator
        */
        void serialize( BufferType* const dstBuffer ) const;
        BufferType serialized() const;

        BufferType getCookieValue(const BufferType& name) const;
    };

    class NX_NETWORK_API Response
    {
    public:
        StatusLine statusLine;
        HttpHeaders headers;
        BufferType messageBody;

        Response() = default;
        Response(Response&&) = default;
        Response& operator=(Response&&) = default;
        Response(const Response&) = default;
        Response& operator=(const Response&) = default;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
        //!Appends serialized multipart data block to \a dstBuffer
        void serializeMultipartResponse( BufferType* const dstBuffer, const ConstBufferRefType& boundary ) const;
        BufferType toString() const;
        BufferType toMultipartString(const ConstBufferRefType& boundary) const;
    };

    class NX_NETWORK_API RtspResponse
    :
        public Response
    {
    public:
        bool parse(const ConstBufferRefType& data);
    };

    namespace MessageType
    {
        enum Value
        {
            none,
            request,
            response
        };

        QLatin1String NX_NETWORK_API toString( Value val );
    }

    class NX_NETWORK_API Message
    {
    public:
        MessageType::Value type;
        union
        {
            Request* request;
            Response* response;
        };

        Message( MessageType::Value _type = MessageType::none );
        Message( const Message& right );
        Message( Message&& right );
        ~Message();

        Message& operator=(const Message& right);
        Message& operator=(Message&& right);

        void serialize( BufferType* const dstBuffer ) const;

        void clear();
        HttpHeaders& headers() { return type == MessageType::request ? request->headers : response->headers; };
        const HttpHeaders& headers() const { return type == MessageType::request ? request->headers : response->headers; };
        BufferType toString() const;
    };

    //!Contains http header structures
    namespace header
    {
        /** common header name constants */
        extern NX_NETWORK_API const StringType kContentType;
        extern NX_NETWORK_API const StringType kUserAgent;


        //!Parses string "name1=val1; name2=val2; ...". ; separator can be specified
        void NX_NETWORK_API parseDigestAuthParams(
            const ConstBufferRefType& authenticateParamsStr,
            QMap<BufferType, BufferType>* const params,
            char sep = ',' );

        //!Http authentication scheme enumeration
        namespace AuthScheme
        {
            enum Value
            {
                none,
                basic,
                digest,
                automatic
            };

            NX_NETWORK_API const char* toString( Value val );
            NX_NETWORK_API Value fromString( const char* str );
            NX_NETWORK_API Value fromString( const ConstBufferRefType& str );
        }

        //!Login/password to use in http authorization
        class NX_NETWORK_API UserCredentials
        {
        public:
            StringType userid;
            StringType password;
        };

        //!rfc2617, section 2
        class NX_NETWORK_API BasicCredentials
        :
            public UserCredentials
        {
        public:
            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!rfc2617, section 3.2.2
        class NX_NETWORK_API DigestCredentials
        :
            public UserCredentials
        {
        public:
            QMap<BufferType, BufferType> params;

            bool parse( const BufferType& str, char separator = ',' );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!Authorization header ([rfc2616, 14.8], rfc2617)
        class NX_NETWORK_API Authorization
        {
        public:
            static const StringType NAME;

            AuthScheme::Value authScheme;
            union
            {
                BasicCredentials* basic;
                DigestCredentials* digest;
            };

            Authorization();
            Authorization( const AuthScheme::Value& authSchemeVal );
            Authorization( Authorization&& right );
            Authorization(const Authorization&);
            ~Authorization();

            Authorization& operator=( Authorization&& right );

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
            BufferType serialized() const;
            void clear();

            StringType userid() const;

        private:
            const Authorization& operator=( const Authorization& );
        };

        //!Convenient class for generating Authorization header with Basic authentication method
        class NX_NETWORK_API BasicAuthorization
        :
            public Authorization
        {
        public:
            BasicAuthorization( const StringType& userName, const StringType& userPassword );
        };

        class NX_NETWORK_API DigestAuthorization
        :
            public Authorization
        {
        public:
            DigestAuthorization();
            DigestAuthorization(DigestAuthorization&& right);
            DigestAuthorization(const DigestAuthorization& right);

            void addParam( const BufferType& name, const BufferType& value );
        };

        //![rfc2616, 14.47]
        class NX_NETWORK_API WWWAuthenticate
        {
        public:
            static const StringType NAME;

            AuthScheme::Value authScheme;
            QMap<BufferType, BufferType> params;

            WWWAuthenticate(AuthScheme::Value authScheme = AuthScheme::none);

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
            BufferType serialized() const;
        };

        //! identity
        static const StringType IDENTITY_CODING( "identity" );
        //! *
        static const StringType ANY_CODING( "*" );

        //![rfc2616, 14.3]
        class NX_NETWORK_API AcceptEncodingHeader
        {
        public:
            AcceptEncodingHeader( const nx_http::StringType& strValue );

            void parse( const nx_http::StringType& str );
            //!Returns \a true if \a encodingName is present in header and returns corresponding qvalue in \a *q (if not null)
            bool encodingIsAllowed( const nx_http::StringType& encodingName, double* q = nullptr ) const;

        private:
            //!map<coding, qvalue>
            std::map<nx_http::StringType, double> m_codings;
            boost::optional<double> m_anyCodingQValue;
        };

        /*!
            \note Boundaries are inclusive
        */
        class NX_NETWORK_API RangeSpec
        {
        public:
            quint64 start;
            boost::optional<quint64> end;

            RangeSpec()
            :
                start( 0 )
            {
            }
        };

        //![rfc2616, 14.35]
        class NX_NETWORK_API Range
        {
        public:
            Range();

            /*!
                \note In case of parse error, contents of this object are undefined
            */
            bool parse( const nx_http::StringType& strValue );

            //!Returns \a true if range is satisfiable for content of size \a contentSize
            bool validateByContentSize( size_t contentSize ) const;
            //!\a true, if empty range (does not include any bytes of content)
            bool empty() const;
            //!\a true, if range is full (includes all bytes of content of size \a contentSize)
            bool full( size_t contentSize ) const;
            //!Returns range length for content of size \a contentSize
            quint64 totalRangeLength( size_t contentSize ) const;

            std::vector<RangeSpec> rangeSpecList;
        };

        //![rfc2616, 14.16]
        class NX_NETWORK_API ContentRange
        {
        public:
            //!By default, bytes
            StringType unitName;
            boost::optional<quint64> instanceLength;
            RangeSpec rangeSpec;

            ContentRange();

            quint64 rangeLength() const;
            StringType toString() const;
        };

        //![rfc2616, 14.45]
        class NX_NETWORK_API Via
        {
        public:
            class ProxyEntry
            {
            public:
                boost::optional<StringType> protoName;
                StringType protoVersion;
                //!( host [ ":" port ] ) | pseudonym
                StringType receivedBy;
                StringType comment;
            };

            std::vector<ProxyEntry> entries;

            /*!
                \note In case of parse error, contents of this object are undefined
            */
            bool parse( const nx_http::StringType& strValue );
            StringType toString() const;
        };

        class NX_NETWORK_API KeepAlive
        {
        public:
            std::chrono::seconds timeout;
            boost::optional<int> max;

            KeepAlive();
            KeepAlive(
                std::chrono::seconds _timeout,
                boost::optional<int> _max = boost::none);

            bool parse(const nx_http::StringType& strValue);
            StringType toString() const;
        };
    }

    typedef std::pair<StringType, StringType> ChunkExtension;

    //! chunk-size [ chunk-extension ] CRLF
    /*!
        chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
        chunk-ext-name = token
        chunk-ext-val  = token | quoted-string
    */
    class NX_NETWORK_API ChunkHeader
    {
    public:
        size_t chunkSize;
        std::vector<ChunkExtension> extensions;

        ChunkHeader();

        void clear();

        /*!
            \return bytes read from \a buf. -1 in case of parse error
            \note In case of parse error object state is indefined
        */
        int parse( const ConstBufferRefType& buf );
        /*!
            \return bytes written to \a dstBuffer. -1 in case of serialize error. In this case contents of \a dstBuffer are undefined
        */
        int serialize( BufferType* const dstBuffer ) const;
    };

    //!Returns common value for User-Agent header
    StringType NX_NETWORK_API userAgentString();
    //!Returns common value for Server header
    StringType NX_NETWORK_API serverString();
}

#endif  //HTTPTYPES_H
