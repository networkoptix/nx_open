#pragma once

#include <QtQuick/QQuickItem>
#include <QtCore/QElapsedTimer>

class QSGGeometry;
class QSGGeometryNode;
class QSGFlatColorMaterial;

namespace nx {
namespace client {
namespace mobile {

class VoiceSpectrumItem: public QQuickItem
{
    Q_OBJECT
    using base_type = QQuickItem;

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(int elementsCount READ elementsCount WRITE setElementsCount
        NOTIFY elementsCountChanged)

public:
    VoiceSpectrumItem(QQuickItem* parent = nullptr);

    virtual QSGNode* updatePaintNode(
        QSGNode* node,
        UpdatePaintNodeData* updatePaintNodeData) override;

    int elementsCount() const;
    void setElementsCount(int value);

    QColor color() const;
    void setColor(const QColor& color);

signals:
    void elementsCountChanged();
    void colorChanged();

private:
    void updateNodeGeometry();
    void updateNodeMaterial();

    QSGGeometryNode* getNode();

private:
    QElapsedTimer m_animationTimer;
    int m_lastWidth = 0;
    int m_elementsCount = 10;
    QColor m_color = Qt::red;
    QSGFlatColorMaterial* m_material = nullptr;
    QSGGeometryNode* m_node = nullptr;
    QSGGeometry* m_geometry = nullptr;
};

} // namespace mobile
} // namespace client
} // namespace nx
