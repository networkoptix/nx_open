#include "save_user_ex_rest_handler.h"
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/user_data.h>

QnSaveUserExRestHandler::QnSaveUserExRestHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{}

int QnSaveUserExRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    using namespace nx::vms::api;

    bool success;
    UserDataEx userDataEx = QJson::deserialized<nx::vms::api::UserDataEx>(body);
    //UserDataEx userDataEx;
    //if (!QJson::deserialize<UserDataEx>(body, &userDataEx))
    //    return nx::network::http::StatusCode::badRequest;

    UserData userData;
    userData.id = userDataEx.id;
    userData.name = userDataEx.name;
    userData.permissions = userDataEx.permissions;
    userData.email = userDataEx.email;
    userData.isCloud = userDataEx.isCloud;
    userData.isLdap = userDataEx.isLdap;
    userData.isEnabled = userDataEx.isEnabled;
    userData.userRoleId = userDataEx.userRoleId;
    userData.fullName = userDataEx.fullName;


    return nx::network::http::StatusCode::ok;
}

