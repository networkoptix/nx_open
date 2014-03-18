/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPTYPES_H
#define HTTPTYPES_H

#include <cstring>
#include <functional>
#include <map>

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QUrl>

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

        Using QByteArray or any STL container results in many memory copy/relocation which makes implementation not effective
    */
    typedef QByteArray BufferType;
    typedef QnByteArrayConstRef ConstBufferRefType;
    typedef QByteArray StringType;

    /*!
        \return < 0, if \a one < \a two. 0 if \a one == \a two. > 0 if \a one > \a two
    */
    int strcasecmp( const StringType& one, const StringType& two );

    /************************************************************************/
    /* Comparator for case-insensitive comparison in STL assos. containers  */
    /************************************************************************/
    struct ci_less : std::binary_function<QByteArray, QByteArray, bool>
    {
        // case-independent (ci) compare_less binary function
        bool operator() (const QByteArray& c1, const QByteArray& c2) const {
            return strcasecmp( c1, c2 ) < 0;
        }
    };

    typedef std::map<StringType, StringType, ci_less> HttpHeaders;
    typedef HttpHeaders::value_type HttpHeader;

    /*!
        This is convinient method for simplify transition from QHttp
        \return Value of header \a headerName (if found), empty string otherwise
    */
    StringType getHeaderValue( const HttpHeaders& headers, const StringType& headerName );

    static const size_t BufferNpos = size_t(-1);


    //following algorithms differ from stl analogue in following: they limit number of characters checked during search 
        //(last argument is the number of characters to check (str) but not length of searchable characters string (toSearch))
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


    //!Parses \a data and saves header name and data to \a *headerName and \a *headerValue
    bool parseHeader(
        StringType* const headerName,
        StringType* const headerValue,
        const ConstBufferRefType& data );
    bool parseHeader(
        ConstBufferRefType* const headerName,
        ConstBufferRefType* const headerValue,
        const ConstBufferRefType& data );

    namespace StatusCode
    {
        enum Value
        {
            undefined = 0,
            _continue = 100,
            ok = 200,
            noContent = 204,
            multipleChoices = 300,
            moved = 302,
            badRequest = 400,
            unauthorized = 401,
            notFound = 404,
            notAllowed = 405,
            internalServerError = 500,
            serviceUnavailable = 503,
            notImplemented = 501
        };

        StringType toString( Value );
        StringType toString( int );
    };

    namespace Method
    {
        extern const StringType GET;
        extern const StringType PUT;
    }

    namespace Version
    {
        extern const StringType http_1_0;
        extern const StringType http_1_1;
    }

    class RequestLine
    {
    public:
        StringType method;
        QUrl url;
        StringType version;

        bool parse( const ConstBufferRefType& data );
        void serialize( BufferType* const dstBuffer ) const;
    };

    class StatusLine
    {
    public:
        StringType version;
        int statusCode;
        StringType reasonPhrase;

        StatusLine();
        bool parse( const ConstBufferRefType& data );
        void serialize( BufferType* const dstBuffer ) const;
    };

    class HttpRequest
    {
    public:
        RequestLine requestLine;
        HttpHeaders headers;
        BufferType messageBody;

        bool parse( const ConstBufferRefType& data );
        void serialize( BufferType* const dstBuffer ) const;
    };

    class HttpResponse
    {
    public:
        StatusLine statusLine;
        HttpHeaders headers;
        BufferType messageBody;

        void serialize( BufferType* const dstBuffer ) const;
        BufferType toString() const;
    };

    namespace MessageType
    {
        enum Value
        {
            none,
            request,
            response
        };
    }

    class HttpMessage
    {
    public:
        MessageType::Value type;
        union
        {
            HttpRequest* request;
            HttpResponse* response;
        };

        HttpMessage( MessageType::Value _type = MessageType::none );
        HttpMessage( const HttpMessage& right );
        ~HttpMessage();

        HttpMessage& operator=( const HttpMessage& right );

        void clear();
        HttpHeaders& headers() { return type == MessageType::request ? request->headers : response->headers; };
        const HttpHeaders& headers() const { return type == MessageType::request ? request->headers : response->headers; };
    };

    //!Contains http header structures
    namespace Header
    {
        //!Http authentication scheme enumeration
        namespace AuthScheme
        {
            enum Value
            {
                none,
                basic,
                digest
            };

            const char* toString( Value val );
            Value fromString( const char* str );
            Value fromString( const ConstBufferRefType& str );
        }

        //!Login/password to use in http authorization
        class UserCredentials
        {
        public:
            StringType userid;
            StringType password;
        };

        //!rfc2617, section 2
        class BasicCredentials
        :
            public UserCredentials
        {
        public:
            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!rfc2617, section 3.2.2
        class DigestCredentials
        :
            public UserCredentials
        {
        public:
            QMap<BufferType, BufferType> params;

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!Authorization header (rfc2616, rfc2617)
        class Authorization
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
            ~Authorization();

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
            StringType toString() const;

        private:
            Authorization( const Authorization& );
            const Authorization& operator=( const Authorization& );
        };

        //!Convient class for generating Authorization header with Basic authentication method
        class BasicAuthorization
        :
            public Authorization
        {
        public:
            BasicAuthorization( const StringType& userName, const StringType& userPassword );
        };

        class DigestAuthorization
        :
            public Authorization
        {
        public:
            DigestAuthorization();

            void addParam( const BufferType& name, const BufferType& value );
        };

        class WWWAuthenticate
        {
        public:
            AuthScheme::Value authScheme;
            QMap<BufferType, BufferType> params;

            WWWAuthenticate();

            bool parse( const BufferType& str );
        };
    }
}

#endif  //HTTPTYPES_H
