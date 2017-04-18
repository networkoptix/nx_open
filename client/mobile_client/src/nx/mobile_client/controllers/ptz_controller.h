#pragma once

#include <core/ptz/basic_ptz_controller.h> // TODO: replace with abstract controller

namespace nx {
namespace client {
namespace mobile {

class PtzControllerPrivate;
class PtzController: public QnBasicPtzController // TODO: replace with abstract controller
{
    Q_OBJECT
    using base_type = QnBasicPtzController;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)

public:
    PtzController(QObject* parent = nullptr);

    virtual ~PtzController();

    QString resourceId() const;
    void setResourceId(const QString& value);

signals:
    void resourceIdChanged();

private:
    Q_DECLARE_PRIVATE(PtzController)
    const QScopedPointer<PtzControllerPrivate> d_ptr;
};

} // namespace mobile
} // namespace client
} // namespace nx
