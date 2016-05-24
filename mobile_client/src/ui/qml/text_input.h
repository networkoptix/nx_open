#pragma once

#include <QtQuick/private/qquicktextinput_p.h>

class QnQuickTextInputPrivate;

class QnQuickTextInput : public QQuickTextInput
{
    Q_OBJECT

    Q_PROPERTY(bool scrollByMouse READ scrollByMouse WRITE setScrollByMouse NOTIFY scrollByMouseChanged)
    Q_PROPERTY(QQuickItem* background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)

    typedef QQuickTextInput base_type;

public:
    QnQuickTextInput(QQuickItem* parent = nullptr);
    ~QnQuickTextInput();

    bool scrollByMouse() const;
    void setScrollByMouse(bool enable);

    QQuickItem* background() const;
    void setBackground(QQuickItem* background);

    QString placeholderText() const;
    void setPlaceholderText(const QString& text);

    virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

signals:
    void scrollByMouseChanged();
    void backgroundChanged();
    void placeholderTextChanged();
    void clicked();

protected:
    virtual void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnQuickTextInput);
};
