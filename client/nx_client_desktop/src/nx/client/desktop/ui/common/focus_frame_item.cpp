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
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4 * 6));
        geometryNode->geometry()->setDrawingMode(GL_TRIANGLES);
        geometryNode->setFlag(QSGNode::OwnsGeometry);
        geometryNode->setMaterial(d->material.data());

        node = geometryNode;
    }

    if (d->textureDirty)
    {
        d->updateDotTexture();
        geometryNode->markDirty(QSGNode::DirtyMaterial);
    }

    auto addRectangle =
        [](QSGGeometry::TexturedPoint2D*& data, const QRectF& rect)
        {
            const auto x = static_cast<float>(rect.x());
            const auto y = static_cast<float>(rect.y());
            const auto r = static_cast<float>(rect.right());
            const auto b = static_cast<float>(rect.bottom());
            const auto twx = (r - x) / kTextureSize;
            const auto thy = (b - y) / kTextureSize;
            // We use chessboard 2x2 texture to avoid sibling painted points of the frame.
            // Second texture column is used when frame edge has odd x or odd y and not both.
            const auto ts = ((qRound(rect.x()) + qRound(rect.y())) & 1) ? 0.5f : 0.0f;

            data[0].set(x, y, ts, 0);
            data[1].set(r, y, twx + ts, 0);
            data[2].set(x, b, ts, thy);
            data[3].set(r, y, twx + ts, 0);
            data[4].set(x, b, ts, thy);
            data[5].set(r, b, twx + ts, thy);

            data += 6;
        };

    auto data = geometryNode->geometry()->vertexDataAsTexturedPoint2D();

    // Rectangles are shifted by kDotSize to avoid painting corner dots multiple times.
    // This is important when the color is semi-transparent.
    addRectangle(data, QRectF(0, 0, kDotSize, height()));
    addRectangle(data, QRectF(width() - kDotSize, 0, kDotSize, height()));
    addRectangle(data, QRectF(kDotSize, 0, width() - 2 * kDotSize, kDotSize));
    addRectangle(data, QRectF(kDotSize, height() - kDotSize, width() - 2 * kDotSize, kDotSize));

    geometryNode->markDirty(QSGNode::DirtyGeometry);

    return node;
}

} // namespace desktop
} // namespace client
} // namespace nx
