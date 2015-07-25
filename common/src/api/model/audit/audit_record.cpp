#include "audit_record.h"

QnByteArrayConstRef QnAuditRecord::extractParam(const QnLatin1Array& name) const
{
    int pos = params.indexOf(name);
    if (pos == -1)
        return QnLatin1Array();
    int pos2 = params.indexOf(pos, ';');
    if (pos2 == -1)
        pos2 = params.size();
    pos += name.size() + 1;
    return QnByteArrayConstRef(params, pos, pos2 - pos);
}
void QnAuditRecord::addParam(const QnLatin1Array& name, const QnLatin1Array& value)
{
    if (!params.isEmpty())
        params.append(';');
    params.append(name).append('=').append(value);
}
