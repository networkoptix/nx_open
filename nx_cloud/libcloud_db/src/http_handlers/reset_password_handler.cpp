/**********************************************************
* dec 9, 2015
* a.kolesnikov
***********************************************************/

#include "reset_password_handler.h"

#include <cloud_db_client/src/cdb_request_path.h>


namespace nx {
namespace cdb {

const QString ResetPasswordHttpHandler::kHandlerPath = QLatin1String(kAccountPasswordResetPath);

}   //cdb
}   //nx
