#pragma once

#include "abstract_event_list_model.h"

#include <QtCore/QScopedPointer>

namespace nx::vms::client::desktop {

class SystemHealthListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    explicit SystemHealthListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~SystemHealthListModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

protected:
    virtual bool defaultAction(const QModelIndex& index) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
