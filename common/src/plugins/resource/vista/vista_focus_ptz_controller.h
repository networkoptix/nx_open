#ifndef QN_VISTA_FOCUS_PTZ_CONTROLLER_H
#define QN_VISTA_FOCUS_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <core/ptz/proxy_ptz_controller.h>

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

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousFocus(qreal speed) override;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits);
    virtual bool runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data);

private:
    void init();

    bool queryLocked(const QString &request, QByteArray *body = NULL);
    bool queryLocked(const QString &request, QnIniSection *section, QByteArray *body = NULL);

    void ensureClientLocked();

private:
    QnVistaResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    QnPtzAuxilaryTraitList m_traits;
    bool m_isMotor;

    QMutex m_mutex;
    QString m_lastHostAddress;
    QScopedPointer<CLSimpleHTTPClient> m_client;
};

#endif // ENABLE_ONVIF

#endif // QN_VISTA_FOCUS_PTZ_CONTROLLER_H
