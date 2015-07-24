#ifndef QN_AUDIT_LOG_SESSION_MODEL_H
#define QN_AUDIT_LOG_SESSION_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "audit_log_model.h"

class QnAuditLogSessionModel: public QnAuditLogModel
{
    Q_OBJECT
    typedef QnAuditLogModel base_type;

public:
    QnAuditLogSessionModel(QObject *parent = NULL): QnAuditLogModel(parent) {}

    virtual ~QnAuditLogSessionModel() {}
};

#endif // QN_AUDIT_LOG_SESSION_MODEL_H
