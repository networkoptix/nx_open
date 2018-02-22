#pragma once

#include <QtQuick/QQuickItem>

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
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(int lineSpacing READ lineSpacing WRITE setLineSpacing NOTIFY lineSpacingChanged)

public:
    VoiceSpectrumItem(QQuickItem* parent = nullptr);

    virtual ~VoiceSpectrumItem() override;

    virtual QSGNode* updatePaintNode(
        QSGNode* node,
        UpdatePaintNodeData* updatePaintNodeData) override;

    QColor color() const;
    void setColor(const QColor& color);

    int lineWidth() const;
    void setLineWidth(int value);

    int lineSpacing() const;
    void setLineSpacing(int value);

signals:
    void colorChanged();
    void lineWidthChanged();
    void lineSpacingChanged();

private:
    void updateNodeGeometry();
    void updateNodeMaterial();
    void clearGeometryData();

    QSGGeometryNode* getNode();

private:
    class VisualizerDataGenerator;
    using VisualizerDataGeneratorPtr = QScopedPointer<VisualizerDataGenerator>;

    const VisualizerDataGeneratorPtr m_generator;

    QColor m_color = Qt::red;

    int m_lastLinesCount = 0;
    QSGFlatColorMaterial* m_material = nullptr;
    QSGGeometryNode* m_node = nullptr;
    QSGGeometry* m_geometry = nullptr;
};

} // namespace mobile
} // namespace client
} // namespace nx
