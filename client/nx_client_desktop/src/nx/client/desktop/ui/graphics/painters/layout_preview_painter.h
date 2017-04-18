#pragma once

#include <QtGui/QPainter>

#include <core/resource/resource_fwd.h>

class QnCameraThumbnailManager;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutPreviewPainter: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)

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

    void paint(QPainter* painter, const QRect& paintRect);

private:
    bool paintItem(QPainter* painter, const QRectF& itemRect, const QnLayoutItemData& item);

private:
    QnLayoutResourcePtr m_layout;

    //TODO: #GDM #3.1 singletons are not safe in such cases
    QnCameraThumbnailManager* m_thumbnailManager;

    QColor m_frameColor;
    QColor m_backgroundColor;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
