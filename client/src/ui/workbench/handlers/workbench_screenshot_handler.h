#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/image_provider.h>
#include <utils/color_space/image_correction.h>

class QPainter;
class QnResourceDisplay;

struct QnScreenshotParameters {
    qint64 time;    //in microseconds since epoch
    bool isUtc;
    QString filename;
    Qn::Corner timestampPosition;
    QnItemDewarpingParams itemDewarpingParams;
    QnMediaDewarpingParams mediaDewarpingParams;
    ImageCorrectionParams imageCorrectionParams;
    QRectF zoomRect;
    QString timeString() const;
};

class QnScreenshotLoader: public QnImageProvider {
    Q_OBJECT
public:
    QnScreenshotLoader(const QnScreenshotParameters& parameters, QObject *parent = 0);
    virtual ~QnScreenshotLoader();

    void setBaseProvider(QnImageProvider* imageProvider);

    virtual QImage image() const override;

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
class QnWorkbenchScreenshotHandler: public QObject, protected QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchScreenshotHandler(QObject *parent = NULL);

private:
    QnImageProvider* getLocalScreenshotProvider(QnScreenshotParameters &parameters, QnResourceDisplay* display);
private slots:
    void at_takeScreenshotAction_triggered();
    void at_imageLoaded(const QImage &image);
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
