#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

#include <QObject>
#include "api/model/audit/audit_record.h"
#include "api/model/audit/auth_session.h"
#include "../../appserver2/src/transaction/transaction.h"

class QnAuditManager: public QObject
{
    Q_OBJECT
public:

    QnAuditManager();

    static QnAuditManager* instance();
public slots:
    virtual void addAuditRecord(const QnAuditRecord& record) = 0;
};
#define qnAuditManager QnAuditManager::instance()

#endif // __AUDIT_MANAGER_H__
