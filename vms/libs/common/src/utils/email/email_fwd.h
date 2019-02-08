#ifndef QN_EMAIL_FWD_H
#define QN_EMAIL_FWD_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <nx/vms/api/types/smtp_types.h>

namespace QnEmail {

using ConnectionType = nx::vms::api::ConnectionType;

} // namespace QnEmail

struct QnEmailSmtpServerPreset;
struct QnEmailSettings;
class  QnEmailAddress;

enum class SmtpReplyCode;
enum class SmtpError;
struct SmtpOperationResult;

#endif // QN_EMAIL_FWD_H

