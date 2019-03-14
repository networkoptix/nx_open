#ifndef QN_VISTA_FOCUS_PTZ_CONTROLLER_H
#define QN_VISTA_FOCUS_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <core/ptz/proxy_ptz_controller.h>
#include <nx/vms/server/resource/resource_fwd.h>

class CLSimpleHTTPClient;
class QnIniSection;

/**
 * Controller for vista-specific PTZ focus functions.
 */
class QnVistaFocusPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnVistaFocusPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnVistaFocusPtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousFocus(qreal speed, const nx::core::ptz::Options& options) override;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList *auxiliaryTraits,
        const nx::core::ptz::Options& options) const;

    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait &trait,
        const QString &data,
        const nx::core::ptz::Options& options);

private:
    void init();

    bool queryLocked(const QString &request, QByteArray *body = NULL);
    bool queryLocked(const QString &request, QnIniSection *section, QByteArray *body = NULL);

    void ensureClientLocked();

private:
    QnVistaResourcePtr m_resource;
    Ptz::Capabilities m_capabilities;
    QnPtzAuxiliaryTraitList m_traits;
    bool m_isMotor;

    QnMutex m_mutex;
    QString m_lastHostAddress;
    QScopedPointer<CLSimpleHTTPClient> m_client;
};

#endif // ENABLE_ONVIF

#endif // QN_VISTA_FOCUS_PTZ_CONTROLLER_H
