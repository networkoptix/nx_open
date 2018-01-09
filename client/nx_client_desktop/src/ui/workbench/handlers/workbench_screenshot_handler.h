#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/workbench/workbench_context_aware.h>

#include <transcoding/timestamp_params.h>

#include <nx/client/desktop/image_providers/image_provider.h>
#include <utils/color_space/image_correction.h>
#include <utils/common/connective.h>

class QPainter;
class QnProgressDialog;
class QnMediaResourceWidget;

struct QnScreenshotParameters
{
    QnScreenshotParameters();

    QnMediaResourcePtr resource;
    qint64 utcTimestampMsec = 0;
    bool isUtc = false;
    qint64 displayTimeMsec = 0;
    QString filename;
    QnTimeStampParams timestampParams;
    QnItemDewarpingParams itemDewarpingParams;
    ImageCorrectionParams imageCorrectionParams;
    QRectF zoomRect;
    qreal customAspectRatio = 0;
    qreal rotationAngle = 0;

    QString timeString() const;
};

/* Proxy class, that starts loading instantly after base provider is set and notifies only once. */
// TODO: #vkutin #gdm Use nx::client::desktop::ProxyImageProvider instead
class QnScreenshotLoader: public QnImageProvider {
    Q_OBJECT
public:
    QnScreenshotLoader(const QnScreenshotParameters& parameters, QObject *parent = 0);
    virtual ~QnScreenshotLoader();

    void setBaseProvider(QnImageProvider* imageProvider);

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
    QScopedPointer<QnImageProvider> m_baseProvider;
    QnScreenshotParameters m_parameters;
    bool m_isReady;
};

/**
 * @brief The QnWorkbenchScreenshotHandler class            Handler for the screenshots related actions.
 */
class QnWorkbenchScreenshotHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchScreenshotHandler(QObject *parent = NULL);

private:
    QnImageProvider* getLocalScreenshotProvider(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters, bool forced = false) const;

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

    qint64 screenshotTimeMSec(QnMediaResourceWidget *widget, bool adjust = false);
    void takeScreenshot(QnMediaResourceWidget *widget, const QnScreenshotParameters &parameters);

private:
    QnProgressDialog *m_screenshotProgressDialog;
    quint64 m_progressShowTime;
    bool m_canceled;
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
