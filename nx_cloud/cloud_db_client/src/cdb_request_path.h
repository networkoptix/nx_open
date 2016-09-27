/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_REQUEST_PATH_H
#define NX_CDB_CL_REQUEST_PATH_H


namespace nx {
namespace cdb {

extern const char* kAccountRegisterPath;
extern const char* kAccountActivatePath;
extern const char* kAccountGetPath;
extern const char* kAccountUpdatePath;
extern const char* kAccountPasswordResetPath;
extern const char* kAccountReactivatePath;
extern const char* kAccountCreateTemporaryCredentialsPath;

extern const char* kSystemBindPath;
extern const char* kSystemUnbindPath;
extern const char* kSystemGetPath;
extern const char* kSystemSharePath;
extern const char* kSystemGetCloudUsersPath;
extern const char* kSystemGetAccessRoleListPath;
extern const char* kSystemRenamePath;
extern const char* kSystemStartUserSessionPath;

extern const char* kAuthGetNoncePath;
extern const char* kAuthGetAuthenticationPath;

extern const char* kSubscribeToSystemEventsPath;

extern const char* kPingPath;

extern const char* kEstablishEc2TransactionConnectionPath;
extern const char* kPushEc2TransactionPath;

/** Maintenance. */
extern const char* kMaintenanceGetVmsConnections;
extern const char* kMaintenanceGetTransactionLog;

}   //cdb
}   //nx

#endif //NX_CDB_CL_REQUEST_PATH_H
