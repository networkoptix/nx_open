// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QImage>
#include <QtQuick/QSGMaterial>
#include <QtQuick/QSGMaterialShader>

#include <nx/vms/client/core/motion/motion_grid.h>
#include <nx/vms/client/core/skin/color_theme.h>

#include "../motion_regions_item.h"

class QSGNode;
class QSGMaterial;
class QSGTexture;

namespace nx::vms::client::desktop {

class MotionRegionsItem::Private
{
    MotionRegionsItem* const q = nullptr;

    class Shader;
    class LabelsModel;

public:
    Private(MotionRegionsItem* q);
    virtual ~Private();

    core::CameraMotionHelper* motionHelper() const;
    void setMotionHelper(core::CameraMotionHelper* value);

    int channel() const;
    void setChannel(int value);

    int rotationQuadrants() const;
    void setRotationQuadrants(int value);

    QVector<QColor> sensitivityColors() const;
    void setSensitivityColors(const QVector<QColor>& value);

    QColor borderColor() const;
    void setBorderColor(const QColor& value);

    QColor labelsColor() const;
    void setLabelsColor(const QColor& value);

    qreal fillOpacity() const;
    void setFillOpacity(qreal value);

    QSGNode* updatePaintNode(QSGNode* node);

private:
    QImage createRegionsImage() const;
    QSGTexture* createRegionsTexture() const;

    QImage createLabelsImage() const;
    QSGTexture* createLabelsTexture() const;

    void updateLabelPositions(bool force = false);
    void updateLabelsNode(QSGNode* mainNode, bool geometryDirty);

private:
    struct State
    {
        QSize resolution;
        QColor borderColor = core::colorTheme()->color("dark1");
        float fillOpacity = 0.1f;
        QPointer<QSGTexture> texture; //< A weak pointer to the texture owned by the SG.

        bool dirty = true;

        bool operator==(const State& other) const;
        bool operator!=(const State& other) const;
    };

    class Shader: public QSGMaterialShader
    {
    public:
        Shader();

        bool updateUniformData(
            RenderState& state,
            QSGMaterial* newMaterial,
            QSGMaterial* oldMaterial) override;

        void updateSampledImage(
            QSGMaterialShader::RenderState& state,
            int binding,
            QSGTexture** texture,
            QSGMaterial* newMaterial,
            QSGMaterial* oldMaterial) override;
    };

    class Material: public QSGMaterial
    {
    public:
        Material();
        QSGMaterialType* type() const override;
        int compare(const QSGMaterial* other) const override;

        QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override
        {
            return new Shader();
        }

        State uniforms;
        std::unique_ptr<QSGTexture> texture;
    };

    struct LabelData
    {
        int sensitivity;
        QPoint position; //< in grid coords.
    };

private:
    QPointer<core::CameraMotionHelper> m_motionHelper;
    core::MotionGrid m_motionGrid;

    int m_channel = 0;
    int m_rotationQuadrants = 0;
    QVector<QColor> m_sensitivityColors;
    QColor m_labelsColor = core::colorTheme()->color("dark1");

    State m_currentState;
    qreal m_devicePixelRatio = 1.0;
    QSizeF m_cellSize;
    QSize m_labelSize;

    QVector<LabelData> m_labels;
    bool m_labelsDirty = true;

    bool m_labelsTextureDirty = true;
    bool m_regionsTextureDirty = true;
};

} // namespace nx::vms::client::desktop
