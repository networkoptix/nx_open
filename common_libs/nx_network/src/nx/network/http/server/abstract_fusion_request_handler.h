/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_ABSTRACT_FUSION_REQUEST_HANDLER_H
#define NX_ABSTRACT_FUSION_REQUEST_HANDLER_H

#include "detail/abstract_fusion_request_handler_detail.h"


namespace nx_http {

//!HTTP server request handler which deserializes input/serializes output data using fusion
/*!
    Reads \a format query parameter from incoming request and deserializes request/serializes response accordingly.
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
        On request processing completion \a requestCompleted(FusionRequestResult, Output) MUST be invoked
        \note If \a Input is \a void, then this method does not have \a inputData argument
        \note If \a Output is \a void, then \a requestCompleted(FusionRequestResult)
    */
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo,
        Input inputData ) = 0;

    //!Call this method when processed request. \a outputData argument is missing when \a Output is \a void
    /*!
        This method is here just for information purpose. Defined in a base class
    */
    //void requestCompleted(
    //    FusionRequestResult result,
    //    Output outputData );
};

/*!
    Partial specialization with no input
*/
template<typename Output>
class AbstractFusionRequestHandler<void, Output>
:
    public detail::BaseFusionRequestHandlerWithOutput<Output>
{
public:
    //!Implement this method in a descendant
    /*!
        On request processing completion \a requestCompleted(FusionRequestResult) MUST be invoked
    */
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo ) = 0;

private:
    //!Implementation of \a AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler ) override
    {
        this->m_completionHandler = std::move( completionHandler );
        this->m_requestMethod = request.requestLine.method;

        if( !this->getDataFormat( request ) )
            return;

        processRequest(
            connection,
            request,
            std::move( authInfo ) );
    }
};

}   //nx_http

#endif  //NX_ABSTRACT_FUSION_REQUEST_HANDLER_H
