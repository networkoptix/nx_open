#include "ptz_controller.h"

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace mobile {

class PtzControllerPrivate: public QObject
{
    using base_type = QObject;

    Q_DECLARE_PUBLIC(PtzController)
    PtzController* const q_ptr;

public:
    PtzControllerPrivate(PtzController* controller);

    QString resourceId() const;
    void setResourceId(const QString& value);

private:
    void handleResourceChanged();

private:
    QString m_resourceId;
    QnPtzControllerPtr m_controller;
};

PtzControllerPrivate::PtzControllerPrivate(PtzController* controller):
    base_type(),
    q_ptr(controller)
{
    Q_Q(PtzController);
    connect(q, &PtzController::resourceIdChanged,
        this, &PtzControllerPrivate::handleResourceChanged);
}

QString PtzControllerPrivate::resourceId() const
{
    return m_resourceId;
}

void PtzControllerPrivate::setResourceId(const QString& value)
{
    if (value == m_resourceId)
        return;

    m_resourceId = value;

    Q_Q(PtzController);
    emit q->resourceIdChanged();
}

void PtzControllerPrivate::handleResourceChanged()
{
    const auto resource = qnResPool->getResourceByUniqueId(m_resourceId);
    const auto controller = qnClientPtzPool->controller(resource);
    if (controller == m_controller)
        return;

    m_controller = controller;
}

//-------------------------------------------------------------------------------------------------

PtzController::PtzController(QObject* parent):
    base_type(QnResourcePtr()),
    d_ptr(new PtzControllerPrivate(this))
{
    setParent(parent);
}

PtzController::~PtzController()
{
}

QString PtzController::resourceId() const
{
    Q_D(const PtzController);
    return d->resourceId();
}

void PtzController::setResourceId(const QString& value)
{
    Q_D(PtzController);
    d->setResourceId(value);
}

} // namespace mobile
} // namespace client
} // namespace nx

