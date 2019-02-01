#include "emulated_non_client_area.h"

#include <array>

#include <QtCore/QtGlobal>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <ui/style/helper.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// EmulatedNonClientArea::Private

class EmulatedNonClientArea::Private: public QObject
{
    EmulatedNonClientArea* const q;

    using base_type = QObject;

public:
    Private(EmulatedNonClientArea* q): q(q)
    {
        NX_CRITICAL(!q->parent(), "Parent must be set only after Private is created.");
        updateInternalMargins();
    }

    void updateParent(QWidget* value)
    {
        if (m_parentWidget == value)
            return;

        if (m_parentWidget)
        {
            m_parentWidget->setProperty(kParentPropertyName.data(), {});
            m_parentWidget->setContentsMargins({});
        }

        m_parentWidget = value;

        if (m_parentWidget)
        {
            m_parentWidget->setProperty(kParentPropertyName.data(), QVariant::fromValue(q));
            updateParentContentsMargins();
        }
    }

    void updateParentContentsMargins()
    {
        if (!m_parentWidget)
            return;

        m_parentWidget->setContentsMargins(q->isHidden()
            ? QMargins()
            : q->contentsMargins());
    }

    QWidget* titleBar() const
    {
        return m_titleBar;
    }

    void setTitleBar(QWidget* value)
    {
        if (m_titleBar == value)
            return;

        if (m_titleBar)
        {
            m_titleBar->disconnect(this);
            m_titleBar->removeEventFilter(this);
        }

        delete m_titleBar;
        NX_ASSERT(!m_titleAnchor); //< Was a child of m_titleBar.

        m_titleBar = value;

        if (m_titleBar)
        {
            m_titleBar->setParent(q);
            m_titleBar->installEventFilter(this);
            connect(m_titleBar.data(), &QObject::destroyed, this, &Private::updateInternalMargins);

            m_titleAnchor = anchorWidgetToParent(m_titleBar,
                Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge,
                frame());
        }

        updateInternalMargins();
    }

    int frameWidth() const
    {
        return m_frameWidth;
    }

    int effectiveFrameWidth() const
    {
        return (m_hasFrameInFullScreen || !isFullScreen()) ? m_frameWidth : 0;
    }

    void setFrameWidth(int value)
    {
        if (m_frameWidth == value)
            return;

        m_frameWidth = value;
        updateInternalMargins();
    }

    bool hasFrameInFullScreen() const
    {
        return m_hasFrameInFullScreen;
    }

    void setHasFrameInFullScreen(bool value)
    {
        if (m_hasFrameInFullScreen == value)
            return;

        m_hasFrameInFullScreen = value;
        updateInternalMargins();
    }

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_titleBar)
            return QObject::eventFilter(watched, event);

        switch (event->type())
        {
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            {
                const auto minHeight = qMax(m_titleBar->minimumHeight(),
                    m_titleBar->minimumSizeHint().height());

                const auto desiredHeight = m_titleBar->hasHeightForWidth()
                    ? m_titleBar->heightForWidth(m_titleBar->width())
                    : m_titleBar->sizeHint().height();

                const auto maxHeight = m_titleBar->maximumHeight();

                m_titleBar->resize(m_titleBar->width(),
                    qBound(minHeight, desiredHeight, maxHeight));

                updateInternalMargins();
                break;
            }

            case QEvent::ParentChange: //< Just for safety.
            {
                m_titleBar->removeEventFilter(this);
                m_titleBar = nullptr;
                delete m_titleAnchor;
                updateInternalMargins();
                break;
            }

            default:
                break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QMargins frame() const
    {
        const int frameWidth = effectiveFrameWidth();
        return QMargins(frameWidth, frameWidth, frameWidth, frameWidth);
    }

    QMargins title() const
    {
        const int titleHeight = m_titleBar ? m_titleBar->height() : 0;
        return QMargins(0, titleHeight, 0, 0);
    }

    void updateInternalMargins()
    {
        if (m_titleAnchor)
            m_titleAnchor->setMargins(frame());

        q->setContentsMargins(frame() + title());
    }

    bool isFullScreen() const
    {
        return q->window() && q->window()->isFullScreen();
    }

public:
    int interactiveFrameWidth = style::Metrics::kStandardPadding;

private:
    QPointer<QWidget> m_parentWidget;

    QPointer<QWidget> m_titleBar;
    QPointer<WidgetAnchor> m_titleAnchor;

    int m_frameWidth = 1;
    bool m_hasFrameInFullScreen = false;
};

// ------------------------------------------------------------------------------------------------
// EmulatedNonClientArea

EmulatedNonClientArea::EmulatedNonClientArea(QWidget* parent):
    base_type(),
    d(new Private(this))
{
    setParent(parent); //< Parent must be set after Private initialization.
    anchorWidgetToParent(this);
}

EmulatedNonClientArea::~EmulatedNonClientArea()
{
    // Required here for forward-declared scoped pointer destruction.
}

EmulatedNonClientArea* EmulatedNonClientArea::create(QWidget* parent, QWidget* titleBar)
{
    const auto result = new EmulatedNonClientArea(parent);
    result->setTitleBar(titleBar);
    return result;
}

QWidget* EmulatedNonClientArea::titleBar() const
{
    return d->titleBar();
}

void EmulatedNonClientArea::setTitleBar(QWidget* value)
{
    d->setTitleBar(value);
}

int EmulatedNonClientArea::visualFrameWidth() const
{
    return d->frameWidth();
}

void EmulatedNonClientArea::setVisualFrameWidth(int value)
{
    d->setFrameWidth(qMax(value, 0));
}

bool EmulatedNonClientArea::hasFrameInFullScreen() const
{
    return d->hasFrameInFullScreen();
}

void EmulatedNonClientArea::setHasFrameInFullScreen(bool value)
{
    d->setHasFrameInFullScreen(value);
}

int EmulatedNonClientArea::interactiveFrameWidth() const
{
    return d->interactiveFrameWidth;
}

void EmulatedNonClientArea::setInteractiveFrameWidth(int value)
{
    d->interactiveFrameWidth = qMax(value, 1);
}

Qt::WindowFrameSection EmulatedNonClientArea::hitTest(const QPoint& pos) const
{
    if (isHidden() || !rect().contains(pos))
        return Qt::NoSection;

    const auto titleBar = d->titleBar();
    const bool insideTitleBar = titleBar && NX_ASSERT(titleBar->parent() == this)
        && titleBar->geometry().contains(pos);

    if (insideTitleBar)
    {
        for (QWidget* child = titleBar; child; child = child->childAt(child->mapFromParent(pos)))
        {
            // If the point hits some button in the title bar.
            if (qobject_cast<QAbstractButton*>(child))
                return Qt::NoSection;
        }
    }

    const auto index =
        [](int pos, int width, int frame)
        {
            return (pos < frame) ? 0 : ((pos < width - frame) ? 1 : 2);
        };

    const int column = index(pos.x(), width(), d->interactiveFrameWidth);
    const int row = index(pos.y(), height(), d->interactiveFrameWidth);

    static constexpr std::array<std::array<Qt::WindowFrameSection, 3>, 3> kGrid = {{
        {Qt::TopLeftSection, Qt::TopSection, Qt::TopRightSection},
        {Qt::LeftSection, Qt::NoSection, Qt::RightSection},
        {Qt::BottomLeftSection, Qt::BottomSection, Qt::BottomRightSection}}};

    const auto section = kGrid[row][column];
    return (section == Qt::NoSection && insideTitleBar) ? Qt::TitleBarArea : section;
}

bool EmulatedNonClientArea::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ParentChange:
            d->updateParent(parentWidget());
            break;

        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::ContentsRectChange:
            if (parent())
                d->updateParentContentsMargins();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void EmulatedNonClientArea::paintEvent(QPaintEvent* event)
{
    const auto frameWidth = d->effectiveFrameWidth();
    if (frameWidth == 0)
        return;

    QPainter painter(this);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(
        palette().window(), frameWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

    const qreal halfWidth = 0.5 * frameWidth;
    painter.drawRect(QRectF(rect()).adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));
}

} // namespace nx::vms::client::desktop
