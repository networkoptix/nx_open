#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#ifdef ENABLE_AXIS

#include <QtCore/QHash>
#include <QElapsedTimer>

#include <core/ptz/basic_ptz_controller.h>
#include <utils/math/functors.h>
#include <nx/vms/server/resource/resource_fwd.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnAxisPtzController(const QnPlAxisResourcePtr &resource);
    virtual ~QnAxisPtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speedVector,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& relativeMovementVector,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(
        qreal relativeFocusMovement,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* position,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits *limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations *flip,
        const nx::core::ptz::Options& options) const override;

    virtual bool getPresets(QnPtzPresetList *presets) const override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;

private:
    void updateState();
    void updateState(const QnAxisParameterMap &params);

    CLSimpleHTTPClient *newHttpClient() const;
    bool queryInternal(const QString &request, QByteArray *body = NULL);
    bool query(const QString &request, QByteArray *body = NULL) const;
    bool query(const QString &request, QnAxisParameterMap *params, QByteArray *body = NULL) const;
    bool query(const QString &request, int restries, QnAxisParameterMap *params, QByteArray *body = NULL) const;
    int channel() const;

    QByteArray getCookie() const;
    void setCookie(const QByteArray &cookie);

private:
    QnPlAxisResourcePtr m_resource;
    Ptz::Capabilities m_capabilities;
    Qt::Orientations m_flip;
    QnPtzLimits m_limits;
    QnPtzLimits m_rawLimits;
    QnLinearFunction m_35mmEquivToCameraZoom, m_cameraTo35mmEquivZoom;
    QVector3D m_maxDeviceSpeed;

    mutable QnMutex m_mutex;
    QByteArray m_cookie;

    mutable QMap<QString, QString> m_cachedData;
    mutable QElapsedTimer m_cacheUpdateTimer;
};

#endif // #ifdef ENABLE_AXIS
#endif // QN_AXIS_PTZ_CONTROLLER_H
