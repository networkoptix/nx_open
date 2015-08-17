/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_ABSTRACT_FUSION_REQUEST_HANDLER_H
#define NX_ABSTRACT_FUSION_REQUEST_HANDLER_H

#include "detail/abstract_fusion_request_handler_detail.h"


namespace nx_http {

//!Reads "format" parameter from incoming request and deserializes request/serializes response accordingly
/*!
    \note \a Input or \a Output can be void. If \a Input is void, 
        \a AbstractFusionRequestHandler::processRequest does not have \a inputData argument.
        If \a Output is void, \a AbstractFusionRequestHandler::requestCompleted does not have \a outputData argument
    \note GET request input data is always deserialized from url query
    \note POST request output data is ignored
*/
template<typename Input = void, typename Output = void>
class AbstractFusionRequestHandler
:
    public detail::BaseFusionRequestHandlerWithInput<
        Input,
        Output,
        AbstractFusionRequestHandler<Input, Output>>
{
public:
    //!Implement this method in a descendant
    /*!
        On request processing completion \a requestCompleted( nx_http::StatusCode::Value, Output ) MUST be invoked
        \note If \a Input is \a void, then this method does not have \a inputData argument
    */
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        Input inputData ) = 0;

    //!Call this method when processed request. \a outputData argument is missing when \a Output is \a void
    /*!
        This method is here just for information purpose. Defined in a base class
    */
    //void requestCompleted(
    //    const nx_http::StatusCode::Value statusCode,
    //    Output outputData );
};

/*!
    Partial specifalization with no input
*/
template<typename Output>
class AbstractFusionRequestHandler<void, Output>
:
    public detail::BaseFusionRequestHandlerWithOutput<Output>
{
public:
    //!Implement this method in a descendant
    /*!
        On request processing completion \a requestCompleted( nx_http::StatusCode::Value ) MUST be invoked
    */
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo ) = 0;

private:
    //!Implementation of \a AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& request,
        nx_http::Response* const /*response*/,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override
    {
        this->m_completionHandler = std::move( completionHandler );
        this->m_requestMethod = request.requestLine.method;

        if( !this->getDataFormat( request ) )
            return;

        processRequest(
            connection,
            std::move( authInfo ) );
    }
};

}   //nx_http

#endif  //NX_ABSTRACT_FUSION_REQUEST_HANDLER_H
