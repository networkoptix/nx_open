#pragma once

#include <QtWidgets/QWidget>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QLabel;
class QStackedWidget;

namespace nx::vms::client::desktop {

class BusyIndicatorWidget;
class AutoscaledPlainText;
class ImageProvider;

/**
 * The widget shows image that it receives from ImageProvider.
 * Busy indicator is shown until the image is loaded.
 */
class AsyncImageWidget : public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

public:
    explicit AsyncImageWidget(QWidget* parent = nullptr);
    virtual ~AsyncImageWidget() override;

    ImageProvider* imageProvider() const;
    void setImageProvider(ImageProvider* provider);

    BusyIndicatorWidget* busyIndicator() const;

    /**
     * @returns:
     *  - Take imageSize from imageProvider if it exists and not empty;
     *  otherwise take minimumSize() if not empty; otherwise take kDefaultThumbnailSize=(1920, 1080).
     *  - If autoScaleDown, scale it down respecting aspect ratio to fit into maximumSize() .
     *
     *  @note always returns non-empty rectangle.
     */
    virtual QSize sizeHint() const override;

    virtual bool hasHeightForWidth() const override;

    /**
     * @returns:
     *   - sizeHint().height() if autoScaleDown == false && autoScaleUp == false
     *   - min(width / (sizeHint() aspect ratio), sizeHint().height()) if autoScaleDown == true && autoScaleUp == false
     *   - max(width / (sizeHint() aspect ratio), sizeHint().height()) if autoScaleDown == false && autoScaleUp == true
     *   - width / (sizeHint() aspect ratio) if autoScaleDown = true && autoScaleUp == true
     */
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

    // Enable automatic downscaling of the image if it does not fit to widget's bounds.
    // Will not cause existing image to be resized
    void setAutoScaleDown(bool value);
    bool autoScaleDown() const;

    // Enable automatic upscaling of the image up to widget's size
    // Will not cause existing image to be resized
    void setAutoScaleUp(bool value);
    bool autoScaleUp() const;

    enum class ReloadMode
    {
        showLoadingIndicator,
        showPreviousImage
    };

    ReloadMode reloadMode() const;
    void setReloadMode(ReloadMode value);

    // If set to true, widget shows "NO DATA" until reset to false.
    void setNoDataMode(bool noData);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void invalidateGeometry();

    void updateSizeHint() const; //< Const because sizeHint() has to be const.
    // Sets both sizeHint() and m_preview image based on current state.
    void updateCache();

    void updateThumbnailStatus(Qn::ThumbnailStatus status);

    bool cropRequired() const;

    static QTransform getAdjustment(QRectF target, QRectF image);

private:
    mutable QSize m_cachedSizeHint; //< Mutable because sizeHint() has to be const.
    AutoscaledPlainText* const m_placeholder = nullptr;
    BusyIndicatorWidget* const m_indicator = nullptr;
    QPixmap m_preview;
    QPointer<ImageProvider> m_imageProvider;
    QPalette::ColorRole m_borderRole = QPalette::Shadow;
    QRectF m_highlightRect;
    CropMode m_cropMode = CropMode::never;
    // Should widget scale image to fit to the size of the widget
    bool m_autoScaleDown = true;
    // Should the widget enlarge image up to size of the widget
    bool m_autoScaleUp = false;
    ReloadMode m_reloadMode = ReloadMode::showLoadingIndicator;
    Qn::ThumbnailStatus m_previousStatus = Qn::ThumbnailStatus::Invalid;
    // Show "NO DATA" no matter what.
    bool m_noDataMode = false;
};

} // namespace nx::vms::client::desktop
