// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>

namespace nx::vms::client::desktop {

class LiveCameraThumbnail;

class CameraHotspotsEditorWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraHotspotsEditorWidget(QWidget* parent = nullptr);
    virtual ~CameraHotspotsEditorWidget() override;

    void setThumbnail(const QSharedPointer<LiveCameraThumbnail>& cameraThumbnail);

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

    nx::vms::common::CameraHotspotDataList getHotspots() const;
    void setHotspots(const nx::vms::common::CameraHotspotDataList& hotspots);

    int hotspotsCount() const;

    nx::vms::common::CameraHotspotData hotspotAt(int index);
    void setHotspotAt(const nx::vms::common::CameraHotspotData& hotspotData, int index);

    int appendHotspot(const nx::vms::common::CameraHotspotData& hotspotData);
    void removeHotstpotAt(int index);

    std::optional<int> selectedHotspotIndex() const;
    void setSelectedHotspotIndex(std::optional<int> index);

signals:
    void selectedHotspotChanged();
    void hotspotsDataChanged();
    void selectHotspotTarget(int hotspotIndex);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
