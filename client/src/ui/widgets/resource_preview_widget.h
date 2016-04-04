#pragma once

#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

class QnCameraThumbnailManager;

class QnResourcePreviewWidget : public Connective<QWidget>
{
    Q_OBJECT
    typedef Connective<QWidget> base_type;

public:
    QnResourcePreviewWidget(QWidget* parent = nullptr);
    virtual ~QnResourcePreviewWidget();

    QnUuid targetResource() const;
    void setTargetResource(const QnUuid &target);

    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize &size);

protected:
    virtual void paintEvent(QPaintEvent *event) override;
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

private:
    QRect pixmapRect() const;

private:
    QScopedPointer<QnCameraThumbnailManager> m_thumbnailManager;
    QnUuid m_target;
    QPixmap m_pixmap;
};
