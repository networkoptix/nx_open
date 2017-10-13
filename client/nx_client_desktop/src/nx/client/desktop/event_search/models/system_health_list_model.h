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

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
