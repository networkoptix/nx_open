#pragma once

#include <client/client_globals_fwd.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

class QLabel;
class QStackedWidget;
class QnBusyIndicatorWidget;
class QnAutoscaledPlainText;
class QnImageProvider;

class QnResourcePreviewWidget: public Connective<QWidget>
{
    Q_OBJECT
    typedef Connective<QWidget> base_type;

public:
    explicit QnResourcePreviewWidget(QWidget* parent = nullptr);
    virtual ~QnResourcePreviewWidget();

    QnImageProvider* imageProvider() const;
    void setImageProvider(QnImageProvider* provider);

    QnBusyIndicatorWidget* busyIndicator() const;

    virtual QSize sizeHint() const override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

    QPalette::ColorRole borderRole() const;
    void setBorderRole(QPalette::ColorRole role);

    QRectF highlightRect() const;
    void setHighlightRect(const QRectF& relativeRect);

    enum class CropMode //< When preview is cropped by highlight rectangle.
    {
        never,
        always,
        hovered,
        notHovered,
    };

    CropMode cropMode() const;
    void setCropMode(CropMode value);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void invalidateGeometry();

    void updateThumbnailStatus(Qn::ThumbnailStatus status);
    void updateThumbnailImage(const QImage& image);

    bool cropRequired() const;

private:
    mutable QSize m_cachedSizeHint;
    QnAutoscaledPlainText* const m_placeholder = nullptr;
    QnBusyIndicatorWidget* const m_indicator = nullptr;
    QPixmap m_preview;
    QPointer<QnImageProvider> m_imageProvider;
    QPalette::ColorRole m_borderRole = QPalette::Shadow;
    QRectF m_highlightRect;
    CropMode m_cropMode = CropMode::never;
};
