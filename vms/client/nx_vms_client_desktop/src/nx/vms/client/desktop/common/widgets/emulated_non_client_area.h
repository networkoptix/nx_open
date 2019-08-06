#pragma once

#include <string_view>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

/**
 * A client-area emulation of a non-client area (frame and title bar).
 * Provides hitTest method which can be used for possible integration with system window manager.
 */
class EmulatedNonClientArea: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit EmulatedNonClientArea(QWidget* parent = nullptr);
    virtual ~EmulatedNonClientArea() override;

    static EmulatedNonClientArea* create(QWidget* parent, QWidget* titleBar);

    QWidget* titleBar() const;
    void setTitleBar(QWidget* value); //< Takes ownership, reparents to this; deletes previous one.

    int visualFrameWidth() const;
    void setVisualFrameWidth(int value);

    bool hasFrameInFullScreen() const;
    void setHasFrameInFullScreen(bool value);

    int interactiveFrameWidth() const;
    void setInteractiveFrameWidth(int value);

    virtual Qt::WindowFrameSection hitTest(const QPoint& pos) const;

public:
    static constexpr std::string_view kParentPropertyName = "EmulatedNonClientArea";

protected:
    virtual bool event(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
