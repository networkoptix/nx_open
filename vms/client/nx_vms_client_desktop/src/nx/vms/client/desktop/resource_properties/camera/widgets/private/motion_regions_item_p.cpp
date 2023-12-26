// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_regions_item_p.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QSGTexture>

#include <nx/vms/client/core/motion/helpers/camera_motion_helper.h>
#include <nx/vms/client/core/graphics/shader_helper.h>

#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/thread/custom_runnable.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr qreal kFontSizeMultiplier = 1.2; //< Multiplier for cell height.

} // namespace

//-------------------------------------------------------------------------------------------------
// MotionRegionsItem::Private

MotionRegionsItem::Private::Private(MotionRegionsItem* q):
    q(q),
    m_regionsImage(core::MotionGrid::kWidth, core::MotionGrid::kHeight,
        QImage::Format_RGBA8888_Premultiplied)
{
}

MotionRegionsItem::Private::~Private()
{
}

core::CameraMotionHelper* MotionRegionsItem::Private::motionHelper() const
{
    return m_motionHelper;
}

void MotionRegionsItem::Private::setMotionHelper(core::CameraMotionHelper* value)
{
    if (m_motionHelper == value)
        return;

    if (m_motionHelper)
        m_motionHelper->disconnect(q);

    const auto channelUpdated =
        [this](int channel)
        {
            if (channel != -1 && channel != m_channel)
                return;

            updateLabelPositions();
            invalidateRegionsTexture();
            q->update();
        };

    m_motionHelper = value;
    channelUpdated(-1);

    if (m_motionHelper)
    {
        connect(m_motionHelper, &QObject::destroyed, q,
            [this, channelUpdated]()
            {
                m_motionHelper.clear();
                channelUpdated(-1);
                emit q->motionHelperChanged();
            });

        connect(m_motionHelper, &core::CameraMotionHelper::motionRegionListChanged,
            q, channelUpdated);
    }

    emit q->motionHelperChanged();
}

int MotionRegionsItem::Private::channel() const
{
    return m_channel;
}

void MotionRegionsItem::Private::setChannel(int value)
{
    if (m_channel == value)
        return;

    m_channel = value;
    updateLabelPositions();
    invalidateRegionsTexture();
    q->update();

    emit q->channelChanged();
}

int MotionRegionsItem::Private::rotationQuadrants() const
{
    return m_rotationQuadrants;
}

void MotionRegionsItem::Private::setRotationQuadrants(int value)
{
    value = ((value % 4) + 4) % 4; //< Normalize to range [0, 3].
    if (m_rotationQuadrants == value)
        return;

    m_rotationQuadrants = value;
    updateLabelPositions(/*force*/ true);
    q->update();

    emit q->rotationQuadrantsChanged();
}

QVector<QColor> MotionRegionsItem::Private::sensitivityColors() const
{
    return m_sensitivityColors;
}

void MotionRegionsItem::Private::setSensitivityColors(const QVector<QColor>& value)
{
    if (m_sensitivityColors == value)
        return;

    m_sensitivityColors = value;
    invalidateRegionsTexture();
    q->update();

    emit q->sensitivityColorsChanged();
}

QColor MotionRegionsItem::Private::borderColor() const
{
    return m_currentState.borderColor;
}

void MotionRegionsItem::Private::setBorderColor(const QColor& value)
{
    if (m_currentState.borderColor == value)
        return;

    m_currentState.borderColor = value;
    q->update();

    emit q->borderColorChanged();
}

QColor MotionRegionsItem::Private::labelsColor() const
{
    return m_labelsColor;
}

void MotionRegionsItem::Private::setLabelsColor(const QColor& value)
{
    if (m_labelsColor == value)
        return;

    m_labelsColor = value;
    m_labelsTextureDirty = true;
    q->update();

    emit q->labelsColorChanged();
}

qreal MotionRegionsItem::Private::fillOpacity() const
{
    return m_currentState.fillOpacity;
}

void MotionRegionsItem::Private::setFillOpacity(qreal value)
{
    if (m_currentState.fillOpacity == value)
        return;

    m_currentState.fillOpacity = value;
    q->update();
}

QSGNode* MotionRegionsItem::Private::updatePaintNode(QSGNode* node)
{
    auto geometryNode = static_cast<QSGGeometryNode*>(node);
    bool nodeCreated{false};
    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
        geometryNode->geometry()->setDrawingMode(GL_TRIANGLE_STRIP);
        geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
        geometryNode->setMaterial(new Material());
        geometryNode->material()->setFlag(QSGMaterial::Blending);
        node = geometryNode;
        nodeCreated = true;
    }

    ensureRegionsTexture();

    const auto w = q->width();
    const auto h = q->height();

    const auto resolution = QSize(w - 1, h - 1); //< 1 pixel is reserved for right/bottom borders.
    const bool resized = resolution != m_currentState.resolution;
    m_currentState.resolution = resolution;

    if (resized || nodeCreated)
    {
        const auto tw = 1.0 + 1.0 / resolution.width();
        const auto th = 1.0 + 1.0 / resolution.height();
        auto data = geometryNode->geometry()->vertexDataAsTexturedPoint2D();
        data[0].set(0, 0, 0, 0);
        data[1].set(0, h, 0, th);
        data[2].set(w, 0, tw, 0);
        data[3].set(w, h, tw, th);
        geometryNode->markDirty(QSGNode::DirtyGeometry);
        m_labelsTextureDirty = true; //< Labels texture is resolution-dependent.
    }

    updateLabelsNode(geometryNode, resized || m_labelsDirty);

    auto& currentState = static_cast<Material*>(geometryNode->material())->uniforms;
    if (currentState != m_currentState)
    {
        currentState = m_currentState;
        currentState.dirty = true;
        geometryNode->markDirty(QSGNode::DirtyMaterial);
    }

    return geometryNode;
}

void MotionRegionsItem::Private::handleWindowChanged(QQuickWindow* win)
{
    if (windowConnection)
    {
        disconnect(windowConnection);
        windowConnection = {};
        cleanup();
    }

    if (!win)
        return;

    windowConnection = connect(win, &QQuickWindow::sceneGraphInvalidated, q,
        [this]() { cleanup(); }, Qt::DirectConnection);
}

void MotionRegionsItem::Private::cleanup()
{
    m_currentState.texture.reset();
    m_labelsTexture.reset();
}

void MotionRegionsItem::Private::releaseResources()
{
    const auto clearTextures =
        [texture = std::move(m_currentState.texture),
            labelsTexture = std::move(m_labelsTexture)]() mutable
        {
            texture.reset();
            labelsTexture.reset();
        };

    const auto window = q->window();
    window->scheduleRenderJob(
        new nx::utils::thread::CustomRunnable(clearTextures),
        QQuickWindow::AfterSynchronizingStage);
    window->update();
}

void MotionRegionsItem::Private::updateLabelsNode(QSGNode* mainNode, bool geometryDirty)
{
    auto labelsNode = mainNode->childCount() > 0
        ? static_cast<QSGGeometryNode*>(mainNode->childAtIndex(0))
        : nullptr;

    if (!m_labels.empty())
        ensureLabelsTexture();

    if (m_labels.empty() || !m_labelsTexture)
    {
        if (labelsNode)
            mainNode->removeChildNode(labelsNode);
    }
    else
    {
        if (!labelsNode)
        {
            labelsNode = new QSGGeometryNode();
            labelsNode->setFlags(
                QSGNode::OwnsGeometry | QSGNode::OwnsMaterial | QSGNode::OwnedByParent);
            labelsNode->setMaterial(new QSGTextureMaterial());
            labelsNode->material()->setFlag(QSGMaterial::Blending);
            labelsNode->setGeometry(new QSGGeometry(
                QSGGeometry::defaultAttributes_TexturedPoint2D(), 6 * m_labels.size()));
            labelsNode->geometry()->setDrawingMode(GL_TRIANGLES);

            geometryDirty = true;
            mainNode->appendChildNode(labelsNode);
        }

        if (geometryDirty)
        {
            labelsNode->geometry()->allocate(6 * m_labels.size());
            labelsNode->markDirty(QSGNode::DirtyGeometry);

            // Text item width and height.
            const bool transposed = (m_rotationQuadrants & 1) != 0;
            const qreal w = transposed ? m_labelSize.height() : m_labelSize.width();
            const qreal h = transposed ? m_labelSize.width() : m_labelSize.height();

            // Texture cell width.
            const qreal tw = 1.0 / (QnMotionRegion::kSensitivityLevelCount - 1);

            auto data = labelsNode->geometry()->vertexDataAsTexturedPoint2D();
            for (const auto& label: m_labels)
            {
                // Top left vertex position.
                const qreal x = std::ceil(m_cellSize.width() * label.position.x());
                const qreal y = std::ceil(m_cellSize.height() * label.position.y());

                // Left texture position.
                const qreal u = tw * (label.sensitivity - 1);

                std::array<QPointF, 4> uv{{{u, 0}, {u, 1}, {u + tw, 1}, {u + tw, 0}}};
                std::rotate(uv.rbegin(), uv.rbegin() + m_rotationQuadrants, uv.rend());

                data[0].set(x, y, uv[0].x(), uv[0].y());
                data[1].set(x, y + h, uv[1].x(), uv[1].y());
                data[2].set(x + w, y + h, uv[2].x(), uv[2].y());
                data[3] = data[0];
                data[4] = data[2];
                data[5].set(x + w, y, uv[3].x(), uv[3].y());
                data += 6;
            }
        }

        auto material = static_cast<QSGTextureMaterial*>(labelsNode->material());
        if (material->texture() != m_labelsTexture.data())
        {
            material->setTexture(m_labelsTexture.data());
            labelsNode->markDirty(QSGNode::DirtyMaterial);
        }
    }

    m_labelsDirty = false;
}

void MotionRegionsItem::Private::invalidateRegionsTexture()
{
    m_updateRegionsTexture = true;
}

void MotionRegionsItem::Private::ensureRegionsTexture()
{
    if (!m_updateRegionsTexture && m_currentState.texture)
        return;

    m_updateRegionsTexture = false;
    const auto window = q->window();
    if (!window)
        return;

    updateRegionsImage();

    m_currentState.texture.reset(window->createTextureFromImage(m_regionsImage));
    m_currentState.texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    m_currentState.texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    m_currentState.texture->setFiltering(QSGTexture::Nearest);
}

void MotionRegionsItem::Private::updateRegionsImage()
{
    m_regionsImage.fill(Qt::transparent);
    if (!m_motionHelper)
        return;

    const auto regions = m_motionHelper->motionRegionList();
    if (m_channel >= regions.size())
        return;

    QPainter painter(&m_regionsImage);
    const auto region = regions[m_channel];

    for (int sensitivity = 1; sensitivity < QnMotionRegion::kSensitivityLevelCount; ++sensitivity)
    {
        const auto path = regions[m_channel].getRegionBySensPath(sensitivity);
        const auto color = sensitivity < m_sensitivityColors.size()
            ? m_sensitivityColors[sensitivity]
            : QColor(Qt::red); //< Used in the same manner as an assert to show incorrect color theme.

        painter.fillPath(path, color);
    }
}

void MotionRegionsItem::Private::ensureLabelsTexture()
{
    if (!m_labelsTextureDirty)
        return;

    const auto window = q->window();
    if (!window)
    {
        m_labelsTexture.reset();
        return;
    }

    // Update cell size.
    m_cellSize.setWidth(qreal(m_currentState.resolution.width()) / core::MotionGrid::kWidth);
    m_cellSize.setHeight(qreal(m_currentState.resolution.height()) / core::MotionGrid::kHeight);

    // Label size as rounded two cells.
    m_labelSize = QSize(int(m_cellSize.width()), int(m_cellSize.height() * 2));

    // Texture size.
    const auto size = QSize(
        m_labelSize.width() * (QnMotionRegion::kSensitivityLevelCount - 1),
        m_labelSize.height())
            * window->effectiveDevicePixelRatio();

    if (m_labelsTexture && m_labelsTexture->textureSize() == size)
        return;

    QImage image(size, QImage::Format_RGBA8888_Premultiplied);
    image.setDevicePixelRatio(window->effectiveDevicePixelRatio());
    image.fill(Qt::transparent);

    QPainter painter(&image);
    QFont font;
    font.setPointSizeF(m_cellSize.height() * kFontSizeMultiplier);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(m_labelsColor);

    QRect rect(QPoint(0, 0), m_labelSize);
    for (int sensitivity = 1; sensitivity < QnMotionRegion::kSensitivityLevelCount; ++sensitivity)
    {
        painter.drawText(rect, Qt::AlignCenter, QString::number(sensitivity));
        rect.moveLeft(rect.left() + m_labelSize.width());
    }

    painter.end();

    m_labelsTexture.reset(window->createTextureFromImage(image));
    m_labelsTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    m_labelsTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    m_labelsTexture->setFiltering(QSGTexture::Nearest);
    m_labelsTextureDirty = false;
}

void MotionRegionsItem::Private::updateLabelPositions(bool force)
{
    if (!m_motionHelper)
        return;

    const auto regions = m_motionHelper->motionRegionList();
    if (m_channel >= regions.size())
        return;

    const auto region = regions[m_channel];
    core::MotionGrid grid;

    // Fill grid with sensitivity numbers.
    for (int sensitivity = 1; sensitivity < QnMotionRegion::kSensitivityLevelCount; ++sensitivity)
    {
        for (const auto& rect: region.getRectsBySens(sensitivity))
            grid.fillRect(rect, sensitivity);
    }

    if (!force && m_motionGrid == grid)
        return;

    m_labels.clear();
    m_motionGrid = grid;

    // Label takes 1x2 cells. Find good areas to fit labels in,
    // going from the top to the bottom, from the left to the right.

    switch (m_rotationQuadrants)
    {
        case 0:
        {
            for (int y = 0; y < core::MotionGrid::kHeight - 1; ++y)
            {
                for (int x = 0; x < core::MotionGrid::kWidth; ++x)
                {
                    const QPoint pos(x, y);
                    const int sensitivity = grid[pos];

                    // If there's no motion detection region, or we already labeled it.
                    if (sensitivity == 0)
                        continue;

                    // If a label doesn't fit.
                    if (grid[{x, y + 1}] != sensitivity)
                        continue;

                    // Add label position.
                    m_labels.push_back({sensitivity, pos});

                    // Clear processed region.
                    grid.clearRegion(pos);
                }
            }
            break;
        }

        case 1: //< 90 degrees.
        {
            for (int x = 0; x < core::MotionGrid::kWidth - 1; ++x)
            {
                for (int y = core::MotionGrid::kHeight - 1; y >= 0; --y)
                {
                    const QPoint pos(x, y);
                    const int sensitivity = grid[pos];

                    // If there's no motion detection region, or we already labeled it.
                    if (sensitivity == 0)
                        continue;

                    // If a label doesn't fit.
                    if (grid[{x + 1, y}] != sensitivity)
                        continue;

                    // Add label position.
                    m_labels.push_back({sensitivity, pos});

                    // Clear processed region.
                    grid.clearRegion(pos);
                }
            }
            break;
        }

        case 2: //< 180 degrees.
        {
            for (int y = core::MotionGrid::kHeight - 1; y > 0; --y)
            {
                for (int x = core::MotionGrid::kWidth - 1; x >= 0; --x)
                {
                    const QPoint pos(x, y);
                    const int sensitivity = grid[pos];

                    // If there's no motion detection region, or we already labeled it.
                    if (sensitivity == 0)
                        continue;

                    // If a label doesn't fit.
                    if (grid[{x, y - 1}] != sensitivity)
                        continue;

                    // Add label position.
                    m_labels.push_back({sensitivity, {pos.x(), pos.y() - 1}});

                    // Clear processed region.
                    grid.clearRegion(pos);
                }
            }
            break;
        }

        case 3: //< 270 degrees.
        {
            for (int x = core::MotionGrid::kWidth - 1; x > 0; --x)
            {
                for (int y = 0; y < core::MotionGrid::kHeight; ++y)
                {
                    const QPoint pos(x, y);
                    const int sensitivity = grid[pos];

                    // If there's no motion detection region, or we already labeled it.
                    if (sensitivity == 0)
                        continue;

                    // If a label doesn't fit.
                    if (grid[{x - 1, y}] != sensitivity)
                        continue;

                    // Add label position.
                    m_labels.push_back({sensitivity, {pos.x() - 1, pos.y()}});

                    // Clear processed region.
                    grid.clearRegion(pos);
                }
            }
            break;
        }
    };

    m_labelsDirty = true;
}

//-------------------------------------------------------------------------------------------------
// MotionRegionsItem::Private::State

bool MotionRegionsItem::Private::State::operator==(const State& other) const
{
    return resolution == other.resolution
        && borderColor == other.borderColor
        && texture == other.texture
        && qFuzzyIsNull(fillOpacity - other.fillOpacity);
}

bool MotionRegionsItem::Private::State::operator!=(const State& other) const
{
    return !(*this == other);
}

//-------------------------------------------------------------------------------------------------
// MotionRegionsItem::Private::Shader

MotionRegionsItem::Private::Shader::Shader()
{
    setShaderFileName(VertexStage, QLatin1String(":/shaders/qsb/motion_regions.vert.qsb"));
    setShaderFileName(FragmentStage, QLatin1String(":/shaders/qsb/motion_regions.frag.qsb"));
}

static inline QVector4D colorToVec4(const QColor& c)
{
    return QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF());
}

bool MotionRegionsItem::Private::Shader::updateUniformData(
    RenderState& state,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial)
{
    bool changed = false;
    QByteArray* buf = state.uniformData();
    NX_ASSERT(buf->size() >= 96);

    if (state.isMatrixDirty())
    {
        const QMatrix4x4 m = state.combinedMatrix();
        memcpy(buf->data(), m.constData(), 64);
        changed = true;
    }

    if (state.isOpacityDirty())
    {
        const float opacity = state.opacity();
        memcpy(buf->data() + 64, &opacity, 4);
        changed = true;
    }

    auto* material = static_cast<Material*>(newMaterial);

    if (oldMaterial != newMaterial || material->uniforms.dirty)
    {
        memcpy(buf->data() + 68, &material->uniforms.fillOpacity, 4);

        const QVector2D step(
            1.0 / material->uniforms.resolution.width(),
            1.0 / material->uniforms.resolution.height());
        memcpy(buf->data() + 72, &step, 8);

        const QVector4D borderColor = colorToVec4(material->uniforms.borderColor);
        memcpy(buf->data() + 80, &borderColor, 16);

        material->uniforms.dirty = false;
        changed = true;
    }

    return changed;
}

void MotionRegionsItem::Private::Shader::updateSampledImage(
    QSGMaterialShader::RenderState& state,
    int binding,
    QSGTexture** texture,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial)
{
    if (binding != 1)
        return;

    NX_ASSERT(oldMaterial == nullptr || newMaterial->type() == oldMaterial->type());

    Material* tx = static_cast<Material*>(newMaterial);
    QSGTexture* t = tx->uniforms.texture.get();
    t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    *texture = t;
}

MotionRegionsItem::Private::Material::Material()
{
}

QSGMaterialType* MotionRegionsItem::Private::Material::type() const
{
    static QSGMaterialType type;
    return &type;
}

int MotionRegionsItem::Private::Material::compare(const QSGMaterial *other) const
{
    if (!other || type() != other->type())
        return 0;

    const auto otherMaterial = static_cast<const Material*>(other);

    const qint64 diff =
        uniforms.texture->comparisonKey() - otherMaterial->uniforms.texture->comparisonKey();

    if (diff != 0)
        return diff < 0 ? -1 : 1;

    const QSize resolutionDiff = uniforms.resolution  - otherMaterial->uniforms.resolution;
    if (!resolutionDiff.isNull())
        return resolutionDiff.width() <= 0 ? -1 : 1;

    const float opacityDiff = uniforms.fillOpacity - otherMaterial->uniforms.fillOpacity;
    if (qFuzzyIsNull(opacityDiff) != 0.0)
        return opacityDiff < 0 ? -1 : 1;

    if (uniforms.borderColor != otherMaterial->uniforms.borderColor)
    {
        return otherMaterial->uniforms.borderColor.rgba() > uniforms.borderColor.rgba()
            ? -1
            : 1;
    }

    return otherMaterial == this ? 0 : 1;
}

} // namespace nx::vms::client::desktop
