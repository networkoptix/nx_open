#ifndef QN_EMAIL_FWD_H
#define QN_EMAIL_FWD_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

#ifdef Q_MOC_RUN
class QnEmail
#else
namespace QnEmail
#endif
{
#ifdef Q_MOC_RUN
    Q_GADGET
    Q_ENUMS(ConnectionType)
public:
#else
    Q_NAMESPACE
#endif

    enum ConnectionType {
        Unsecure = 0,
        Ssl = 1,
        Tls = 2
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ConnectionType)

} // namespace QnEmail

struct QnEmailSmtpServerPreset;
struct QnEmailSettings;
class  QnEmailAddress;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnEmail::ConnectionType), (metatype)(lexical))

#endif // QN_EMAIL_FWD_H

