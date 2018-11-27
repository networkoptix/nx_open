#include "search_edit.h"

#include <QtGui/QPainter>
#include <QtGui/QFocusEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAction>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <ui/style/skin.h>
#include <ui/style/helper.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/common/palette.h>

#include <utils/common/scoped_value_rollback.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/utils/app_info.h>
#include <utils/math/color_transformations.h>
#include <utils/common/event_processors.h>

namespace {

static const int kLineEditHeight = 32;
static const int kTagHolderHeight = 40;
static const int kLineHeight = 1;
static const int kControlMaxHeight = kLineEditHeight + kTagHolderHeight + kLineHeight;

QPalette modifiedPalette(QPalette palette, const QColor& backgroundColor = Qt::transparent)
{
    palette.setBrush(QPalette::Base, QBrush(backgroundColor));
    palette.setBrush(QPalette::Shadow, QBrush(backgroundColor));
    palette.setBrush(QPalette::Button, QBrush(backgroundColor));
    return palette;
}

void addLineTo(QLayout* layout)
{
    const auto line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(kLineHeight);
    layout->addWidget(line);
}

class ColoredLineEdit: public QLineEdit
{
    using base_type = QLineEdit;

public:
    ColoredLineEdit(QWidget* parent);

    void setPlaceholderText(const QString& text);

    QString placeholderText() const;

    void setIndentOn(bool on);

private:
    QLabel* const m_label;
};

ColoredLineEdit::ColoredLineEdit(QWidget* parent):
    base_type(parent),
    m_label(new QLabel())
{
    const auto layout = new QHBoxLayout(this);
    layout->addWidget(m_label);
    setPaletteColor(m_label, QPalette::Text, QPalette().color(QPalette::WindowText));

    const auto updatePlaceholderVisibility =
        [this](const QString& text) { m_label->setVisible(text.isEmpty()); };
    connect(this, &QLineEdit::textChanged, this, updatePlaceholderVisibility);
    updatePlaceholderVisibility(text());

    setIndentOn(false);
}

void ColoredLineEdit::setIndentOn(bool on)
{
    m_label->setIndent(on ? style::Metrics::kStandardPadding / 2 : 0);
    auto margins = textMargins();
    margins.setLeft(-style::Metrics::kStandardPadding / (on ? 2 : 1));
    setTextMargins(margins);
}

void ColoredLineEdit::setPlaceholderText(const QString& text)
{
    m_label->setText(text);
}

QString ColoredLineEdit::placeholderText() const
{
    return m_label->text();
}

//-------------------------------------------------------------------------------------------------

class HoverablePushButton: public QPushButton
{
    using base_type = QPushButton;

public:
    using Callback = std::function<void (bool hovered)>;

    HoverablePushButton(QWidget* widget, const Callback& hoverCallback);

    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    const Callback m_hoverCallback;
};

HoverablePushButton::HoverablePushButton(QWidget* widget, const Callback& hoverCallback)
    :
    base_type(widget),
    m_hoverCallback(hoverCallback)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_NoMousePropagation);
}

void HoverablePushButton::enterEvent(QEvent* /*event*/)
{
    if (m_hoverCallback)
        m_hoverCallback(true);
}

void HoverablePushButton::leaveEvent(QEvent* /*event*/)
{
    if (m_hoverCallback)
        m_hoverCallback(false);
}

} //namespace

#include <utils/common/delayed.h>
namespace nx::vms::client::desktop {

struct SearchEdit::Private
{
    QWidget* const lineEditHolder;
    ColoredLineEdit* const lineEdit = nullptr;
    HoverablePushButton* const menuButton = nullptr;
    SelectableTextButton* const tagButton = nullptr;

    bool focused = false;
    bool hovered = false;

    bool menuEnabled = false;
    std::optional<ResourceTreeNodeType> currentFilter;
    std::function<QMenu*()> filterMenuCreator;
    std::function<QString(ResourceTreeNodeType)> filterNameProvider;
};

SearchEdit::SearchEdit(QWidget* parent):
    base_type(parent),
    d(new Private({new QWidget(this), new ColoredLineEdit(this),
        new HoverablePushButton(this, [this](bool hovered){ setButtonHovered(hovered); }),
        new SelectableTextButton(this)}))
{
    setFocusPolicy(d->lineEdit->focusPolicy());
    setSizePolicy(d->lineEdit->sizePolicy());
    setBackgroundRole(d->lineEdit->backgroundRole());

    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_InputMethodEnabled);

    d->lineEdit->setFrame(false);
    d->lineEdit->setFixedHeight(kLineEditHeight);
    d->lineEdit->setFocusProxy(this);
    d->lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    d->lineEdit->setClearButtonEnabled(true);

    connect(d->lineEdit, &QLineEdit::returnPressed, this, &SearchEdit::editingFinished);
    connect(d->lineEdit, &QLineEdit::editingFinished, this, &SearchEdit::editingFinished);
    connect(d->lineEdit, &QLineEdit::textChanged, this, &SearchEdit::textChanged);

    const auto marginsLayout = new QVBoxLayout();
    marginsLayout->setContentsMargins(style::Metrics::kStandardPadding, 0,
        style::Metrics::kStandardPadding, 0);
    marginsLayout->addWidget(d->tagButton);
    marginsLayout->setAlignment(d->tagButton, Qt::AlignLeft | Qt::AlignVCenter);

    const auto holderLayout = new QVBoxLayout(this);
    holderLayout->setSpacing(0);
    holderLayout->addLayout(marginsLayout);
    addLineTo(holderLayout);

    const auto tagHolderWidget = new QWidget(this);
    tagHolderWidget->setFixedHeight(kTagHolderHeight);
    tagHolderWidget->setLayout(holderLayout);

    d->tagButton->setFixedHeight(24);
    d->tagButton->setDeactivatable(true);
    d->tagButton->setSelectable(false);
    d->tagButton->setFocusPolicy(Qt::NoFocus);
    d->tagButton->setState(SelectableTextButton::State::unselected);
    connect(d->tagButton, &SelectableTextButton::stateChanged,
        this, &SearchEdit::handleTagButtonStateChanged);

    const auto searchLineLayout = new QHBoxLayout(this);
    searchLineLayout->setSpacing(0);
    searchLineLayout->addWidget(d->menuButton);
    searchLineLayout->addWidget(d->lineEdit);

    d->lineEditHolder->setLayout(searchLineLayout);
    d->lineEditHolder->setAutoFillBackground(true);

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->addWidget(d->lineEditHolder);
    addLineTo(layout);
    layout->addWidget(tagHolderWidget);
    setLayout(layout);

    setupMenuButton();
    updateTagButton();
    updatePalette();

    installEventHandler(this, {QEvent::KeyPress, QEvent::ShortcutOverride}, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            const auto keyEvent = static_cast<QKeyEvent*>(event);
            switch (keyEvent->key())
            {
                case Qt::Key_Enter:
                case Qt::Key_Return:
                {
                    if (event->type() == QEvent::ShortcutOverride)
                    {
                        if (keyEvent->modifiers().testFlag(Qt::ControlModifier))
                            emit ctrlEnterPressed();
                    }
                    else if (!keyEvent->modifiers())
                    {
                        emit enterPressed();
                    }
                    break;
                }
                default:
                    break;
            }
        });

}

SearchEdit::~SearchEdit()
{
}

bool SearchEdit::focused() const
{
    return d->focused;
}

void SearchEdit::setFocused(bool value)
{
    if (d->focused == value)
        return;

    d->focused = value;
    emit focusedChanged();
}

void SearchEdit::updateFocused()
{
    auto menuHasFocus = [this]() -> bool
        {
            for (auto menu : findChildren<QMenu*>())
                if (menu->hasFocus())
                    return true;
            return false;
        };

    setFocused(hasFocus()
        || d->menuButton->hasFocus()
        || menuHasFocus());
}

void SearchEdit::setupMenuButton()
{
    d->menuButton->setFlat(true);
    d->menuButton->setFocusPolicy(Qt::NoFocus);
    d->menuButton->setAutoFillBackground(true);
    updateMenuButtonIcon();

    connect(d->menuButton, &QPushButton::clicked, this,
        [this]()
        {
            if (menuEnabled())
                showFilterMenu();
            else
                d->lineEdit->setFocus();
        });

    connect(d->lineEdit, &QLineEdit::textChanged, this, &SearchEdit::updateMenuButtonIcon);
}

void SearchEdit::updateMenuButtonIcon()
{
    const auto kIcon = menuEnabled()
        ? qnSkin->icon("tree/search_drop.png")
        : qnSkin->icon("tree/search.png");
    const auto kSelectedIcon = menuEnabled()
        ? qnSkin->icon("tree/search_drop_selected.png")
        : qnSkin->icon("tree/search_selected.png");

    d->menuButton->setFixedSize(menuEnabled() ? QSize(40, 32) : QSize(32, 32));
    d->menuButton->setIcon(d->lineEdit->text().isEmpty() ? kIcon : kSelectedIcon);
}

QString SearchEdit::text() const
{
    return d->lineEdit->text();
}

void SearchEdit::setText(const QString& value)
{
    if (value == text())
        return;

    d->lineEdit->setText(value);
    emit textChanged();
}

void SearchEdit::clear()
{
    setText(QString());
}

QString SearchEdit::placeholderText() const
{
    return d->lineEdit->placeholderText();
}

void SearchEdit::setPlaceholderText(const QString& value)
{
    d->lineEdit->setPlaceholderText(value);
}

void SearchEdit::showFilterMenu() 
{
    if (!menuEnabled() || !d->filterMenuCreator)
        return;

    auto menu = d->filterMenuCreator();
    menu->setParent(this, menu->windowFlags());

    connect(menu, &QMenu::aboutToHide, this, [this]() { d->lineEdit->setFocus(); });
    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);

    for (auto action: menu->actions())
        connect(action, &QAction::triggered, this,
            [this, action]()
            {
                setCurrentFilter(action->data().value<ResourceTreeNodeType>());
            });

    const auto buttonGeometry = d->menuButton->geometry();
    const auto bottomPoint = QPoint(0, buttonGeometry.height());
    const auto globalPoint = d->menuButton->mapToGlobal(bottomPoint);
    QnHiDpiWorkarounds::showMenu(menu, globalPoint);
}

void SearchEdit::setFilterOptionsSource(std::function<QMenu*()> filterMenuCreator,
    std::function<QString(ResourceTreeNodeType)> filterNameProvider)
{
    d->filterMenuCreator = filterMenuCreator;
    d->filterNameProvider = filterNameProvider;
}

void SearchEdit::handleTagButtonStateChanged()
{
    if (d->tagButton->state() == SelectableTextButton::State::deactivated)
        setCurrentFilter(std::optional<ResourceTreeNodeType>());
}

void SearchEdit::updatePalette()
{
    const auto backgroundColor =
        [this]() -> QColor
        {
            if (hasFocus())
                return colorTheme()->color("dark2");

            return d->hovered
                ? toTransparent(colorTheme()->color("dark9"), 0.2)
                : QColor(Qt::transparent);
        }();

    const auto controlPalette = modifiedPalette(d->lineEdit->palette(), backgroundColor);
    d->lineEditHolder->setPalette(controlPalette);
    d->lineEdit->setPalette(controlPalette);
    d->menuButton->setPalette(controlPalette);
}

void SearchEdit::setCurrentFilter(std::optional<ResourceTreeNodeType> allowedNodeType)
{
    if (allowedNodeType == d->currentFilter)
        return;

    d->currentFilter = allowedNodeType;
    updateTagButton();
    emit currentFilterChanged();
}

void SearchEdit::updateTagButton()
{
    if (d->currentFilter)
    {
        if (d->filterNameProvider)
            d->tagButton->setText(d->filterNameProvider(d->currentFilter.value()));
        d->tagButton->setState(SelectableTextButton::State::unselected);
    }

    const bool tagVisible = d->currentFilter.has_value();
    setFixedHeight(tagVisible
        ? kLineEditHeight + kTagHolderHeight + kLineHeight
        : kLineEditHeight);
    d->tagButton->parentWidget()->setVisible(tagVisible);
}

QSize SearchEdit::sizeHint() const
{
    const QnScopedTypedPropertyRollback<bool, QLineEdit>
        frameRollback(d->lineEdit, &QLineEdit::setFrame, &QLineEdit::hasFrame, true);

    const auto tagVerticalSize = d->tagButton->isVisible()
        ? QSize(0, d->tagButton->parentWidget()->sizeHint().height())
        : QSize();

    const auto menuButtonHorizontalSize = QSize(d->menuButton->sizeHint().width(), 0);
    const auto result = d->lineEdit->sizeHint() + menuButtonHorizontalSize + tagVerticalSize;
    return result;
}

QVariant SearchEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return d->lineEdit->inputMethodQuery(property);
}

void SearchEdit::resizeEvent(QResizeEvent *event)
{
    base_type::resizeEvent(event);
    d->lineEdit->setGeometry(rect());
}

void SearchEdit::focusInEvent(QFocusEvent* event)
{
    d->lineEdit->event(event);
    d->lineEdit->selectAll();

    updatePalette();
    updateFocused();

    base_type::focusInEvent(event);
}

void SearchEdit::focusOutEvent(QFocusEvent* event)
{
    d->lineEdit->event(event);

    if (const auto completer = d->lineEdit->completer())
    {
        connect(completer, QnCompleterActivated, d->lineEdit, &QLineEdit::setText);
        connect(completer, SIGNAL(highlighted(QString)),
            d->lineEdit, SLOT(_q_completionHighlighted));
    }

    updatePalette();
    updateFocused();

    base_type::focusOutEvent(event);
}

void SearchEdit::keyPressEvent(QKeyEvent* event)
{
    d->lineEdit->event(event);
}

void SearchEdit::inputMethodEvent(QInputMethodEvent* event)
{
    d->lineEdit->event(event);
}

void SearchEdit::enterEvent(QEvent* /*event*/)
{
    setHovered(true);
}

void SearchEdit::leaveEvent(QEvent* /*event*/)
{
    setHovered(false);
}

void SearchEdit::setHovered(bool value)
{
    if (d->hovered == value)
        return;

    d->hovered = value;
    updatePalette();
}

void SearchEdit::setButtonHovered(bool value)
{
    if (value)
    {
        const auto color = toTransparent(colorTheme()->color("dark9"), 0.2);
        d->menuButton->setPalette(modifiedPalette(d->menuButton->palette(), color));
    }
    else
    {
        updatePalette();
    }
}

bool SearchEdit::event(QEvent* event)
{
    if (event->type() != QEvent::ShortcutOverride)
        return QWidget::event(event);

    const auto keyEvent = static_cast<QKeyEvent*>(event);
    const bool result = d->lineEdit->event(event);
    if (keyEvent->key() == Qt::Key_Escape)
        keyEvent->accept();

    return result;
}

std::optional<ResourceTreeNodeType> SearchEdit::currentFilter() const
{
    return d->currentFilter;
}

bool SearchEdit::menuEnabled() const 
{
    return d->menuEnabled;
}

void SearchEdit::setMenuEnabled(bool enabled) 
{
    if (d->menuEnabled == enabled)
        return;

    d->menuEnabled = enabled;

    d->lineEdit->setIndentOn(!d->menuEnabled);
    updateMenuButtonIcon();
}

} // namespace nx::vms::client::desktop
