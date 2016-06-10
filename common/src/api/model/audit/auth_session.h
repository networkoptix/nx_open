#ifndef __QN_AUTH_SESSION_H__
#define __QN_AUTH_SESSION_H__

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnAuthSession
{
    QnUuid id;
    QString userName;
    QString userHost;
    QString userAgent;

    QByteArray toByteArray() const;
    void fromByteArray(const QByteArray& data);
};

#define QnAuthSession_Fields (id)(userName)(userHost)(userAgent)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuthSession), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))

void serialize_field(const QnAuthSession &authData, QVariant *target);
void deserialize_field(const QVariant &value, QnAuthSession *target);

#endif // __QN_AUTH_SESSION_H__
