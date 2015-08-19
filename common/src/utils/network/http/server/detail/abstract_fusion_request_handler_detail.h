/**********************************************************
* Aug 13, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_ABSTRACT_FUSION_REQUEST_HANDLER_DETAIL_H
#define NX_ABSTRACT_FUSION_REQUEST_HANDLER_DETAIL_H

#include "../abstract_http_request_handler.h"

#include <type_traits>

#include <utils/serialization/json.h>


namespace nx_http {
namespace detail {

class BaseFusionRequestHandler
:
    public AbstractHttpRequestHandler
{
public:
    BaseFusionRequestHandler()
    :
        m_outputDataFormat( Qn::JsonFormat )  //by default, using json
    {
    }

protected:
    std::function<void(
        const nx_http::StatusCode::Value,
        std::unique_ptr<nx_http::AbstractMsgBodySource> )> m_completionHandler;
    Qn::SerializationFormat m_outputDataFormat;
    nx_http::Method::ValueType m_requestMethod;

    //!Call this method after request has been processed
    void requestCompleted(
        nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> outputMsgBody =
            std::unique_ptr<nx_http::AbstractMsgBodySource>() )
    {
        auto completionHandler = std::move( m_completionHandler );
        completionHandler(
            statusCode,
            std::move(outputMsgBody) );
    }

    bool getDataFormat(
        const nx_http::Request& request,
        Qn::SerializationFormat* const inputDataFormat = nullptr )
    {
        //input/output formats can differ. E.g., GET request can specify input data in url query only
        //TODO #ak fetching m_outputDataFormat from url query
        //if( no format in query )
        //{
        if( request.requestLine.method == nx_http::Method::GET )
        {
            if( inputDataFormat )
            {
                //TODO #ak fetching format from Accept header
                *inputDataFormat = Qn::UrlQueryFormat;
            }
        }
        else    //POST, PUT
        {
            const auto contentTypeIter = request.headers.find( "Content-Type" );
            if( contentTypeIter != request.headers.cend() )
            {
                //TODO #ak add Content-Type header parser to nx_http
                const QByteArray dataFormatStr = contentTypeIter->second.split( ';' )[0];
                m_outputDataFormat = Qn::serializationFormatFromHttpContentType( dataFormatStr );
            }
            if( inputDataFormat )
                *inputDataFormat = m_outputDataFormat;
        }
        //}

        if( (inputDataFormat && !isFormatSupported( *inputDataFormat )) ||
            !isFormatSupported( m_outputDataFormat ) )
        {
            requestCompleted(
                nx_http::StatusCode::notAcceptable,
                std::unique_ptr<nx_http::AbstractMsgBodySource>() );
            return false;
        }

        return true;
    }

    template<class T>
    bool parseAnyFusionFormat(
        Qn::SerializationFormat dataFormat,
        const nx::Buffer& data,
        T* const target )
    {
        bool success = false;
        switch( dataFormat )
        {
            case Qn::UrlQueryFormat:
                return loadFromUrlQuery(
                    QUrlQuery( data ),
                    target );
                return true;

            case Qn::JsonFormat:
                *target = QJson::deserialized<T>( data, T(), &success );
                return success;

                //case Qn::UbjsonFormat:
                //    *target = QnUbjson::deserialized<T>( data, T(), &success );
                //    return success;

            default:
                return false;
        }
    }

    template<class T>
    bool serializeToAnyFusionFormat(
        const T& data,
        Qn::SerializationFormat dataFormat,
        nx::Buffer* const output )
    {
        switch( dataFormat )
        {
            case Qn::UrlQueryFormat:
                assert( false );
                return true;

            case Qn::JsonFormat:
                *output = QJson::serialized( data );
                return true;

                //case Qn::UbjsonFormat:
                //    *output = QnUbjson::serialized<T>( data );
                //    return true;

            default:
                return false;
        }
    }

    bool isFormatSupported( Qn::SerializationFormat dataFormat ) const
    {
        return dataFormat == Qn::JsonFormat || dataFormat == Qn::UrlQueryFormat;
    }
};


template<typename Output>
class BaseFusionRequestHandlerWithOutput
:
    public BaseFusionRequestHandler
{
public:
    //!Call this method after request has been processed
    /*!
        \note This overload is for requests returning something
    */
    void requestCompleted(
        nx_http::StatusCode::Value statusCode,
        Output outputData = Output() )
    {
        std::unique_ptr<nx_http::AbstractMsgBodySource> outputMsgBody;

        if( statusCode / 100 == nx_http::StatusCode::ok / 100 ) //success
        {
            //requests returning some data MUST be issues with GET method
            Q_ASSERT( m_requestMethod == nx_http::Method::GET ); 

            //serializing outputData
            nx::Buffer serializedData;
            if( serializeToAnyFusionFormat( outputData, m_outputDataFormat, &serializedData ) )
            {
                outputMsgBody = std::make_unique<nx_http::BufferSource>(
                    Qn::serializationFormatToHttpContentType( m_outputDataFormat ),
                    std::move( serializedData ) );
            }
            else
            {
                statusCode = nx_http::StatusCode::internalServerError;
            }
        }

        BaseFusionRequestHandler::requestCompleted(
            statusCode,
            std::move(outputMsgBody) );
    }
};


template<>
class BaseFusionRequestHandlerWithOutput<void>
:
    public BaseFusionRequestHandler
{
public:
};

/*!
    \note for requests that set Input to non-void
*/
template<typename Input, typename Output, typename Descendant>
class BaseFusionRequestHandlerWithInput
:
    public BaseFusionRequestHandlerWithOutput<Output>
{
private:
    typedef std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )
    > RequestCompletionHandlerType;

    //!Implementation of \a AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& request,
        nx_http::Response* const /*response*/,
        RequestCompletionHandlerType&& completionHandler ) override
    {
        this->m_completionHandler = std::move( completionHandler );
        this->m_requestMethod = request.requestLine.method;

        Qn::SerializationFormat inputDataFormat = this->m_outputDataFormat;
        if( !this->getDataFormat( request, &inputDataFormat ) )
            return;

        //parse request message body using fusion
        Input inputData;
        if( !this->template parseAnyFusionFormat<Input>(
                inputDataFormat,
                request.requestLine.method == nx_http::Method::GET
                    ? request.requestLine.url.query().toUtf8()
                    : request.messageBody,
                &inputData ) )
        {
            this->requestCompleted( nx_http::StatusCode::badRequest );
            return;
        }

        static_cast<Descendant*>(this)->processRequest(
            connection,
            std::move( authInfo ),
            std::move( inputData ) );
    }
};

}   //detail
}   //nx_http

#endif //NX_ABSTRACT_FUSION_REQUEST_HANDLER_DETAIL_H
