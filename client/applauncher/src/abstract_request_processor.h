/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_REQUEST_PROCESSOR_H
#define ABSTRACT_REQUEST_PROCESSOR_H

#include <api/applauncher_api.h>


class AbstractRequestProcessor
{
public:
    virtual ~AbstractRequestProcessor() {}

    //!
    /*!
        It is recommended that implementation does not block!
    */
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response ) = 0;
};

#endif  //ABSTRACT_REQUEST_PROCESSOR_H
