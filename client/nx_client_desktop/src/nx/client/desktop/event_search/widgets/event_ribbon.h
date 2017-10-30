#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/uuid.h>

class QAbstractItemModel;
class QScrollBar;

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

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    virtual QSize sizeHint() const override;

    QScrollBar* scrollBar() const;

signals:
    void closeRequested(const QnUuid& id);
    void linkActivated(const QnUuid& id, const QString& link);
    void clicked(const QnUuid& id);

protected:
    virtual bool event(QEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
