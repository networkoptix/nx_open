// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

class CameraThumbnailManager;
class QnResourcePool;

namespace nx::vms::client::desktop {

class BusyIndicator;
class LayoutThumbnailLoader;

namespace ui {

class LayoutPreviewPainter: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutPreviewPainter(QObject* parent = nullptr);
    virtual ~LayoutPreviewPainter() override;

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    void paint(QPainter* painter, const QRect& paintRect);

private:
    void at_updateThumbnailStatus(core::ThumbnailStatus status);
    void at_updateThumbnailImage(const QImage& image);

private:
    QnLayoutResourcePtr m_layout;

    QScopedPointer<BusyIndicator> m_busyIndicator;

    QPixmap m_layoutThumbnail;

    // Status of resource loading.
    Qn::ResourceStatusOverlay m_overlayStatus = Qn::NoDataOverlay;

    std::unique_ptr<nx::vms::client::desktop::LayoutThumbnailLoader> m_layoutThumbnailProvider;
};

} // namespace ui
} // namespace nx::vms::client::desktop
