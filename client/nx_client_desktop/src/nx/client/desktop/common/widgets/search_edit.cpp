#include "search_edit.h"

#include <QtGui/QPainter>
#include <QtGui/QFocusEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/common/palette.h>

#include <utils/common/scoped_value_rollback.h>
#include <nx/client/desktop/ui/common/selectable_text_button.h>
#include <nx/client/desktop/ui/common/color_theme.h>

namespace {

QPalette modifiedPalette(QPalette palette, const QColor& backgroundColor = Qt::transparent)
{
    palette.setBrush(QPalette::Base, QBrush(backgroundColor));
    palette.setBrush(QPalette::Shadow, QBrush(backgroundColor));
    return palette;
}

void addLineTo(QLayout* layout)
{
    const auto line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    layout->addWidget(line);
}

} //namespace

namespace nx {
namespace client {
namespace desktop {

SearchEdit::SearchEdit(QWidget* parent):
    base_type(parent),
    m_lineEditHolder(new QWidget(this)),
    m_lineEdit(new QLineEdit(this)),
    m_menuButton(new QPushButton(this)),
    m_menu(new QMenu(this)),
    m_tagButton(new ui::SelectableTextButton(this))
{
    setFocusPolicy(m_lineEdit->focusPolicy());
    setSizePolicy(m_lineEdit->sizePolicy());
    setBackgroundRole(m_lineEdit->backgroundRole());

    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_InputMethodEnabled);

    m_lineEdit->setFrame(false);
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setFixedHeight(32);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_lineEdit->setClearButtonEnabled(true);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &SearchEdit::editingFinished);
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &SearchEdit::editingFinished);
    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchEdit::textChanged);

    const auto marginsLayout = new QVBoxLayout();
    marginsLayout->setContentsMargins(8, 0, 8, 0);
    marginsLayout->addWidget(m_tagButton);
    marginsLayout->setAlignment(m_tagButton, Qt::AlignLeft | Qt::AlignVCenter);

    const auto holderLayout = new QVBoxLayout(this);
    holderLayout->setSpacing(0);
    holderLayout->addLayout(marginsLayout);
    addLineTo(holderLayout);

    const auto tagHolderWidget = new QWidget(this);
    tagHolderWidget->setFixedHeight(40);
    tagHolderWidget->setLayout(holderLayout);

    m_tagButton->setFixedHeight(24);
    m_tagButton->setDeactivatable(true);
    m_tagButton->setSelectable(false);
    m_tagButton->setState(ui::SelectableTextButton::State::unselected);
    connect(m_tagButton, &ui::SelectableTextButton::stateChanged,
        this, &SearchEdit::handleTagButtonStateChanged);
    connect(this, &SearchEdit::selectedTagIndexChanged, this, &SearchEdit::updateTagButton);


    const auto searchLineLayout = new QHBoxLayout(this);
    searchLineLayout->setSpacing(0);
    searchLineLayout->addWidget(m_menuButton);
    searchLineLayout->addWidget(m_lineEdit);

    m_lineEditHolder->setLayout(searchLineLayout);
    m_lineEditHolder->setAutoFillBackground(true);

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->addWidget(m_lineEditHolder);
    addLineTo(layout);
    layout->addWidget(tagHolderWidget);
    setLayout(layout);

    setupMenuButton();
    updateTagButton();
    updatePalette();
}

SearchEdit::~SearchEdit()
{
}

void SearchEdit::setupMenuButton()
{
    m_menuButton->setFlat(true);
    m_menuButton->setFixedSize(40, 32);
    m_menuButton->setFocusPolicy(Qt::NoFocus);
    m_menuButton->setIcon(qnSkin->icon("theme/search_drop.png"));

    connect(m_menuButton, &QPushButton::clicked, this,
        [this]()
        {
            const auto buttonGeometry = m_menuButton->geometry();
            const auto bottomPoint = QPoint(0, buttonGeometry.height());
            const auto globalPoint = m_menuButton->mapToGlobal(bottomPoint);
            QnHiDpiWorkarounds::showMenu(m_menu, globalPoint);
        });

    connect(m_lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            m_menuButton->setIcon(m_lineEdit->text().isEmpty()
                ? qnSkin->icon("theme/search_drop.png")
                : qnSkin->icon("theme/search_drop_selected.png"));
        });
}

QString SearchEdit::text() const
{
    return m_lineEdit->text();
}

void SearchEdit::setText(const QString& value)
{
    if (value == text())
        return;

    m_lineEdit->setText(value);
    emit textChanged();
}

void SearchEdit::clear()
{
    setText(QString());
}

QString SearchEdit::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

void SearchEdit::setPlaceholderText(const QString& value)
{
    m_lineEdit->setPlaceholderText(value);
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

    m_menu->clear();
    for (int index = 0; index != m_tags.size(); ++ index)
    {
        const auto tag = m_tags.at(index);
        const auto action = tag.isEmpty()
            ? m_menu->addSeparator()
            : m_menu->addAction(tag);

        connect(action, &QAction::triggered, this,
            [this, index]() { setSelectedTagIndex(index); });
    }
}

void SearchEdit::handleTagButtonStateChanged()
{
    if (m_tagButton->state() == ui::SelectableTextButton::State::deactivated)
        setSelectedTagIndex(-1);
}

int SearchEdit::selectedTagIndex() const
{
    return m_selectedTagIndex;
}

void SearchEdit::updatePalette()
{
    const auto backgroundColor = hasFocus()
        ? ColorTheme::instance()->color(lit("dark2"))
        : QColor(Qt::transparent);

    const auto controlPalette = modifiedPalette(m_lineEdit->palette(), backgroundColor);
    m_lineEditHolder->setPalette(controlPalette);
    m_lineEdit->setPalette(controlPalette);
}

void SearchEdit::setSelectedTagIndex(int value)
{
    if (value == m_clearingTagIndex)
        value = -1; // Clears selected index.

    if (value == m_selectedTagIndex)
        return;

    m_selectedTagIndex = value;
    emit selectedTagIndexChanged();
}

void SearchEdit::updateTagButton()
{
    if (m_selectedTagIndex != -1)
    {
        m_tagButton->setText(m_tags.at(m_selectedTagIndex));
        m_tagButton->setState(ui::SelectableTextButton::State::unselected);
    }

    m_tagButton->parentWidget()->setVisible(m_selectedTagIndex != -1);
}

void SearchEdit::setClearingTagIndex(int index)
{
    m_clearingTagIndex = index;
}

int SearchEdit::clearingTagIndex() const
{
    return m_clearingTagIndex;
}

QSize SearchEdit::sizeHint() const
{
    const QnScopedTypedPropertyRollback<bool, QLineEdit>
        frameRollback(m_lineEdit, &QLineEdit::setFrame, &QLineEdit::hasFrame, true);

    const auto tagVerticalSize = m_selectedTagIndex == -1
        ? QSize()
        : QSize(0, m_tagButton->parentWidget()->sizeHint().height());

    const auto menuButtonHorizontalSize = QSize(m_menuButton->sizeHint().width(), 0);
    const auto result = m_lineEdit->sizeHint() + menuButtonHorizontalSize + tagVerticalSize;
    return result;
}

QVariant SearchEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return m_lineEdit->inputMethodQuery(property);
}

void SearchEdit::resizeEvent(QResizeEvent *event)
{
    base_type::resizeEvent(event);
    m_lineEdit->setGeometry(rect());
}

void SearchEdit::focusInEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);
    m_lineEdit->selectAll();

    base_type::focusInEvent(event);

    updatePalette();
}

void SearchEdit::focusOutEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);

    if (const auto completer = m_lineEdit->completer())
    {
        connect(completer, QCompleterActivated, m_lineEdit, &QLineEdit::setText);
        connect(completer, SIGNAL(highlighted(QString)),
            m_lineEdit, SLOT(_q_completionHighlighted));
    }

    base_type::focusOutEvent(event);

    updatePalette();
}

void SearchEdit::keyPressEvent(QKeyEvent* event)
{
    m_lineEdit->event(event);
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        if (event->modifiers().testFlag(Qt::ControlModifier))
            emit ctrlEnterPressed();
        else
            emit enterPressed();

        event->accept();
    }
}

void SearchEdit::inputMethodEvent(QInputMethodEvent* event)
{
    m_lineEdit->event(event);
}

bool SearchEdit::event(QEvent* event)
{
    if (event->type() != QEvent::ShortcutOverride)
        return QWidget::event(event);

    const auto keyEvent = static_cast<QKeyEvent*>(event);
    const bool result = m_lineEdit->event(event);
    if (keyEvent->key() == Qt::Key_Escape)
        keyEvent->accept();

    return result;
}

} // namespace desktop
} // namespace client
} // namespace nx
