#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/event_search/models/event_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class SystemHealthListModel: public EventListModel
{
    Q_OBJECT
    using base_type = EventListModel;

public:
    explicit SystemHealthListModel(QObject* parent = nullptr);
    virtual ~SystemHealthListModel() override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

protected:
    virtual int eventPriority(const EventData& event) const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
