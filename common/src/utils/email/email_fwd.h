#ifndef QN_EMAIL_FWD_H
#define QN_EMAIL_FWD_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(QnEmail, ConnectionType, )

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

