// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QVector>
#include <QtGui/QMatrix3x3>
#include <QtGui/QMatrix4x4>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>

#include <nx/media/ffmpeg/frame_info.h>
#include <nx/vms/api/data/dewarping_data.h>

#include "abstract_image_filter.h"

class NX_VMS_COMMON_API QnDewarpingImageFilter: public QnAbstractImageFilter
{
public:
    QnDewarpingImageFilter();

    QnDewarpingImageFilter(
        const nx::vms::api::dewarping::MediaData& mediaDewarping,
        const nx::vms::api::dewarping::ViewData& itemDewarping);

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

    void setParameters(
        const nx::vms::api::dewarping::MediaData& mediaDewarping,
        const nx::vms::api::dewarping::ViewData& itemDewarping);

    static quint8 getPixel(quint8* buffer, int stride, float x, float y, int width, int height);

protected: //< For unit tests.
    void updateDewarpingParameters(qreal aspectRatio);
    QPointF transformed(int x, int y, const QSize& imageSize) const;

private:
    void updateTransformMap(const QSize& imageSize, int plane, qreal aspectRatio);

    static QSize getOptimalSize(
        const QSize& srcResolution,
        const nx::vms::api::dewarping::MediaData& mediaDewarpingParams,
        const nx::vms::api::dewarping::ViewData& itemDewarpingParams);

    QVector2D cameraProject(const QVector3D& pointOnSphere) const;
    QVector3D viewUnproject(const QVector2D& viewCoords) const;

private:
    static const int MAX_COLOR_PLANES = 4;

    QMatrix3x3 m_viewTransform;
    QMatrix3x3 m_cameraImageTransform;
    QMatrix3x3 m_unprojectionTransform; //< Used only with rectilinear view projection.
    QMatrix3x3 m_sphereRotationTransform; //< Used only with equirectangular view projection.
    QMatrix4x4 m_horizonCorrectionTransform; //< Used only with 360VR camera projection(s).
    QSize m_lastImageSize;

    int m_lastImageFormat = -1;

protected: //< For unit tests.
    nx::vms::api::dewarping::MediaData m_mediaParams;
    nx::vms::api::dewarping::ViewData m_itemParams;

    using PointsVector = QVector<QPointF>;
    PointsVector m_transform[MAX_COLOR_PLANES];
};
