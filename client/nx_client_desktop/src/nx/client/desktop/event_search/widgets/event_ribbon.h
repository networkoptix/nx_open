#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class EventTile;

class EventRibbon: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventRibbon(QWidget* parent = nullptr);
    virtual ~EventRibbon() override;

    void addTile(EventTile* tile, const QnUuid& id);
    void removeTile(const QnUuid& id);

    virtual QSize sizeHint() const override;

protected:
    virtual bool event(QEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
