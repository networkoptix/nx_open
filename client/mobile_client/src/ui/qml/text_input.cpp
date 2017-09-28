#include "text_input.h"

#include <QtCore/QTimer>

#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQuick/private/qquicktextinput_p_p.h>
#include <QtQuick/private/qquickclipnode_p.h>

#include <utils/common/app_info.h>

class QnQuickTextInputPrivate: public QQuickTextInputPrivate
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

    bool scrollByMouse = true;
    qreal hscrollWhenPressed = 0.0;
    QQuickItem* background = nullptr;
    QString placeholder;
    bool dragStarted = false;
    QnQuickTextInput::EnterKeyType enterKeyType = QnQuickTextInput::EnterKeyDefault;

    QTimer* pressAndHoldTimer = nullptr;
    QPoint lastPos;
    Qt::MouseButton lastButton;
    Qt::MouseButtons lastButtons;
    Qt::KeyboardModifiers lastModifiers;

    void resizeBackground();
    void updateInputMethod();
};

QnQuickTextInputPrivate::QnQuickTextInputPrivate()
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

    d->pressAndHoldTimer = new QTimer(this);
    d->pressAndHoldTimer->setInterval(QGuiApplication::styleHints()->mousePressAndHoldInterval());
    d->pressAndHoldTimer->setSingleShot(true);
    connect(d->pressAndHoldTimer, &QTimer::timeout, this,
        [this, d]()
        {
            QQuickMouseEvent event(
                d->lastPos.x(), d->lastPos.y(),
                d->lastButton, d->lastButtons, d->lastModifiers, false, true);
            emit pressAndHold(&event);
        });
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

QnQuickTextInput::EnterKeyType QnQuickTextInput::enterKeyType() const
{
    Q_D(const QnQuickTextInput);
    return d->enterKeyType;
}

void QnQuickTextInput::setEnterKeyType(EnterKeyType enterKeyType)
{
    Q_D(QnQuickTextInput);

    if (d->enterKeyType == enterKeyType)
        return;

    d->enterKeyType = enterKeyType;
    emit enterKeyTypeChanged();

    #ifndef QT_NO_IM
        updateInputMethod(Qt::ImEnterKeyType);
    #endif
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

#ifndef QT_NO_IM

QVariant QnQuickTextInput::inputMethodQuery(Qt::InputMethodQuery query) const
{
    Q_D(const QnQuickTextInput);

    switch (query)
    {
        case Qt::ImEnterKeyType:
            if (d->enterKeyType == EnterKeyDefault && nextInputItem())
                return EnterKeyNext;

            return d->enterKeyType;

        case Qt::ImCursorRectangle:
        {
            if (QnAppInfo::isIos())
                return QRectF(0, 0, 0, 0); //< Avoids screen shift when focus received.
            else
                break;
        }
        default:
            break;
    }

    return base_type::inputMethodQuery(query);
}

#endif

QQuickItem* QnQuickTextInput::nextInputItem() const
{
    // Seems like somebody forgot to make the getter const.
    const auto nextItem = const_cast<QnQuickTextInput*>(this)->nextItemInFocusChain();
    if (nextItem && nextItem->flags().testFlag(QQuickItem::ItemAcceptsInputMethod))
        return nextItem;
    return nullptr;
}

void QnQuickTextInput::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QnQuickTextInput);
    base_type::geometryChanged(newGeometry, oldGeometry);
    d->resizeBackground();
}

void QnQuickTextInput::mousePressEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);

    d->lastPos = event->pos();
    d->lastButton = event->button();
    d->lastButtons = event->buttons();
    d->lastModifiers = event->modifiers();
    d->pressAndHoldTimer->start();

    if (event->button() == Qt::LeftButton)
        base_type::mousePressEvent(event);

    d->hscrollWhenPressed = d->hscroll;
    d->dragStarted = false;
}

void QnQuickTextInput::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);

    if (qAbs(int(event->localPos().x() - d->pressPos.x()))
        > QGuiApplication::styleHints()->startDragDistance())
    {
        d->dragStarted = true;
    }

    if (d->dragStarted)
        d->pressAndHoldTimer->stop();

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

    auto dx = event->localPos().x() - d->pressPos.x();
    d->hscroll = qBound(0.0, d->hscrollWhenPressed - dx, hscrollMax);

    d->textLayoutDirty = true;
    d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
    polish();
    update();
    emit cursorRectangleChanged();
    if (d->cursorItem)
    {
        const auto r = cursorRectangle();
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
    {
        QQuickMouseEvent mouseEvent(
            event->pos().x(), event->pos().y(),
            event->button(), event->buttons(), event->modifiers(), true, false);
        emit clicked(&mouseEvent);
    }

    d->pressAndHoldTimer->stop();

    if (event->button() == Qt::LeftButton)
        base_type::mouseReleaseEvent(event);
}

void QnQuickTextInput::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);
    d->pressAndHoldTimer->stop();

    QQuickMouseEvent mouseEvent(
        event->pos().x(), event->pos().y(),
        event->button(), event->buttons(), event->modifiers(), true, false);
    emit doubleClicked(&mouseEvent);

    if (event->button() == Qt::LeftButton)
        base_type::mouseDoubleClickEvent(event);
}

void QnQuickTextInput::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
    {
        if (const auto item = nextInputItem())
        {
            item->forceActiveFocus();
            event->ignore();
            return;
        }
    }

    base_type::keyPressEvent(event);
}
