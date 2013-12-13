#ifndef QN_ASYNCHRONOUS_PTZ_CONTROLLER_H
#define QN_ASYNCHRONOUS_PTZ_CONTROLLER_H

#include "abstract_ptz_controller.h"

/**
 * An intermediate ptz controller that is not to be used directly. In changes
 * the semantics of default ptz controller in the following way:
 * <ul>
 * <li>All methods work asynchronously and return their results via <tt>finished</tt> signal.</tt>
 * <li>All getters ignore the output parameter.</li>
 * </ul>
 */
class QnAsynchronousPtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnAsynchronousPtzController(const QnResourcePtr &resource): base_type(resource) {}

    virtual bool getData(Qn::PtzDataFields, QnPtzData *) override { return true; }

signals:
    void finished(Qn::PtzCommand command, const QVariant &data);
};

#endif // QN_ASYNCHRONOUS_PTZ_CONTROLLER_H
