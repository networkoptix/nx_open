// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_log_session_model.h"

#include <client/client_globals.h>
#include <nx/vms/client/core/skin/color_theme.h>

using namespace nx::vms::client::core;

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
        case Qn::AuditLogChartDataRole:
        {
            const QnAuditRecord* record = rawData(index.row());
            return record->extractParam(ChildCntParamName).toUInt() / (qreal) m_maxActivity;
        }
        case Qt::ForegroundRole:
        {
            using namespace nx::vms::client::core;
            switch (column)
            {
                case UserActivityColumn:
                    return colorTheme()->color("auditLog.chart");

                case SelectRowColumn:
                case TimestampColumn:
                    break;

                default:
                {
                    const auto record = rawData(index.row());
                    // TODO: separate color for MitM attack.
                    if (record->eventType == Qn::AR_UnauthorizedLogin
                        || record->eventType == Qn::AR_MitmAttack)
                    {
                        return colorTheme()->color("auditLog.actions.failedLogin");
                    }

                    break;
                }
            }

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
