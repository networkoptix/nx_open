#pragma once

#include <QtCore/QScopedPointer>

#include <QtGui/QPainter>

#include <core/resource/resource_fwd.h>

class QnCameraThumbnailManager;
class QnBusyIndicator;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutPreviewPainter: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor)

    using base_type = QObject;

public:
    LayoutPreviewPainter(QnCameraThumbnailManager* thumbnailManager, QObject* parent = nullptr);
    virtual ~LayoutPreviewPainter() override;

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QColor frameColor() const;
    void setFrameColor(const QColor& value);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& value);

    QColor fontColor() const;
    void setFontColor(const QColor& value);

    void paint(QPainter* painter, const QRect& paintRect);

private:
    struct ThumbnailInfo
    {
        ThumbnailInfo();
        ThumbnailInfo(const QPixmap& pixmap);
        ThumbnailInfo(Qn::ThumbnailStatus status, bool ignoreTrasformation, const QPixmap& pixmap);

        Qn::ThumbnailStatus status;
        bool ignoreTrasformation;
        QPixmap pixmap;
    };

    ThumbnailInfo thumbnailForItem(const QnLayoutItemData& item) const;
    void paintItem(QPainter* painter, const QRectF& itemRect, const QnLayoutItemData& item);

private:
    QnLayoutResourcePtr m_layout;

    //TODO: #GDM #3.1 singletons are not safe in such cases
    QnCameraThumbnailManager* m_thumbnailManager;

    QColor m_frameColor;
    QColor m_backgroundColor;
    QColor m_fontColor;
    QScopedPointer<QnBusyIndicator> m_busyIndicator;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
