#include "text_input.h"

#include <QtQuick/private/qquicktextinput_p_p.h>
#include <QtQuick/private/qquickclipnode_p.h>
#include <QtLabsTemplates/private/qquickmenu_p.h>
#include <QtLabsTemplates/private/qquickmenuitem_p.h>

namespace {

template<typename Type>
Type* createComponent(QObject* textInput, const QString& description, const QString& text)
{
    QQmlComponent component(qmlEngine(textInput));
    component.setData(text.toLatin1(), QUrl(description));
    return qobject_cast<Type*>(component.create());
};

void addMenuItem(
    QObject* textInput,
    QQuickMenu* menu,
    const QString& text,
    bool enabled,
    const std::function<void()> callback)
{
    const auto menuItem = createComponent<QQuickMenuItem>(
        textInput, lit("menu_item"), lit("import Nx.Controls 1.0; MenuItem {}"));
    if (!menuItem)
        return;

    QObject::connect(menuItem, &QQuickMenuItem::triggered, textInput,
        [menu, callback]()
    {
        callback();
        menu->close();
    });

    menuItem->setText(text);
    menuItem->setEnabled(enabled);
    menu->addItem(menuItem);
    menu->setHeight(menuItem->height() * menu->children().count());
};

} // namespace

class QnQuickTextInputPrivate : public QQuickTextInputPrivate
{
    Q_DECLARE_PUBLIC(QnQuickTextInput)

public:
    QnQuickTextInputPrivate();

    enum Handle
    {
        CursorHandle,
        SelectionStartHandle,
        SelectionEndHandle
    };

    bool scrollByMouse;
    qreal hscrollWhenPressed;
    QQuickItem* background;
    QString placeholder;
    bool dragStarted;

    void resizeBackground();
    void updateInputMethod();
};

QnQuickTextInputPrivate::QnQuickTextInputPrivate() :
    scrollByMouse(true),
    hscrollWhenPressed(0),
    background(nullptr),
    dragStarted(false)
{
}

void QnQuickTextInputPrivate::resizeBackground()
{
    Q_Q(QnQuickTextInput);

    if (!background)
        return;

    QQuickItemPrivate* p = QQuickItemPrivate::get(background);
    if (!p->widthValid && qFuzzyIsNull(background->x()))
    {
        background->setWidth(q->width());
        p->widthValid = false;
    }
    if (!p->heightValid && qFuzzyIsNull(background->y()))
    {
        background->setHeight(q->height());
        p->heightValid = false;
    }
}

void QnQuickTextInputPrivate::updateInputMethod()
{
    Q_Q(const QnQuickTextInput);

    if (!q->hasActiveFocus())
        return;

    auto inputMethod = qApp->inputMethod();

    bool enabled = q->isEnabled() && q->isVisible();
    if (enabled)
        inputMethod->show();
    else
        inputMethod->hide();
}

QnQuickTextInput::QnQuickTextInput(QQuickItem* parent) :
    base_type(*(new QnQuickTextInputPrivate), parent)
{
    Q_D(QnQuickTextInput);

    auto updateInputMethod = [d](){ d->updateInputMethod(); };
    setAcceptedMouseButtons(Qt::AllButtons);
    connect(this, &QnQuickTextInput::visibleChanged, updateInputMethod);
    connect(this, &QnQuickTextInput::enabledChanged, updateInputMethod);
}

QnQuickTextInput::~QnQuickTextInput()
{
}

bool QnQuickTextInput::scrollByMouse() const
{
    Q_D(const QnQuickTextInput);
    return d->scrollByMouse;
}

void QnQuickTextInput::setScrollByMouse(bool enable)
{
    Q_D(QnQuickTextInput);
    if (d->scrollByMouse == enable)
        return;

    setSelectByMouse(false);
    d->scrollByMouse = enable;
    emit scrollByMouseChanged();
}

QQuickItem* QnQuickTextInput::background() const
{
    Q_D(const QnQuickTextInput);
    return d->background;
}

void QnQuickTextInput::setBackground(QQuickItem* background)
{
    /* Copy paste from QQuickTextField (Qt.labs.controls) */
    Q_D(QnQuickTextInput);
    if (d->background == background)
        return;

    delete d->background;

    d->background = background;

    if (background)
    {
        background->setParentItem(this);
        if (qFuzzyIsNull(background->z()))
            background->setZ(-1);
        if (isComponentComplete())
            d->resizeBackground();
    }

    emit backgroundChanged();
}

QString QnQuickTextInput::placeholderText() const
{
    Q_D(const QnQuickTextInput);
    return d->placeholder;
}

void QnQuickTextInput::setPlaceholderText(const QString& text)
{
    Q_D(QnQuickTextInput);
    if (d->placeholder == text)
        return;

    d->placeholder = text;
    emit placeholderTextChanged();
}

QSGNode* QnQuickTextInput::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* data)
{
    /* Copy paste from QQuickTextField (Qt.labs.controls) */
    QQuickDefaultClipNode* clipNode = static_cast<QQuickDefaultClipNode*>(oldNode);
    if (!clipNode)
        clipNode = new QQuickDefaultClipNode(QRectF());

    clipNode->setRect(clipRect().adjusted(leftPadding(), topPadding(), -rightPadding(), -bottomPadding()));
    clipNode->update();

    QSGNode* textNode = QQuickTextInput::updatePaintNode(clipNode->firstChild(), data);
    if (!textNode->parent())
        clipNode->appendChildNode(textNode);

    return clipNode;
}

#include <QtWidgets/QMenu>

void QnQuickTextInput::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QnQuickTextInput);
    base_type::geometryChanged(newGeometry, oldGeometry);
    d->resizeBackground();
}

void QnQuickTextInput::mousePressEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);

    if (event->button() == Qt::RightButton)
        showMenu(event->pos());

    base_type::mousePressEvent(event);

    d->hscrollWhenPressed = d->hscroll;
    d->dragStarted = false;
}

void QnQuickTextInput::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);

    if (qAbs(int(event->localPos().x() - d->pressPos.x())) > QGuiApplication::styleHints()->startDragDistance())
        d->dragStarted = true;

    if (!d->scrollByMouse)
    {
        base_type::mouseMoveEvent(event);
        return;
    }

    /* Copy paste from QQuickTextInput */
    QTextLine textLine = d->m_textLayout.lineForTextPosition(d->m_cursor + d->m_preeditCursor);
    const qreal width = qMax<qreal>(0, this->width() - leftPadding() - rightPadding());
    qreal cix = 0;
    qreal hscrollMax = 0;
    if (textLine.isValid())
    {
        const int preeditLength = d->m_textLayout.preeditAreaText().length();
        cix = textLine.cursorToX(d->m_cursor + preeditLength);
        const qreal cursorWidth = cix >= 0 ? cix : width - cix;
        hscrollMax = qMax(textLine.naturalTextWidth(), cursorWidth) - width;
    }

    int dx = event->localPos().x() - d->pressPos.x();
    d->hscroll = qBound(0.0, d->hscrollWhenPressed - dx, hscrollMax);

    d->textLayoutDirty = true;
    d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
    polish();
    update();
    emit cursorRectangleChanged();
    if (d->cursorItem)
    {
        QRectF r = cursorRectangle();
        d->cursorItem->setPosition(r.topLeft());
        d->cursorItem->setHeight(r.height());
    }
#ifndef QT_NO_IM
    updateInputMethod(Qt::ImCursorRectangle);
#endif
}

void QnQuickTextInput::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);
    if (!d->dragStarted)
        emit clicked();

    base_type::mouseReleaseEvent(event);
}

void QnQuickTextInput::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    selectAll();
}

void QnQuickTextInput::showMenu(const QPoint& position)
{
    auto menu = createComponent<QQuickMenu>(this, lit("context_menu"), lit("import Nx.Controls 1.0; Menu {}"));
    if (!menu)
        return;

    const auto currentText = text();
    const auto currentSelectedText = selectedText();

    addMenuItem(this, menu, lit("Cut"), !currentSelectedText.isEmpty(),
        [this, selStart = selectionStart(), selEnd = selectionEnd()]()
        {
            select(selStart, selEnd);
            cut();
        });

    addMenuItem(this, menu, lit("Copy"), !currentSelectedText.isEmpty(),
        [this, selStart = selectionStart(), selEnd = selectionEnd()]()
        {
            select(selStart, selEnd);
            copy();
        });

    addMenuItem(this, menu, lit("Paste"), true,
        [this, selStart = selectionStart(), selEnd = selectionEnd()]()
        {
            remove(selStart, selEnd);
            paste();
        });

    addMenuItem(this, menu, lit("Select All"), currentSelectedText != currentText,
        [this]() { selectAll(); });

    connect(menu, &QQuickMenu::aboutToHide, this,
        [menu]() { menu->deleteLater(); });

    menu->setX(position.x());
    menu->setY(position.y());
    menu->setParentItem(this);
    menu->open();
}
