// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_mask_item.h"

#include <QtQml/QtQml>
#include <QtGui/QBitmap>
#include <QtGui/QImage>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGTexture>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QSGTextureProvider>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/private/qquickwindow_p.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/custom_runnable.h>
#include <nx/vms/client/core/scene_graph/texture_material.h>

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------

class MotionMaskItemTextureProvider: public QSGTextureProvider
{
public:
    void updateTexture(QSGTexture* texture)
    {
        if (m_texture == texture)
            return;

        if (m_texture)
            m_texture->disconnect(this);

        m_texture = texture;
        emit textureChanged();

        if (m_texture)
        {
            connect(m_texture, &QObject::destroyed, this, [this]() { updateTexture(nullptr); },
                Qt::DirectConnection);
        }
    }

    virtual QSGTexture* texture() const override
    {
        return m_texture;
    }

private:
    QPointer<QSGTexture> m_texture = nullptr;
};

//-------------------------------------------------------------------------------------------------

class MotionMaskItem::Private
{
    MotionMaskItem* const q = nullptr;

public:
    explicit Private(MotionMaskItem* q):
        q(q)
    {
    }

    QSGTexture* createTexture() const
    {
        const auto window = q->window();
        if (!NX_ASSERT(window))
            return nullptr;

        const auto bitmap = QBitmap::fromData(QSize(MotionGrid::kHeight, MotionGrid::kWidth),
            reinterpret_cast<const uchar*>(mask.data()), QImage::Format_Mono);

        // Rotate90 and mirror are operations optimized by QImage, while transposition
        // - single transformation with QMatrix(0,1,1,0, 0,0) - is not optimized.
        auto image = bitmap.toImage().transformed(QTransform().rotate(-90)).mirrored();
        image.setColorTable({0, 0xFFFFFFFF});

        const auto newTexture = window->createTextureFromImage(image);
        newTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        newTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        return newTexture;
    }

    QByteArray motionMask() const
    {
        return mask;
    }

    void setMotionMask(QByteArray value)
    {
        if (value.size() != kMotionMaskSizeBytes)
            value.resize(kMotionMaskSizeBytes);

        if (mask == value)
            return;

        mask = value;
        emit q->motionMaskChanged();

        textureDirty = true;
        q->update();
    }

public:
    QByteArray mask = QByteArray(kMotionMaskSizeBytes, 0);
    bool textureDirty = true;

    mutable QPointer<MotionMaskItemTextureProvider> provider;
    QPointer<QSGTexture> texture; //< A weak pointer to the texture owned by the SG.
};

//-------------------------------------------------------------------------------------------------

MotionMaskItem::MotionMaskItem(QQuickItem* parent):
    base_type(parent),
    d(new Private(this))
{
    setFlag(QQuickItem::ItemHasContents);
}

MotionMaskItem::~MotionMaskItem()
{
}

bool MotionMaskItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider* MotionMaskItem::textureProvider() const
{
    if (!d->provider)
    {
        d->provider = new MotionMaskItemTextureProvider();
        d->provider->updateTexture(d->texture);
    }

    return d->provider;
}

QByteArray MotionMaskItem::motionMask() const
{
    return d->motionMask();
}

void MotionMaskItem::setMotionMask(const QByteArray& value)
{
    d->setMotionMask(value);
}

QSGNode* MotionMaskItem::updatePaintNode(
    QSGNode* node, UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    auto geometryNode = dynamic_cast<QSGGeometryNode*>(node);
    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
        geometryNode->geometry()->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    }

    auto material = dynamic_cast<core::sg::TextureMaterial*>(geometryNode->material());
    if (!material)
    {
        material = new core::sg::TextureMaterial();
        material->setFlag(QSGMaterial::Blending);
        geometryNode->setMaterial(material);
        d->textureDirty = true;
    }

    if (d->textureDirty)
    {
        d->texture = d->createTexture();
        if (d->provider)
            d->provider->updateTexture(d->texture);

        material->resetTexture(d->texture);
        geometryNode->markDirty(QSGNode::DirtyMaterial);
        d->textureDirty = false;
    }

    const auto w = (float) width();
    const auto h = (float) height();
    auto data = geometryNode->geometry()->vertexDataAsTexturedPoint2D();
    data[0].set(0, 0, 0, 0);
    data[1].set(0, h, 0, 1);
    data[2].set(w, 0, 1, 0);
    data[3].set(w, h, 1, 1);
    geometryNode->markDirty(QSGNode::DirtyGeometry);

    return geometryNode;
}

void MotionMaskItem::releaseResources()
{
    if (d->provider)
    {
        QQuickWindowQObjectCleanupJob::schedule(window(), d->provider);
        d->provider = nullptr;
    }
}

void MotionMaskItem::registerQmlType()
{
    qmlRegisterType<MotionMaskItem>("nx.client.core", 1, 0, "MotionMaskItem");
}

} // namespace nx::vms::client::core
