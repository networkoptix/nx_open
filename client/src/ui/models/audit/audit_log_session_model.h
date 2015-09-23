#ifndef QN_AUDIT_LOG_MASTER_MODEL_H
#define QN_AUDIT_LOG_MASTER_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "audit_log_model.h"

class QnAuditLogMasterModel: public QnAuditLogModel
{
    Q_OBJECT
    typedef QnAuditLogModel base_type;
public:
    virtual void setData(const QnAuditRecordRefList &data) override;
    QnAuditLogMasterModel(QObject *parent = NULL);
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual ~QnAuditLogMasterModel() {}
private:
    uint m_maxActivity;
};

#endif // QN_AUDIT_LOG_MASTER_MODEL_H
