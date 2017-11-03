#include "focus_frame_item.h"

#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QSGGeometryNode>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kDotSize = 1;
static constexpr int kTextureSize = kDotSize * 2;

} // namespace

class FocusFrameItem::Private
{
    FocusFrameItem* const q = nullptr;

public:
    Private(FocusFrameItem* parent);

public:
    QColor color;
    bool textureDirty = true;
    QScopedPointer<QSGTextureMaterial> material;

    void updateDotTexture();
};

FocusFrameItem::Private::Private(FocusFrameItem* parent):
    q(parent),
    material(new QSGTextureMaterial())
{
}

void FocusFrameItem::Private::updateDotTexture()
{
    const auto window = q->window();
    if (!window)
        return;

    QImage image(kTextureSize, kTextureSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.fillRect(0, 0, kDotSize, kDotSize, color);
    painter.fillRect(kDotSize, kDotSize, kDotSize, kDotSize, color);
    painter.end();

    const auto texture = window->createTextureFromImage(image);

    if (material->texture())
        delete material->texture();

    material->setTexture(texture);
    material->setHorizontalWrapMode(QSGTexture::Repeat);
    material->setVerticalWrapMode(QSGTexture::Repeat);
    textureDirty = false;
}

FocusFrameItem::FocusFrameItem(QQuickItem* parent):
    base_type(parent),
    d(new Private(this))
{
    setFlag(QQuickItem::ItemHasContents);
}

FocusFrameItem::~FocusFrameItem()
{
    if (d->material->texture())
        delete d->material->texture();
}

QColor FocusFrameItem::color() const
{
    return d->color;
}

void FocusFrameItem::setColor(const QColor& color)
{
    if (d->color == color)
        return;

    d->color = color;
    emit colorChanged();

    d->textureDirty = true;
    update();
}

void FocusFrameItem::registerQmlType()
{
    qmlRegisterType<FocusFrameItem>("nx.client.desktop", 1, 0, "FocusFrame");
}

QSGNode* FocusFrameItem::updatePaintNode(
    QSGNode* node, QQuickItem::UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    auto geometryNode = static_cast<QSGGeometryNode*>(node);

    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4 * 2 + 2));
        geometryNode->setFlag(QSGNode::OwnsGeometry);
        geometryNode->setMaterial(d->material.data());

        node = geometryNode;
    }

    if (d->textureDirty)
    {
        d->updateDotTexture();
        geometryNode->markDirty(QSGNode::DirtyMaterial);
    }

    auto data = geometryNode->geometry()->vertexDataAsTexturedPoint2D();

    auto addPoint =
        [&data](const QPointF& point)
        {
            const auto x = static_cast<float>(point.x());
            const auto y = static_cast<float>(point.y());

            // We use chessboard 2x2 texture to avoid sibling painted points of the frame.
            // Second texture column is used when frame edge has odd x or odd y and not both.
            const auto ts = (point.toPoint().manhattanLength() & 1) ? 0.5f : 0.0f;

            data->set(x, y, x / kTextureSize + ts, y / kTextureSize + ts);
            ++data;
        };

    const auto outerRect = QRectF(0, 0, width(), height());
    const auto innerRect = outerRect.adjusted(kDotSize, kDotSize, -kDotSize, -kDotSize);

    addPoint(outerRect.topLeft());
    addPoint(innerRect.topLeft());
    addPoint(outerRect.topRight());
    addPoint(innerRect.topRight());
    addPoint(outerRect.bottomRight());
    addPoint(innerRect.bottomRight());
    addPoint(outerRect.bottomLeft());
    addPoint(innerRect.bottomLeft());
    addPoint(outerRect.topLeft());
    addPoint(innerRect.topLeft());

    geometryNode->markDirty(QSGNode::DirtyGeometry);

    return node;
}

} // namespace desktop
} // namespace client
} // namespace nx
