/**********************************************************
* Dec 8, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_NOTIFICATION_H
#define NX_CDB_NOTIFICATION_H

#include <QtCore/QString>

#include <utils/common/model_functions_fwd.h>


namespace nx {
namespace cdb {

template<class MessageData>
struct Notification
{
    QString user_email;
    QString type;
    MessageData message;
};

#define Notification_Fields (user_email)(type)(message)

struct ActivateAccountData
{
    std::string code;
};

#define ActivateAccountData_Fields (code)


typedef Notification<ActivateAccountData> ActivateAccountNotification;

#define ActivateAccountNotification_Fields Notification_Fields


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ActivateAccountData)(ActivateAccountNotification),
    (json))

}   //cdb
}   //nx

#endif  //NX_CDB_NOTIFICATION_H
