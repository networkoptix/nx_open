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
    if (!geometryNode)
    {
        geometryNode = new QSGGeometryNode();
        geometryNode->setGeometry(
            new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
        geometryNode->geometry()->setDrawingMode(GL_TRIANGLE_STRIP);
        geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
        geometryNode->setMaterial(Shader::createMaterial());
        geometryNode->material()->setFlag(QSGMaterial::Blending);
        node = geometryNode;
    }

    ensureRegionsTexture();

    const auto w = q->width();
    const auto h = q->height();

    const auto resolution = QSize(w - 1, h - 1); //< 1 pixel is reserved for right/bottom borders.
    const bool resized = resolution != m_currentState.resolution;
    m_currentState.resolution = resolution;

    if (resized)
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

    auto& currentState = *static_cast<QSGSimpleMaterial<State>*>(geometryNode->material())->state();
    if (currentState != m_currentState)
    {
        currentState = m_currentState;
        geometryNode->markDirty(QSGNode::DirtyMaterial);
    }

    return geometryNode;
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
            : QColor(Qt::red);

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

const char* MotionRegionsItem::Private::Shader::vertexShader() const
{
    static const QByteArray shader(core::graphics::ShaderHelper::modernizeShaderSource(R"GLSL(
        attribute vec4 vertex;
        attribute vec2 texCoord;

        uniform mat4 qt_Matrix;

        varying vec2 uv;

        void main()
        {
            gl_Position = qt_Matrix * vertex;
            uv = texCoord;
        }
    )GLSL"));
    return shader.constData();
}

const char* MotionRegionsItem::Private::Shader::fragmentShader() const
{
    static const QByteArray shader(core::graphics::ShaderHelper::modernizeShaderSource(R"GLSL(
        uniform float qt_Opacity;
        uniform float fillOpacity;
        uniform sampler2D sourceTexture;
        uniform vec4 borderColor;
        uniform vec2 step;

        varying vec2 uv;

        void main()
        {
            vec4 colors[4];
            colors[0] = texture2D(sourceTexture, uv);
            colors[1] = texture2D(sourceTexture, uv - vec2(step.x, 0.0));
            colors[2] = texture2D(sourceTexture, uv - step);
            colors[3] = texture2D(sourceTexture, uv - vec2(0.0, step.y));

            bool same = distance(colors[0], colors[1]) + distance(colors[0], colors[2])
                + distance(colors[0], colors[3]) == 0.0;

            bool boundary = (any(lessThan(uv, step)) || any(greaterThan(uv, vec2(1.0))))
                && colors[0].a != 0.0;

            gl_FragColor = ((same && !boundary) ? (colors[0] * fillOpacity) : vec4(borderColor))
                * qt_Opacity;
        }

    )GLSL"));
    return shader.constData();
}

QList<QByteArray> MotionRegionsItem::Private::Shader::attributes() const
{
    return {"vertex", "texCoord"};
}

void MotionRegionsItem::Private::Shader::updateState(
    const State* newState, const State* /*oldState*/)
{
    if (!newState->texture || !program()->isLinked())
        return;

    QOpenGLContext::currentContext()->functions()->glActiveTexture(GL_TEXTURE0);
    newState->texture->bind();

    program()->setUniformValue("borderColor", newState->borderColor);
    program()->setUniformValue("fillOpacity", GLfloat(newState->fillOpacity));
    program()->setUniformValue("step",
        QVector2D(1.0 / newState->resolution.width(), 1.0 / newState->resolution.height()));
}

void MotionRegionsItem::Private::Shader::resolveUniforms()
{
    if (program()->isLinked())
        program()->setUniformValue("sourceTexture", 0); //< GL_TEXTURE0.
}

} // namespace nx::vms::client::desktop
