#pragma once

#include <QtCore/QDebug>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <QtGui/QImage>

#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState: AbstractReduxState
{
    /** Recommended area of the layout background - in square cells. */
    static constexpr int kBackgroundRecommendedArea = 40 * 40;

    /** Recommended size of the layout background - in cells. */
    static constexpr int kBackgroundMinSize = 5;

    /** Recommended size of the layout background - in cells. */
    static constexpr int kBackgroundMaxSize = 64;

    bool locked = false;
    bool isLocalFile = false;
    float cellAspectRatio = 0.0;

    int logicalId = 0;
    int reservedLogicalId = 0;

    // Split values to keep old values when user toggles checkbox.
    bool fixedSizeEnabled = false;
    QSize fixedSize;

    struct Range
    {
        int min = kBackgroundMinSize;
        int value = 0;
        int max = kBackgroundMaxSize;

        void setRange(int min, int max)
        {
            NX_ASSERT(min <= max);
            this->min = min;
            this->max = max;
        }

        void setValue(int value)
        {
            this->value = qBound(min, value, max);
        }

        constexpr Range() = default;

        constexpr Range(int min, int value, int max):
            min(min),
            value(value),
            max(max)
        {
        }

        explicit constexpr Range(int value):
            value(value)
        {
        }

        constexpr bool isValid() const
        {
            return min <= value && value <= max;
        }
    };

    /** Possible states of the background image. */
    enum class BackgroundImageStatus
    {
        /** No image is selected. */
        empty,
        /** BackgroundImageState::error is occurred. */
        error,
        /** Current layout image is being downloaded. */
        downloading,
        /** Current layout image is downloaded and being loading into memory. */
        loading,
        /** Current layout image is loaded. */
        loaded,
        /** New image is selected and being loaded. */
        newImageLoading,
        /** New image is selected and loaded. */
        newImageLoaded,
        /** New image is selected being uploaded. */
        newImageUploading
    };

    struct Background
    {
        BackgroundImageStatus status = BackgroundImageStatus::empty;
        QString errorText;

        Range width;
        Range height;
        bool keepImageAspectRatio = false;
        int opacityPercent = 100;
        bool cropToMonitorAspectRatio = false;

        /** Filename of the image (GUID-generated) on the server storage or in local cache. */
        QString filename;

        /** Path to the selected image (may be path in cache). */
        QString imageSourcePath;

        /** Preview image. */
        QImage preview;

        /** Cropped preview image. */
        QImage croppedPreview;

        /** Image is present and image file is available locally. */
        bool imagePresent() const
        {
            switch (status)
            {
                case BackgroundImageStatus::loaded:
                case BackgroundImageStatus::newImageLoaded:
                    return true;
                default:
                    return false;
            }
        }

        bool loadingInProgress() const
        {
            switch (status)
            {
                case BackgroundImageStatus::downloading:
                case BackgroundImageStatus::loading:
                case BackgroundImageStatus::newImageLoading:
                case BackgroundImageStatus::newImageUploading:
                    return true;
                default:
                    return false;
            }
        }

        bool canChangeAspectRatio() const
        {
            return !croppedPreview.isNull();
        }

        /**
         *  Whether we can crop the image. The only case when we cannot - if we open already
         *  cropped and saved image.
         */
        bool canCropImage() const
        {
            const bool loadedImageIsCropped = (status == BackgroundImageStatus::loaded
                && croppedPreview.isNull());
            return !loadedImageIsCropped;
        }

        bool canStartDownloading() const
        {
            return status == BackgroundImageStatus::empty && !filename.isEmpty();
        }
    };

    Background background;
};

#define LayoutSettingsDialogState_Range_Fields (min)(max)(value)

QN_FUSION_DECLARE_FUNCTIONS(LayoutSettingsDialogState::Range,
    (debug)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

QN_FUSION_DECLARE_FUNCTIONS(LayoutSettingsDialogState::BackgroundImageStatus,
    (debug)(lexical),
    NX_VMS_CLIENT_DESKTOP_API)

#define LayoutSettingsDialogState_Background_Fields (status)(errorText)(width)(height)\
    (keepImageAspectRatio)(opacityPercent)(cropToMonitorAspectRatio)(filename)(imageSourcePath)\
    (preview)(croppedPreview)

QN_FUSION_DECLARE_FUNCTIONS(LayoutSettingsDialogState::Background,
    (debug)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

#define LayoutSettingsDialogState_Fields (locked)(isLocalFile)(cellAspectRatio)(logicalId)\
    (reservedLogicalId)(fixedSizeEnabled)(fixedSize)(background)

QN_FUSION_DECLARE_FUNCTIONS(LayoutSettingsDialogState,
    (debug)(eq),
    NX_VMS_CLIENT_DESKTOP_API)

} // namespace nx::vms::client::desktop
