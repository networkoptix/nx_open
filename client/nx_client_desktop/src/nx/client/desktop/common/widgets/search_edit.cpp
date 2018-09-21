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
#include <nx/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/client/desktop/ui/common/color_theme.h>
#include <nx/utils/app_info.h>
#include <utils/math/color_transformations.h>

namespace {

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
    line->setFixedHeight(1);
    layout->addWidget(line);
}

class ColoredLineEdit: public QLineEdit
{
    using base_type = QLineEdit;

public:
    ColoredLineEdit(QWidget* parent, const QColor& placeholderTextColor);

    void setPlaceholderText(const QString& text);

    QString placeholderText() const;

private:
    QLabel* const m_label;
};

ColoredLineEdit::ColoredLineEdit(QWidget* parent, const QColor& placeholderTextColor):
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

    m_label->setIndent(style::Metrics::kStandardPadding / 2);
    auto margins = textMargins();
    margins.setLeft(-style::Metrics::kStandardPadding / 2);
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
namespace nx {
namespace client {
namespace desktop {

struct SearchEdit::Private
{
    QWidget* const lineEditHolder;
    ColoredLineEdit* const lineEdit = nullptr;
    HoverablePushButton* const menuButton = nullptr;
    QMenu* const menu = nullptr;
    SelectableTextButton* const tagButton = nullptr;

    bool hovered = false;
    int selectedTagIndex = -1;
    int clearingTagIndex = -1;

};

SearchEdit::SearchEdit(QWidget* parent):
    base_type(parent),
    d(new Private({new QWidget(this), new ColoredLineEdit(this, Qt::red),
        new HoverablePushButton(this, [this](bool hovered){ setButtonHovered(hovered); }),
        new QMenu(this), new SelectableTextButton(this)}))
{
    setFocusPolicy(d->lineEdit->focusPolicy());
    setSizePolicy(d->lineEdit->sizePolicy());
    setBackgroundRole(d->lineEdit->backgroundRole());

    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_InputMethodEnabled);

    d->lineEdit->setFrame(false);
    d->lineEdit->setFocusProxy(this);
    d->lineEdit->setFixedHeight(32);
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
    tagHolderWidget->setFixedHeight(40);
    tagHolderWidget->setLayout(holderLayout);

    d->tagButton->setFixedHeight(24);
    d->tagButton->setDeactivatable(true);
    d->tagButton->setSelectable(false);
    d->tagButton->setState(SelectableTextButton::State::unselected);
    connect(d->tagButton, &SelectableTextButton::stateChanged,
        this, &SearchEdit::handleTagButtonStateChanged);
    connect(this, &SearchEdit::selectedTagIndexChanged, this, &SearchEdit::updateTagButton);

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

    setupMenuButton();;
    updateTagButton();
    updatePalette();
}

SearchEdit::~SearchEdit()
{
}

void SearchEdit::setupMenuButton()
{
    d->menuButton->setFlat(true);
    d->menuButton->setFixedSize(40, 32);
    d->menuButton->setFocusPolicy(Qt::NoFocus);
    d->menuButton->setIcon(qnSkin->icon("theme/search_drop.png"));
    d->menuButton->setAutoFillBackground(true);

    connect(d->menuButton, &QPushButton::clicked, this,
        [this]()
        {
            const auto buttonGeometry = d->menuButton->geometry();
            const auto bottomPoint = QPoint(0, buttonGeometry.height());
            const auto globalPoint = d->menuButton->mapToGlobal(bottomPoint);
            QnHiDpiWorkarounds::showMenu(d->menu, globalPoint);
        });

    connect(d->lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            d->menuButton->setIcon(d->lineEdit->text().isEmpty()
                ? qnSkin->icon("theme/search_drop.png")
                : qnSkin->icon("theme/search_drop_selected.png"));
        });
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

QStringList SearchEdit::tagsList() const
{
    return m_tags;
}

void SearchEdit::setTags(const QStringList& value)
{
    if (m_tags == value)
        return;

    m_tags = value;

    setSelectedTagIndex(-1);

    d->menu->clear();
    for (int index = 0; index != m_tags.size(); ++ index)
    {
        const auto tag = m_tags.at(index);
        const auto action = tag.isEmpty()
            ? d->menu->addSeparator()
            : d->menu->addAction(tag);

        connect(action, &QAction::triggered, this,
            [this, index]() { setSelectedTagIndex(index); });
    }
}

void SearchEdit::handleTagButtonStateChanged()
{
    if (d->tagButton->state() == SelectableTextButton::State::deactivated)
        setSelectedTagIndex(-1);
}

int SearchEdit::selectedTagIndex() const
{
    return d->selectedTagIndex;
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

void SearchEdit::setSelectedTagIndex(int value)
{
    if (value == d->clearingTagIndex)
        value = -1; // Clears selected index.

    if (value == d->selectedTagIndex)
        return;

    d->selectedTagIndex = value;
    emit selectedTagIndexChanged();
}

void SearchEdit::updateTagButton()
{
    if (d->selectedTagIndex != -1)
    {
        d->tagButton->setText(m_tags.at(d->selectedTagIndex));
        d->tagButton->setState(SelectableTextButton::State::unselected);
    }

    d->tagButton->parentWidget()->setVisible(d->selectedTagIndex != -1);
}

void SearchEdit::setClearingTagIndex(int index)
{
    d->clearingTagIndex = index;
}

int SearchEdit::clearingTagIndex() const
{
    return d->clearingTagIndex;
}

QSize SearchEdit::sizeHint() const
{
    const QnScopedTypedPropertyRollback<bool, QLineEdit>
        frameRollback(d->lineEdit, &QLineEdit::setFrame, &QLineEdit::hasFrame, true);

    const auto tagVerticalSize = d->selectedTagIndex == -1
        ? QSize()
        : QSize(0, d->tagButton->parentWidget()->sizeHint().height());

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

    base_type::focusInEvent(event);

    updatePalette();
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

    base_type::focusOutEvent(event);

    updatePalette();
}

void SearchEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        const auto ctrlModifier = nx::utils::AppInfo::isMacOsX()
            ? Qt::MetaModifier
            : Qt::ControlModifier;
        if (event->modifiers().testFlag(ctrlModifier))
            emit ctrlEnterPressed();
        else
            emit enterPressed();

        event->accept();
    }
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

} // namespace desktop
} // namespace client
} // namespace nx
