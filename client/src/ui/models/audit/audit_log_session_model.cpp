#include "audit_log_session_model.h"

QVariant QnAuditLogSessionModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::ForegroundRole)
        return base_type::data(index, role);
    
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];
    const QnAuditRecord* record = rawData(index.row());
    if (record->eventType == Qn::AR_UnauthorizedLogin && column != SelectRowColumn && column != TimestampColumn)
        return m_colors.unsucessLoginAction;
    else
        return QVariant();
}
