#include "line_edit_controls.h"

#include <algorithm>
#include <QtCore/QEvent>
#include <QtCore/QMargins>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QApplication>

#include <ui/common/widget_anchor.h>
#include <utils/common/event_processors.h>

#include <nx/utils/disconnect_helper.h>
#include <nx/utils/log/assert.h>

namespace {

static const QMargins kDefaultMargins(6, 1, 6, 2);
static constexpr int kDefaultSpacing = 6;

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// ------------------------------------------------------------------------------------------------
// LineEditControls::Private.

struct LineEditControls::Private
{
    Private(LineEditControls* main):
        main(main),
        anchor(new QnWidgetAnchor(main)),
        contents(new QWidget(main)),
        initialMargins(main->parentWidget()->contentsMargins())
    {
        main->setAttribute(Qt::WA_TranslucentBackground);
        contents->setAttribute(Qt::WA_TranslucentBackground);
        contents->setFont(qApp->font()); //< Otherwise controls inherit line edit font.

        installEventHandler(contents, QEvent::LayoutRequest, contents,
            [this]() { updateLayout(); });
    }

    ~Private()
    {
        disconnectHelper.clear();
    }

    void setEmpty()
    {
        main->resize(QSize());
        main->parentWidget()->setContentsMargins(initialMargins);
    }

    void updateLayout()
    {
        if (!main->isVisible())
            return; //< Don't lay out if invisible.

        if (controls.empty())
        {
            setEmpty();
            return;
        }

        const bool rtl = main->layoutDirection() == Qt::RightToLeft;
        if (!rtl && main->layoutDirection() != Qt::LeftToRight)
        {
            NX_ASSERT(false); //< Should never happen.
            setEmpty();
            return;
        }

        const auto controlSize =
            [this](QWidget* control)
            {
                const auto policy = control->sizePolicy();
                if (control->isHidden() && !policy.retainSizeWhenHidden())
                    return QSize();

                NX_ASSERT(!policy.hasHeightForWidth(), Q_FUNC_INFO,
                    "Controls embedded to line edit should not have height-for-width.");

                bool sizeHintCached = false;
                QSize cachedSizeHint;
                const auto sizeHint =
                    [&sizeHintCached, &cachedSizeHint](QWidget* control)
                    {
                        if (sizeHintCached)
                            return cachedSizeHint;

                        cachedSizeHint = control->sizeHint()
                            .expandedTo(control->minimumSizeHint())
                            .expandedTo(control->minimumSize())
                            .boundedTo(control->maximumSize());

                        sizeHintCached = true;
                        return cachedSizeHint;
                    };

                QSize result(
                    (policy.horizontalPolicy() & QSizePolicy::IgnoreFlag)
                        ? control->width()
                        : sizeHint(control).width(),
                    (policy.verticalPolicy() & QSizePolicy::IgnoreFlag)
                        ? control->height()
                        : sizeHint(control).height());

                if (policy.verticalPolicy() & QSizePolicy::ExpandFlag)
                    result.setHeight(qMax(result.height(), contents->height()));

                return result;
            };

        QVector<QSize> sizes(controls.size());
        std::transform(controls.cbegin(), controls.cend(), sizes.begin(), controlSize);

        int totalWidth = 0;
        int numValid = 0;

        for (const auto& size: sizes)
        {
            if (!size.isValid())
                continue;

            totalWidth += size.width();
            ++numValid;
        }

        if (!numValid) //< No visible controls.
        {
            setEmpty();
            return;
        }

        totalWidth += spacing * (numValid - 1);
        const int maxHeight = main->contentsRect().height();

        int x = 0;
        auto sizeIter = sizes.cbegin();
        for (const auto control: controls)
        {
            const auto size = *sizeIter++;
            if (!size.isValid())
                continue;

            const QPoint position(
                rtl ? (totalWidth - x - size.width()) : x,
                int((maxHeight - size.height()) * 0.5 + 1.0));

            control->setGeometry({position, size});
            x += size.width() + spacing;
        }

        const auto margins = main->contentsMargins();
        main->resize(totalWidth + margins.left() + margins.right(), main->height());
    }

    void updatePosition()
    {
        auto lineEdit = main->parentWidget();
        NX_ASSERT(lineEdit);
        if (!lineEdit)
            return;

        const auto dir = main->layoutDirection();
        const auto rtl = dir == Qt::RightToLeft;
        const Qt::Edges vertical = Qt::TopEdge | Qt::BottomEdge;
        const Qt::Edges horizontal = rtl ? Qt::LeftEdge : Qt::RightEdge;

        anchor->setEdges(horizontal | vertical);

        auto margins = lineEdit->contentsMargins();
        margins.setLeft(initialMargins.left());
        margins.setRight(initialMargins.right());

        if (rtl)
            margins.setLeft(margins.left() + main->width());
        else
            margins.setRight(margins.left() + main->width());

        lineEdit->setContentsMargins(margins);
    }

    bool removeControl(QWidget* control, bool controlDeleted)
    {
        if (!controls.removeOne(control))
            return false;

        if (controlDeleted || !control->isHidden())
            updateLayout();

        return true;
    }

    void addControls(const std::initializer_list<QWidget*>& list)
    {
        const auto oldCount = controls.size();

        for (const auto control: list)
        {
            if (!control || controls.contains(control))
                continue;

            control->setParent(contents.data());
            controls << control;

            // Remove if deleted.
            *disconnectHelper << QObject::connect(control, &QWidget::destroyed,
                [this](QObject* what)
                {
                    removeControl(static_cast<QWidget*>(what), true);
                });

            // Remove if reparented.
            installEventHandler(control, QEvent::ParentChange, contents,
                [this](QObject* what)
                {
                    removeControl(static_cast<QWidget*>(what), false);
                });
        }

        if (oldCount != controls.size())
            updateLayout();
    }

    LineEditControls* const main = nullptr;
    QnWidgetAnchor* const anchor = nullptr;
    int spacing = kDefaultSpacing;

    const QScopedPointer<QWidget> contents;
    const QMargins initialMargins; //< Initial contents margins of line edit.
    QList<QWidget*> controls;
    QnDisconnectHelperPtr disconnectHelper = QnDisconnectHelper::create();
};

// ------------------------------------------------------------------------------------------------
// LineEditControls.

LineEditControls::LineEditControls(QLineEdit* lineEdit):
    base_type(lineEdit),
    d(new Private(this))
{
    setContentsMargins(kDefaultMargins);
}

LineEditControls* LineEditControls::get(QLineEdit* lineEdit)
{
    if (!lineEdit)
        return nullptr;

    if (auto result = lineEdit->findChild<LineEditControls*>(QString(), Qt::FindDirectChildrenOnly))
        return result;

    return new LineEditControls(lineEdit);
}

LineEditControls::~LineEditControls()
{
}

bool LineEditControls::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Show:
        case QEvent::ContentsRectChange:
        case QEvent::LayoutDirectionChange:
            d->updateLayout();
            break;

        case QEvent::Resize:
            d->contents->setGeometry(contentsRect());
            d->updatePosition();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void LineEditControls::addControl(QWidget* control)
{
    d->addControls({control});
}

void LineEditControls::addControls(const std::initializer_list<QWidget*>& controls)
{
    d->addControls(controls);
}

int LineEditControls::spacing() const
{
    return d->spacing;
}

void LineEditControls::setSpacing(int value)
{
    if (d->spacing == value)
        return;

    d->spacing = value;
    d->updateLayout();
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
