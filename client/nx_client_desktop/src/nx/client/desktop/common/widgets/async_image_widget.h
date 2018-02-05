#pragma once

#include <client/client_globals_fwd.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

class QLabel;
class QStackedWidget;
class QnBusyIndicatorWidget;
class QnAutoscaledPlainText;
class QnImageProvider;


namespace nx {
namespace client {
namespace desktop {

/**
 * The widget shows image from QnImageProvider
 */
class AsyncImageWidget : public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

public:
    explicit AsyncImageWidget(QWidget* parent = nullptr);
    virtual ~AsyncImageWidget();

    QnImageProvider* imageProvider() const;
    void setImageProvider(QnImageProvider* provider);

    QnBusyIndicatorWidget* busyIndicator() const;

    virtual QSize sizeHint() const override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

    QPalette::ColorRole borderRole() const;
    void setBorderRole(QPalette::ColorRole role);

    // Enable automatic downscaling of the image if it does not fit to widget's bounds.
    // Will not cause existing image to be resized
    void setAutoScaleDown(bool value);
    bool autoScaleDown() const;

    // Enable automatic upscaling of the image up to widget's size
    // Will not cause existing image to be resized
    void setAutoScaleUp(bool value);
    bool autoScaleUp() const;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void invalidateGeometry();

    void updateThumbnailStatus(Qn::ThumbnailStatus status);
    void updateThumbnailImage(const QImage& image);

private:
    mutable QSize m_cachedSizeHint;
    QnAutoscaledPlainText* const m_placeholder = nullptr;
    QnBusyIndicatorWidget* const m_indicator = nullptr;
    QPixmap m_preview;
    QPointer<QnImageProvider> m_imageProvider;
    QPalette::ColorRole m_borderRole = QPalette::Shadow;
    // Should widget scale image to fit to the size of the widget
    bool m_autoScaleDown = true;
    // Should the widget enlarge image up to size of the widget
    bool m_autoScaleUp = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
