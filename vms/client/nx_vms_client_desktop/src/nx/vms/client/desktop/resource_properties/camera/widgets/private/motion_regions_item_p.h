#pragma once

#include "../motion_regions_item.h"

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>

#include <QtGui/QImage>

#include <QtQuick/QSGSimpleMaterialShader>

#include <nx/client/core/motion/motion_grid.h>

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
    void invalidateRegionsTexture();
    void ensureRegionsTexture();
    void ensureLabelsTexture();
    void updateRegionsImage();
    void updateLabelPositions();
    void updateLabelsNode(QSGNode* mainNode, bool geometryDirty);

private:
    struct State
    {
        QSize resolution;
        QColor borderColor = Qt::black;
        qreal fillOpacity = 0.1;
        QSharedPointer<QSGTexture> texture;

        bool operator==(const State& other) const;
        bool operator!=(const State& other) const;
    };

    class Shader: public QSGSimpleMaterialShader<State>
    {
        QSG_DECLARE_SIMPLE_SHADER(Shader, State)

    public:
        virtual const char* vertexShader() const override;
        virtual const char* fragmentShader() const override;

        virtual QList<QByteArray> attributes() const override;
        virtual void updateState(const State* newState, const State* oldState) override;
        virtual void resolveUniforms() override;
    };

    struct LabelData
    {
        int sensitivity;
        QPoint position; //< in grid coords.
    };

private:
    QPointer<core::CameraMotionHelper> m_motionHelper;
    int m_channel = 0;
    QVector<QColor> m_sensitivityColors;

    State m_currentState;

    QVector<LabelData> m_labels;
    QScopedPointer<QSGTexture, QScopedPointerDeleteLater> m_labelsTexture;
    QSizeF m_cellSize;
    QSize m_labelSize;
    QColor m_labelsColor = Qt::black;
    bool m_labelsTextureDirty = true;
    bool m_labelsDirty = true;

    core::MotionGrid m_motionGrid;
    QImage m_regionsImage;
};

} // namespace nx::vms::client::desktop
