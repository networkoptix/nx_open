#include "text_input.h"

#include <QtQuick/private/qquicktextinput_p_p.h>
#include <QtQuick/private/qquickclipnode_p.h>

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
};

QnQuickTextInputPrivate::QnQuickTextInputPrivate()
    : scrollByMouse(true)
    , hscrollWhenPressed(0)
    , background(nullptr)
    , dragStarted(false)
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

QnQuickTextInput::QnQuickTextInput(QQuickItem* parent)
    : base_type(*(new QnQuickTextInputPrivate), parent)
{
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

void QnQuickTextInput::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Q_D(QnQuickTextInput);
    base_type::geometryChanged(newGeometry, oldGeometry);
    d->resizeBackground();
}

void QnQuickTextInput::mousePressEvent(QMouseEvent* event)
{
    Q_D(QnQuickTextInput);

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
