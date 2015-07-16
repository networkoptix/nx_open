/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_add_account_handler_h
#define cloud_db_add_account_handler_h

#include <memory>

#include <QString>

#include "access_control/types.h"
#include "base_http_handler.h"
#include "managers/types.h"


namespace cdb_api
{
    class AddAccountHttpHandler
    :
        public BaseHttpHandler
    {
    public:
        static const QString HANDLER_PATH;

    protected:
        virtual void processRequest(
            const AuthorizationInfo& authzInfo,
            const stree::AbstractResourceReader& inputParams,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override;

    private:
        void addAccountDone(
            ResultCode resultCode,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler );
    };
}

#endif  //cloud_db_add_account_handler_h
