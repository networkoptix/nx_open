#pragma once

#include <QtCore/QScopedPointer>

#include <QtGui/QPainter>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <ui/customization/customized.h>
#include <client/client_globals.h>

class CameraThumbnailManager;
class QnResourcePool;

namespace nx::vms::client::desktop {

class BusyIndicator;
class LayoutThumbnailLoader;

namespace ui {

class LayoutPreviewPainter: public Customized<QObject>
{
    Q_OBJECT
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QColor itemBackgroundColor READ itemBackgroundColor WRITE setItemBackgroundColor)
    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor)

    using base_type = Customized<QObject>;

public:
    LayoutPreviewPainter(QnResourcePool* resourcePool, QObject* parent = nullptr);
    virtual ~LayoutPreviewPainter() override;

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QColor frameColor() const;
    void setFrameColor(const QColor& value);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& value);

    QColor itemBackgroundColor() const;
    void setItemBackgroundColor(const QColor& value);

    QColor fontColor() const;
    void setFontColor(const QColor& value);

    void paint(QPainter* painter, const QRect& paintRect);

private:
    void at_updateThumbnailStatus(Qn::ThumbnailStatus status);
    void at_updateThumbnailImage(const QImage& image);

private:
    QnLayoutResourcePtr m_layout;

    QColor m_frameColor;
    QColor m_backgroundColor;
    QColor m_itemBackgroundColor;
    QColor m_fontColor;
    QScopedPointer<BusyIndicator> m_busyIndicator;

    QPixmap m_layoutThumbnail;

    // We need resource pool to pass it to m_layoutThumbnailProvider.
    QPointer<QnResourcePool> m_resourcePool;
    // Status of resource loading.
    Qn::ResourceStatusOverlay m_overlayStatus = Qn::NoDataOverlay;

    std::unique_ptr<nx::vms::client::desktop::LayoutThumbnailLoader> m_layoutThumbnailProvider;
};

} // namespace ui
} // namespace nx::vms::client::desktop
