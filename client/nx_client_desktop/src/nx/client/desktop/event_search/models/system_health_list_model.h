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

protected:
    virtual void triggerDefaultAction(const EventData& event) override;
    virtual void triggerCloseAction(const EventData& event) override;

    virtual void beforeRemove(const EventData& event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
