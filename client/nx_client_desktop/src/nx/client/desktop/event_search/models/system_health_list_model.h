#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class SystemHealthListModel: public AbstractSearchListModel
{
    Q_OBJECT
    using base_type = AbstractSearchListModel;

public:
    explicit SystemHealthListModel(QObject* parent = nullptr);
    virtual ~SystemHealthListModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
