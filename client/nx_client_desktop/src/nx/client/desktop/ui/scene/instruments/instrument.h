#pragma once

#include <QtCore/QObject>

class QQuickItem;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace scene {

class MouseEvent;

class Instrument: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

    using base_type = QObject;

public:
    Instrument(QObject* parent = nullptr);
    virtual ~Instrument() override;

    QQuickItem* item() const;
    void setItem(QQuickItem* item);

    bool enabled() const;
    void setEnabled(bool enabled);

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    static void registerQmlType();

signals:
    void itemChanged();
    void enabledChanged();

    void mousePress(nx::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseRelease(nx::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseMove(nx::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseDblClick(nx::client::desktop::ui::scene::MouseEvent* mouse);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
