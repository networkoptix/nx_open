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

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------

class MotionMaskItemTextureProvider: public QSGTextureProvider
{
public:
    void updateTexture(QSGTexture* texture)
    {
        if (m_texture == texture)
            return;

        m_texture = texture;
        emit textureChanged();
    }

    virtual QSGTexture* texture() const override
    {
        return m_texture;
    }

private:
    QSGTexture* m_texture = nullptr;
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

    void ensureTexture()
    {
        if (!textureDirty)
            return;

        if (auto window = q->window())
        {
            const auto bitmap = QBitmap::fromData(QSize(MotionGrid::kHeight, MotionGrid::kWidth),
                reinterpret_cast<const uchar*>(mask.data()), QImage::Format_Mono);

            // Rotate90 and mirror are operations optimized by QImage, while transposition
            // - single transformation with QMatrix(0,1,1,0, 0,0) - is not optimized.
            auto image = bitmap.toImage().transformed(QMatrix().rotate(-90)).mirrored();
            image.setColorTable({0, 0xFFFFFFFF});

            texture.reset(window->createTextureFromImage(image));
            texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            texture->setVerticalWrapMode(QSGTexture::ClampToEdge);

            if (provider)
                provider->updateTexture(texture.data());

            textureDirty = false;
        }
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

        textureDirty = true;
        q->update();
        emit q->motionMaskChanged();
    }

public:
    QByteArray mask = QByteArray(kMotionMaskSizeBytes, 0);
    QScopedPointer<QSGTexture, QScopedPointerDeleteLater> texture;
    bool textureDirty = true;
    MotionMaskItemTextureProvider* provider = nullptr;
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
        d->provider->updateTexture(d->texture.data());
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
    auto geometryNode = static_cast<QSGGeometryNode*>(node);
    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
        geometryNode->geometry()->setDrawingMode(GL_TRIANGLE_STRIP);
        geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
        geometryNode->setMaterial(new QSGTextureMaterial());
        geometryNode->material()->setFlag(QSGMaterial::Blending);
        node = geometryNode;
    }

    d->ensureTexture();

    auto material = static_cast<QSGTextureMaterial*>(geometryNode->material());
    if (material->texture() != d->texture.data())
    {
        material->setTexture(d->texture.data());
        geometryNode->markDirty(QSGNode::DirtyMaterial);
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
