#include "audit_log_detail_model.h"

int QnAuditLogDetailModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 5;
}
