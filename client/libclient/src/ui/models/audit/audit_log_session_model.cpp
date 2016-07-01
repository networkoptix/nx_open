#include "audit_log_session_model.h"

QnAuditLogMasterModel::QnAuditLogMasterModel(QObject *parent): 
    QnAuditLogModel(parent), 
    m_maxActivity(0)
{

}

QVariant QnAuditLogMasterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];

    switch (role)
    {
        case Qt::BackgroundColorRole:
            if (column == UserActivityColumn)
                return m_colors.chartColor;
            else
                return base_type::data(index, role);
        case Qn::AuditLogChartDataRole:
        {
            const QnAuditRecord* record = rawData(index.row());
            return record->extractParam(ChildCntParamName).toUInt() / (qreal) m_maxActivity;
        }
        case Qt::ForegroundRole:
        {
            const QnAuditRecord* record = rawData(index.row());
            if (record->eventType == Qn::AR_UnauthorizedLogin && column != SelectRowColumn && column != TimestampColumn)
                return m_colors.unsucessLoginAction;
            else
                return QVariant();

        }
        default:
            return base_type::data(index, role);
    }
}

void QnAuditLogMasterModel::setData(const QnAuditRecordRefList &data)
{
    base_type::setData(data);
    m_maxActivity = 0;
    for (const auto& record: data)
    {
        m_maxActivity = qMax(m_maxActivity, record->extractParam(ChildCntParamName).toUInt());
    }
}
