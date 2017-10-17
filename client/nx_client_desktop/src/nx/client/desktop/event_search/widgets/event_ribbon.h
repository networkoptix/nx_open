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

    void insertTile(int index, EventTile* tile);
    void removeTiles(int first, int count);

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
