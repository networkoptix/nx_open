// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_AUDIT_LOG_MASTER_MODEL_H
#define QN_AUDIT_LOG_MASTER_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>

#include "audit_log_model.h"

class QnAuditLogMasterModel: public QnAuditLogModel
{
    Q_OBJECT
    typedef QnAuditLogModel base_type;
public:
    virtual void setData(const QnLegacyAuditRecordRefList &data) override;
    QnAuditLogMasterModel(QObject *parent = nullptr);
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual ~QnAuditLogMasterModel() {}
private:
    uint m_maxActivity;
};

#endif // QN_AUDIT_LOG_MASTER_MODEL_H
