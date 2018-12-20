#pragma once

#include <core/ptz/proxy_ptz_controller.h>

#include <nx/utils/thread/mutex.h>

class QThreadPool;

template<class T>
class QnResourcePropertyAdaptor;
class QnTourPtzExecutor;

using QnPtzTourHash = QHash<QString, QnPtzTour>;

class QnTourPtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    QnTourPtzController(
        const QnPtzControllerPtr &baseController,
        QThreadPool* threadPool,
        QThread* executorThread);
    virtual ~QnTourPtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool activatePreset(const QString& presetId, qreal speed) override;

    virtual bool createTour(const QnPtzTour& tour) override;
    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;
    virtual bool getTours(QnPtzTourList* tours) const override;

private:
    void clearActiveTour();

private:
    QnMutex m_mutex;
    QnResourcePropertyAdaptor<QnPtzTourHash>* m_adaptor;
    QnPtzTour m_activeTour;
    QnTourPtzExecutor* m_executor;
};

