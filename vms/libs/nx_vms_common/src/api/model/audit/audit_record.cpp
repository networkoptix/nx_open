// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_record.h"

void QnAuditRecord::removeParam(const QnLatin1Array& name)
{
    int pos = params.indexOf(name);
    if (pos == -1)
        return;
    int pos2 = params.indexOf(pos, ';');
    if (pos2 == -1)
        pos2 = params.size();
    if (pos > 0)
        --pos;
    params.remove(pos, pos2 - pos);
}

QnByteArrayConstRef QnAuditRecord::extractParam(const QnLatin1Array& name) const
{
    int pos = params.indexOf(name);
    if (pos == -1)
        return QnByteArrayConstRef();
    int pos2 = params.indexOf(';', pos);
    if (pos2 == -1)
        pos2 = params.size();
    pos += name.size() + 1;
    return QnByteArrayConstRef(params, pos, pos2 - pos);
}
void QnAuditRecord::addParam(const QnLatin1Array& name, const QnLatin1Array& value)
{
    if (!params.isEmpty()) {
        removeParam(name);
        if (!params.isEmpty())
            params.append(';');
    }
    params.append(name).append('=').append(value);
}
