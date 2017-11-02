#pragma once

#include <QtQuick/QQuickItem>

namespace nx {
namespace client {
namespace desktop {

class FocusFrameItem: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

    using base_type = QQuickItem;

public:
    FocusFrameItem(QQuickItem* parent = nullptr);
    virtual ~FocusFrameItem() override;

    QColor color() const;
    void setColor(const QColor& color);

    static void registerQmlType();

signals:
    void colorChanged();

protected:
    virtual QSGNode* updatePaintNode(
        QSGNode* node,
        UpdatePaintNodeData* updatePaintNodeData) override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
