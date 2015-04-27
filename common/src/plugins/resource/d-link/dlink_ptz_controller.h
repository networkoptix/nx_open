#ifndef QN_DLINK_PTZ_CONTROLLER_H
#define QN_DLINK_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <QtCore/QScopedPointer>
#include <core/ptz/basic_ptz_controller.h>

class QnPlOnvifResource;
class QnDlinkPtzRepeatCommand;

class QnDlinkPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnDlinkPtzController(const QnPlOnvifResourcePtr &resource);
    virtual ~QnDlinkPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;

private:
    friend class QnDlinkPtzRepeatCommand;

    bool doQuery(const QString &request, QByteArray* body = 0) const;
    QString getLastCommand() const;
private:
    QSharedPointer<QnPlOnvifResource> m_resource;
    mutable QMutex m_mutex;
    QString m_lastRequest;
    QScopedPointer<QnDlinkPtzRepeatCommand> m_repeatCommand;
};

#endif // ENABLE_ONVIF
#endif // QN_DLINK_PTZ_CONTROLLER_H
