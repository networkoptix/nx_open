#include "export_media_validator.h"

#include <recording/time_period.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>

// TODO: #GDM move constant to another place
#include <nx/client/desktop/ui/dialogs/rapid_review_dialog.h>

namespace nx {
namespace client {
namespace desktop {

namespace
{

/** Maximum sane duration: 30 minutes of recording. */
static const qint64 kMaxRecordingDurationMs = 1000 * 60 * 30;
static const qint64 kTimelapseBaseFrameStepMs = 1000 / ui::dialogs::ExportRapidReview::kResultFps;

bool validateLength(const QnTimePeriod& timePeriod, qint64 timelapseFrameStepMs = 0)
{
    const auto exportSpeed = qMax(1ll, timelapseFrameStepMs / kTimelapseBaseFrameStepMs);

    // TODO: #GDM implement more precise estimation
    const auto durationMs = timePeriod.durationMs;

    /* //TODO: #GDM Restore this logic #3.1.1 #high
        if (auto loader = context()->instance<QnCameraDataManager>()->loader(mediaResource))
        {
            QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
            if (!periods.isEmpty())
                durationMs = periods.duration();
            NX_ASSERT(durationMs > 0, "Intersected periods must not be empty or infinite");
        }
     */

    return durationMs / exportSpeed <= kMaxRecordingDurationMs;
}

} // namespace

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportMediaSettings& settings)
{
    Results results;

    if  (!validateLength(settings.timePeriod, settings.timelapseFrameStepMs))
        results.set(int(Result::tooLong));

    /*
    if (!FileExtensionUtils::isExecutable(settings.filename.extension))
    {
        const bool transcodingRequired =
            settings.contrastParams.enabled || dewarpingParams.enabled || itemData.rotation || customAr || !zoomRect.isNull();
        || (mediaResource->getVideoLayout() && mediaResource->getVideoLayout()->channelCount() > 1)
        || timestamp || other overlays
    }
    */

    if (settings.fileName.extension == FileExtension::avi)
    {

    }

    return results;
}

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportLayoutSettings& settings)
{
    if (FileExtensionUtils::isExecutable(settings.filename.extension))
    {

    }

    return Results();
}

} // namespace desktop
} // namespace client
} // namespace nx
