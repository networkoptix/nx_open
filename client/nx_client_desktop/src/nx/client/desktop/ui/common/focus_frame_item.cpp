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
    QSGGeometryNode* leftNode = nullptr;
    QSGGeometryNode* rightNode = nullptr;
    QSGGeometryNode* topNode = nullptr;
    QSGGeometryNode* bottomNode = nullptr;

    if (!node)
    {
        node = new QSGNode();

        auto createGeometryNode =
            [this, node]()
            {
                auto geometryNode = new QSGGeometryNode();
                geometryNode->setGeometry(
                    new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
                geometryNode->setFlag(QSGNode::OwnsGeometry);
                geometryNode->setMaterial(d->material.data());
                node->appendChildNode(geometryNode);

                return geometryNode;
            };

        leftNode = createGeometryNode();
        rightNode = createGeometryNode();
        topNode = createGeometryNode();
        bottomNode = createGeometryNode();
    }
    else
    {
        leftNode = static_cast<QSGGeometryNode*>(node->childAtIndex(0));
        rightNode = static_cast<QSGGeometryNode*>(node->childAtIndex(1));
        topNode = static_cast<QSGGeometryNode*>(node->childAtIndex(2));
        bottomNode = static_cast<QSGGeometryNode*>(node->childAtIndex(3));
    }

    auto setNodesDirtyState =
        [leftNode, rightNode, topNode, bottomNode](QSGNode::DirtyStateBit bit)
        {
            leftNode->markDirty(bit);
            rightNode->markDirty(bit);
            topNode->markDirty(bit);
            bottomNode->markDirty(bit);
        };

    if (d->textureDirty)
    {
        d->updateDotTexture();
        setNodesDirtyState(QSGNode::DirtyMaterial);
    }

    auto setRectToData =
        [](QSGGeometry::TexturedPoint2D* data, const QRectF& rect)
        {
            const auto x = static_cast<float>(rect.x());
            const auto y = static_cast<float>(rect.y());
            const auto w = static_cast<float>(rect.width());
            const auto h = static_cast<float>(rect.height());
            const auto twx = w / kTextureSize;
            const auto thy = h / kTextureSize;
            // We use chessboard 2x2 texture to avoid sibling painted points of the frame.
            // Second texture column is used when frame edge has odd x or odd y and not both.
            const auto ts = ((qRound(rect.x()) + qRound(rect.y())) & 1) ? 0.5f : 0.0f;

            data[0].set(x, y, ts, 0);
            data[1].set(x + w, y, twx + ts, 0);
            data[2].set(x, y + h, ts, thy);
            data[3].set(x + w, y + h, twx + ts, thy);
        };

    setRectToData(
        leftNode->geometry()->vertexDataAsTexturedPoint2D(),
        QRectF(0, 0, kDotSize, height()));
    setRectToData(
        rightNode->geometry()->vertexDataAsTexturedPoint2D(),
        QRectF(width() - kDotSize, 0, kDotSize, height()));
    setRectToData(
        topNode->geometry()->vertexDataAsTexturedPoint2D(),
        QRectF(0, 0, width(), kDotSize));
    setRectToData(
        bottomNode->geometry()->vertexDataAsTexturedPoint2D(),
        QRectF(0, height() - kDotSize, width(), kDotSize));

    setNodesDirtyState(QSGNode::DirtyGeometry);

    return node;
}

} // namespace desktop
} // namespace client
} // namespace nx
