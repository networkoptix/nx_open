#ifndef QN_AUDIT_LOG_DETAIL_MODEL_H
#define QN_AUDIT_LOG_DETAIL_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "audit_log_model.h"

class QnAuditLogDetailModel: public QnAuditLogModel
{
    Q_OBJECT
    typedef QnAuditLogModel base_type;

public:
    QnAuditLogDetailModel(QObject *parent = NULL): QnAuditLogModel(parent) {}

    virtual ~QnAuditLogDetailModel() {}
};

#endif // QN_AUDIT_LOG_DETAIL_MODEL_H
