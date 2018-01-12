#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QTabBar>

namespace nx {
namespace client {
namespace desktop {

/**
A custom tab bar that draws icon and text for the current tab, but only icons for all other tabs.

Icons are mandatory for all tabs; if no icon is specified a warning placeholder is drawn instead.
Tab switching is animated. Only horizontal mode is supported. All painting bypasses QStyle.
*/
class CompactTabBar: public QTabBar
{
    Q_OBJECT
    using base_type = QTabBar;

public:
    explicit CompactTabBar(QWidget* parent = nullptr);
    virtual ~CompactTabBar() override = default;

    virtual bool event(QEvent* event) override;

signals:
    void tabHoverChanged(int hoveredTabIndex); //< -1 if no tab is hovered.

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual QSize tabSizeHint(int index) const override;
    virtual QSize minimumTabSizeHint(int index) const override;

    virtual void tabInserted(int index) override;
    virtual void tabRemoved(int index) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
