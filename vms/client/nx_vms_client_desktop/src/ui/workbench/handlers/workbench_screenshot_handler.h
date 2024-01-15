// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/transcoding/filters/timestamp_params.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/client/desktop/image_providers/image_provider.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/aspect_ratio.h>

class QPainter;
class QnMediaResourceWidget;

namespace nx::vms::client::desktop { class ProgressDialog; }

struct SharedScreenshotParameters
{
    QString filename;
    nx::core::transcoding::FilterParams cameraNameParams;
    nx::core::transcoding::TimestampParams timestampParams;
    bool operator==(const SharedScreenshotParameters&) const = default;
    bool isInitialized() const;
};
NX_REFLECTION_INSTRUMENT(
    SharedScreenshotParameters, (filename)(cameraNameParams)(timestampParams))

struct QnScreenshotParameters
{
    QnScreenshotParameters();

    QnMediaResourcePtr resource;

    /** UTC time of the screenshot, can also contain nx::api::ImageRequest::kLatestThumbnail. */
    std::chrono::milliseconds utcTimestamp;

    /** Time zone which should be used to paint timestamp. */
    QTimeZone timeZone{QTimeZone::LocalTime};

    /** Whether original media has timestamps in UTC. */
    bool isUtc = false;

    SharedScreenshotParameters sharedParameters;

    nx::vms::api::ImageCorrectionData imageCorrectionParams;
    nx::vms::api::dewarping::ViewData itemDewarpingParams;
    QRectF zoomRect;
    QnAspectRatio customAspectRatio;
    qreal rotationAngle = 0;

    QString timeString(bool forFilename = false) const;
};

// TODO: #vkutin Use nx::vms::client::desktop::ProxyImageProvider instead.
/* Proxy class, that starts loading instantly after base provider is set and notifies only once. */
class QnScreenshotLoader: public nx::vms::client::desktop::ImageProvider {
    Q_OBJECT
public:
    QnScreenshotLoader(const QnScreenshotParameters& parameters, QObject *parent = 0);
    virtual ~QnScreenshotLoader();

    void setBaseProvider(nx::vms::client::desktop::ImageProvider* imageProvider);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnScreenshotParameters parameters() const;
    void setParameters(const QnScreenshotParameters &parameters);

protected:
    virtual void doLoadAsync() override;

private slots:
    void at_imageLoaded(const QImage &image);

private:
    QScopedPointer<nx::vms::client::desktop::ImageProvider> m_baseProvider;
    QnScreenshotParameters m_parameters;
    bool m_isReady;
};

/**
 * @brief The QnWorkbenchScreenshotHandler class            Handler for the screenshots related actions.
 */
class QnWorkbenchScreenshotHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchScreenshotHandler(QObject *parent = nullptr);

private:
    nx::vms::client::desktop::ImageProvider* getLocalScreenshotProvider(QnMediaResourceWidget *widget,
        const QnScreenshotParameters &parameters, bool forced = false) const;

private slots:
    void at_takeScreenshotAction_triggered();
    void at_imageLoaded(const QImage &image);

    void showProgressDelayed(const QString &message);
    void showProgress();
    void hideProgressDelayed();
    void hideProgress();
    void cancelLoading();

private:
    bool updateParametersFromDialog(QnScreenshotParameters &parameters);
    void takeDebugScreenshotsSet(QnMediaResourceWidget *widget);
    std::vector<QnCustomFileDialog::FileFilter> generateFileFilters();

    void takeScreenshot(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters);

private:
    nx::vms::client::desktop::ProgressDialog* m_screenshotProgressDialog = nullptr;
    quint64 m_progressShowTime = 0;
    bool m_canceled = false;
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
