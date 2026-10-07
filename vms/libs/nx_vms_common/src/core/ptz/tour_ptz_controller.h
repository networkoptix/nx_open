// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/ptz/proxy_ptz_controller.h>
#include <nx/utils/lockable.h>

class QThreadPool;

template<class T>
class QnResourcePropertyAdaptor;
class QnTourPtzExecutor;

using QnPtzTourHash = QHash<QString, QnPtzTour>;

class NX_VMS_COMMON_API QnTourPtzController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    QnTourPtzController(const QnPtzControllerPtr& baseController,
        QThreadPool* threadPool,
        QThread* executorThread);
    virtual ~QnTourPtzController() override;

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(const Vector& speed, const Options& options) override;

    virtual bool absoluteMove(CoordinateSpace space,
        const Vector& position,
        qreal speed,
        const Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio, const QRectF& viewport, qreal speed, const Options& options) override;

    virtual bool activatePreset(const QString& presetId, qreal speed) override;

    virtual bool createTour(const QnPtzTour& tour) override;
    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;
    virtual std::optional<QnPtzTour> getActiveTour() override;
    virtual bool getTours(QnPtzTourList* tours) const override;

private:
    void stopActiveTour();

private:
    QnResourcePropertyAdaptor<QnPtzTourHash>* m_adaptor = nullptr;
    // Also serializes the adaptor's read-modify-write operations with active-tour changes.
    nx::Lockable<QnPtzTour> m_activeTour;
    QScopedPointer<QnTourPtzExecutor, QScopedPointerDeleteLater> m_executor;
};
