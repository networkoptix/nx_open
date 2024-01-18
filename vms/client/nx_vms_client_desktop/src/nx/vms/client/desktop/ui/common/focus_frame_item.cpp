// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "focus_frame_item.h"

#include <cmath>
#include <memory>

#include <QtGui/QImage>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QSGGeometryNode>

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/custom_runnable.h>
#include <nx/vms/client/core/scene_graph/texture_material.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kTextureSize = 2;

} // namespace

class FocusFrameItem::Private: public QObject
{
    FocusFrameItem* const q = nullptr;

public:
    Private(FocusFrameItem* parent);

public:
    QColor color = core::colorTheme()->color("dark1");
    int frameWidth = 1;

    bool textureDirty = true;

    QSGTexture* createDotTexture();
};

FocusFrameItem::Private::Private(FocusFrameItem* parent): q(parent)
{
}

QSGTexture* FocusFrameItem::Private::createDotTexture()
{
    const auto window = q->window();
    if (!NX_ASSERT(window))
        return nullptr;

    QImage image(kTextureSize, kTextureSize, QImage::Format_RGBA8888);
    image.setPixelColor(0, 0, color);
    image.setPixelColor(1, 1, color);
    image.setPixelColor(0, 1, Qt::transparent);
    image.setPixelColor(1, 0, Qt::transparent);

    return window->createTextureFromImage(image);
}

FocusFrameItem::FocusFrameItem(QQuickItem* parent):
    base_type(parent),
    d(new Private(this))
{
    setFlag(QQuickItem::ItemHasContents);
}

FocusFrameItem::~FocusFrameItem()
{
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

int FocusFrameItem::frameWidth() const
{
    return d->frameWidth;
}

void FocusFrameItem::setFrameWidth(int frameWidth)
{
    if (d->frameWidth == frameWidth)
        return;

    d->frameWidth = frameWidth;
    emit frameWidthChanged();

    update();
}

void FocusFrameItem::registerQmlType()
{
    qmlRegisterType<FocusFrameItem>("nx.client.desktop", 1, 0, "FocusFrame");
}

QSGNode* FocusFrameItem::updatePaintNode(
    QSGNode* node, QQuickItem::UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    auto geometryNode = dynamic_cast<QSGGeometryNode*>(node);
    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4 * 6));
        geometryNode->geometry()->setDrawingMode(QSGGeometry::DrawTriangles);
        geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    }

    auto material = dynamic_cast<core::sg::TextureMaterial*>(geometryNode->material());
    if (!material)
    {
        material = new core::sg::TextureMaterial();
        material->setHorizontalWrapMode(QSGTexture::Repeat);
        material->setVerticalWrapMode(QSGTexture::Repeat);
        geometryNode->setMaterial(material);
        d->textureDirty = true;
    }

    if (d->textureDirty)
    {
        material->resetTexture(d->createDotTexture());
        geometryNode->markDirty(QSGNode::DirtyMaterial);
        d->textureDirty = false;
    }

    const auto fw = d->frameWidth;

    auto addRectangle =
        [fw](QSGGeometry::TexturedPoint2D*& data, const QRectF& rect)
        {
            const auto x = static_cast<float>(rect.x());
            const auto y = static_cast<float>(rect.y());
            const auto w = static_cast<float>(std::round(rect.width() / fw) * fw);
            const auto h = static_cast<float>(std::round(rect.height() / fw) * fw);
            const auto r = x + w;
            const auto b = y + h;
            const auto twx = w / kTextureSize / fw;
            const auto thy = h / kTextureSize / fw;
            // We use chessboard 2x2 texture to avoid sibling painted points of the frame.
            // Second texture column is used when frame edge has odd x or odd y and not both.
            const auto ts = ((qRound(rect.x() / fw) + qRound(rect.y() / fw)) & 1) ? 0.5f : 0.0f;

            data[0].set(x, y, ts, 0);
            data[1].set(r, y, twx + ts, 0);
            data[2].set(x, b, ts, thy);
            data[3].set(r, y, twx + ts, 0);
            data[4].set(x, b, ts, thy);
            data[5].set(r, b, twx + ts, thy);

            data += 6;
        };

    const auto w = width();
    const auto h = height();
    auto data = geometryNode->geometry()->vertexDataAsTexturedPoint2D();

    // Rectangles are shifted by dotSize to avoid painting corner dots multiple times.
    // This is important when the color is semi-transparent.
    addRectangle(data, QRectF(0, 0, w - fw, fw));
    addRectangle(data, QRectF(w - fw, 0, fw, h - fw));
    addRectangle(data, QRectF(fw, h - fw, w - fw, fw));
    addRectangle(data, QRectF(0, fw, fw, h - fw));

    geometryNode->markDirty(QSGNode::DirtyGeometry);

    return geometryNode;
}

} // namespace nx::vms::client::desktop
