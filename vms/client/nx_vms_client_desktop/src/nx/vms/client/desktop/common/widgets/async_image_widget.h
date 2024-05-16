// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtGui/QTransform>
#include <QtWidgets/QWidget>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>

class QLabel;
class QStackedWidget;
class QTimer;

namespace nx::vms::client::core { class ImageProvider; }

namespace nx::vms::client::desktop {

class BusyIndicatorWidget;
class AutoscaledPlainText;

/**
 * The widget shows image that it receives from ImageProvider.
 * Busy indicator is shown until the image is loaded.
 */
class AsyncImageWidget : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit AsyncImageWidget(QWidget* parent = nullptr);
    virtual ~AsyncImageWidget() override;

    core::ImageProvider* imageProvider() const;
    void setImageProvider(core::ImageProvider* provider);

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
        showPreviousImage,
        showBlurredPreviousImage,
    };

    ReloadMode reloadMode() const;
    void setReloadMode(ReloadMode value);

    // Shows custom text placeholder until reset to empty string.
    void setPlaceholder(const QString& text);
    const QString& placeholder() const;

    void setForceLoadingIndicator(bool force);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void invalidateGeometry();

    void updateSizeHint() const; //< Const because sizeHint() has to be const.
    // Sets both sizeHint() and m_preview image based on current state.
    void updateCache();

    void updateThumbnailStatus(core::ThumbnailStatus status);

    void setLoadingIndicationVisible(bool visible);

    bool cropRequired() const;

    static QTransform getAdjustment(QRectF target, QRectF image);

private:
    mutable QSize m_cachedSizeHint; //< Mutable because sizeHint() has to be const.
    AutoscaledPlainText* const m_placeholder = nullptr;
    BusyIndicatorWidget* const m_indicator = nullptr;
    QPixmap m_preview;
    QImage m_previewImage;
    QPointer<core::ImageProvider> m_imageProvider;
    QPalette::ColorRole m_borderRole = QPalette::Shadow;
    QRectF m_highlightRect;
    CropMode m_cropMode = CropMode::never;
    // Should widget scale image to fit to the size of the widget
    bool m_autoScaleDown = true;
    // Should the widget enlarge image up to size of the widget
    bool m_autoScaleUp = false;
    ReloadMode m_reloadMode = ReloadMode::showLoadingIndicator;
    core::ThumbnailStatus m_previousStatus = core::ThumbnailStatus::Invalid;
    // Show placeholder if not empty.
    QString m_placeholderText;
    bool m_applyBlurEffect = false;
    QColor m_placeholderBackgroundColor;
    // Force no crop and no highlight for frames potentially pre-cropped by analytics plugin.
    bool m_forceNoCrop = false;
    bool m_forceLoadingIndicator = false;
};

} // namespace nx::vms::client::desktop
