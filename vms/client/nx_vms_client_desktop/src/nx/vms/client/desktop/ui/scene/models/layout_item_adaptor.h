// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRect>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")

namespace nx::vms::client::desktop {

class LayoutModel;

class LayoutItemAdaptor: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnLayoutResource* layout READ layoutPlainPointer CONSTANT)
    Q_PROPERTY(QnUuid itemId READ itemId CONSTANT)
    Q_PROPERTY(QnUuid resourceId READ resourceId CONSTANT)
    Q_PROPERTY(QnResource* resource READ resourcePlainPointer CONSTANT)
    Q_PROPERTY(int flags READ flags WRITE setFlags NOTIFY flagsChanged)
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(QnUuid zoomTargetId
        READ zoomTargetId WRITE setZoomTargetId NOTIFY zoomTargetIdChanged)
    Q_PROPERTY(QRectF zoomRect READ zoomRect WRITE setZoomRect NOTIFY zoomRectChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool displayInfo READ displayInfo WRITE setDisplayInfo NOTIFY displayInfoChanged)
    Q_PROPERTY(bool controlPtz READ controlPtz WRITE setControlPtz NOTIFY controlPtzChanged)
    Q_PROPERTY(bool displayAnalyticsObjects
        READ displayAnalyticsObjects
        WRITE setDisplayAnalyticsObjects
        NOTIFY displayAnalyticsObjectsChanged)
    Q_PROPERTY(bool displayRoi READ displayRoi WRITE setDisplayRoi NOTIFY displayRoiChanged)

public:
    LayoutItemAdaptor(const QnLayoutResourcePtr& layout, const QnUuid& itemId);
    virtual ~LayoutItemAdaptor() override;

    QnLayoutResourcePtr layout() const;
    QnLayoutResource* layoutPlainPointer() const;

    QnUuid itemId() const;

    QnUuid resourceId() const;
    QnResourcePtr resource() const;
    QnResource* resourcePlainPointer() const;

    int flags() const;
    void setFlags(int flags);

    QRect geometry() const;
    void setGeometry(const QRect& geometry);

    QnUuid zoomTargetId() const;
    void setZoomTargetId(const QnUuid& id);

    QRectF zoomRect() const;
    void setZoomRect(const QRectF& zoomRect);

    qreal rotation() const;
    void setRotation(qreal rotation);

    bool displayInfo() const;
    void setDisplayInfo(bool displayInfo);

    bool controlPtz() const;
    void setControlPtz(bool controlPtz);

    bool displayAnalyticsObjects() const;
    void setDisplayAnalyticsObjects(bool displayAnalyticsObjects);

    bool displayRoi() const;
    void setDisplayRoi(bool displayRoi);

    nx::vms::api::ImageCorrectionData imageCorrectionParams() const;
    void setImageCorrectionParams(const nx::vms::api::ImageCorrectionData& params);

    nx::vms::api::dewarping::ViewData dewarpingParams() const;
    void setDewarpingParams(const nx::vms::api::dewarping::ViewData& params);

signals:
    void flagsChanged();
    void geometryChanged();
    void zoomTargetIdChanged();
    void zoomRectChanged();
    void rotationChanged();
    void displayInfoChanged();
    void controlPtzChanged();
    void displayAnalyticsObjectsChanged();
    void displayRoiChanged();
    void imageCorrectionParamsChanged();
    void dewarpingParamsChanged();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
