/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPTYPES_H
#define HTTPTYPES_H

#include <map>

#include <QByteArray>
#include <QUrl>

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
    typedef QByteArray BufferType;
    typedef QnByteArrayConstRef ConstBufferRefType;
    typedef QByteArray StringType;

    typedef std::multimap<StringType, StringType> HttpHeaders;
    typedef HttpHeaders::value_type HttpHeader;

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


    bool parseHeader(
        StringType* const headerName,
        StringType* const headerValue,
        const ConstBufferRefType& data );

    namespace StatusCode
    {
        enum Value
        {
            undefined = 0,
            _continue = 100,
            ok = 200,
            multipleChoices = 300,
            badRequest = 400,
            forbidden = 403,
            notFound = 404,
            internalServerError = 500,
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
        //!Appends serialized data to \a dstBuffer
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
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class Request
    {
    public:
        RequestLine requestLine;
        HttpHeaders headers;
        BufferType messageBody;

        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class Response
    {
    public:
        StatusLine statusLine;
        HttpHeaders headers;
        BufferType messageBody;

        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    namespace MessageType
    {
        enum Value
        {
            none,
            request,
            response
        };

        QLatin1String toString( Value val );
    }

    class Message
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
        ~Message();

        Message& operator=( const Message& right );

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
			//TODO

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
    }
}

#endif  //HTTPTYPES_H
