#pragma once

#include <common/common_globals.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/delegates/resource_tree_model_custom_column_delegate.h>

#include <nx/utils/uuid.h>

class QnFailoverPriorityResourceModelDelegate: public QnResourceTreeModelCustomColumnDelegate {
    Q_OBJECT

    typedef QnResourceTreeModelCustomColumnDelegate base_type;
public:
    QnFailoverPriorityResourceModelDelegate(QObject* parent = nullptr);
    virtual ~QnFailoverPriorityResourceModelDelegate();

    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    const QnFailoverPriorityColors &colors() const;
    void setColors(const QnFailoverPriorityColors &colors);

    void forceCamerasPriority(const QnVirtualCameraResourceList &cameras, Qn::FailoverPriority priority);
    QHash<QnUuid, Qn::FailoverPriority> forcedCamerasPriorities() const;
private:
    QnFailoverPriorityColors m_colors;
    QHash<QnUuid, Qn::FailoverPriority> m_forcedPriorities;
};
