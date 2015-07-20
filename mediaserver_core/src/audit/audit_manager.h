#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

#include <QObject>
#include "api/model/audit/audit_record.h"

class QnAuditManager: public QObject
{
    Q_OBJECT
public:
public slots:
    void at_auditRecord(const QnAuditRecord& record);
};

#endif // __AUDIT_MANAGER_H__
