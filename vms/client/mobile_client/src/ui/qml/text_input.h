#pragma once

#include <QtQuick/private/qquicktextinput_p.h>

class QQuickMouseEvent;

class QnQuickTextInputPrivate;

class QnQuickTextInput: public QQuickTextInput
{
    Q_OBJECT

    Q_PROPERTY(bool scrollByMouse READ scrollByMouse WRITE setScrollByMouse NOTIFY scrollByMouseChanged)
    Q_PROPERTY(QQuickItem* background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(EnterKeyType enterKeyType
        READ enterKeyType WRITE setEnterKeyType NOTIFY enterKeyTypeChanged)

    typedef QQuickTextInput base_type;

public:
    enum EnterKeyType
    {
        EnterKeyDefault = Qt::EnterKeyDefault,
        EnterKeyReturn = Qt::EnterKeyReturn,
        EnterKeyDone = Qt::EnterKeyDone,
        EnterKeyGo = Qt::EnterKeyGo,
        EnterKeySend = Qt::EnterKeySend,
        EnterKeySearch = Qt::EnterKeySearch,
        EnterKeyNext = Qt::EnterKeyNext,
        EnterKeyPrevious = Qt::EnterKeyPrevious
    };
    Q_ENUM(EnterKeyType)

    QnQuickTextInput(QQuickItem* parent = nullptr);
    ~QnQuickTextInput();

    bool scrollByMouse() const;
    void setScrollByMouse(bool enable);

    QQuickItem* background() const;
    void setBackground(QQuickItem* background);

    QString placeholderText() const;
    void setPlaceholderText(const QString& text);

    EnterKeyType enterKeyType() const;
    void setEnterKeyType(EnterKeyType enterKeyType);

    virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

#ifndef QT_NO_IM
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
#endif

protected:
    QQuickItem* nextInputItem() const;

signals:
    void scrollByMouseChanged();
    void backgroundChanged();
    void placeholderTextChanged();
    void clicked(QQuickMouseEvent* event);
    void pressAndHold(QQuickMouseEvent* event);
    void doubleClicked(QQuickMouseEvent* event);
    void enterKeyTypeChanged();

protected:
    virtual void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnQuickTextInput)

private:
    QPoint m_contextMenuPos;
    int m_selectionStart = -1;
    int m_selectionEnd = -1;
    int m_cursorPosition = -1;
};
