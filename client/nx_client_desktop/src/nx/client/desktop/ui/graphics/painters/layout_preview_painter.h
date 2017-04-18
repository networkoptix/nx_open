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
    using base_type = QObject;
public:
    LayoutPreviewPainter(QnCameraThumbnailManager* thumbnailManager, QObject* parent = nullptr);
    virtual ~LayoutPreviewPainter() override;

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    void paint(QPainter* painter, const QRect& paintRect);

private:
    bool paintItem(QPainter* painter, const QRectF& itemRect, const QnLayoutItemData& item);

private:
    QnLayoutResourcePtr m_layout;

    //TODO: #GDM #3.1 singletons are not safe in such cases
    QnCameraThumbnailManager* m_thumbnailManager;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
