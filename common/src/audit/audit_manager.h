#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

#include <QObject>
#include "api/model/audit/audit_record.h"
#include "api/model/audit/auth_session.h"

class QnAuditManager: public QObject
{
    Q_OBJECT
public:

    QnAuditManager();

    static QnAuditManager* instance();
public:
    QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType);
    virtual int addAuditRecord(const QnAuditRecord& record) = 0;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record) = 0;
public slots:
    void at_connectionOpened(const QnAuthSession &data);
    void at_connectionClosed(const QnAuthSession &data);
protected:
    /* return internal id of inserted record. Returns <= 0 if error */
private:

    struct AuditConnection
    {
        AuditConnection(): internalId(0) {}

        QnAuditRecord record;
        int internalId;
    };

    QMap<QnUuid, AuditConnection> m_openedConnections;
    mutable QMutex m_mutex;
};
#define qnAuditManager QnAuditManager::instance()

#endif // __AUDIT_MANAGER_H__
