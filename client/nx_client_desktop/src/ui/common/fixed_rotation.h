#ifndef QN_FIXED_ROTATION_H
#define QN_FIXED_ROTATION_H

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsRotation>

class QGraphicsWidget;

namespace Qn {
    enum FixedRotation {
        Angle0 = 0,
        Angle90 = 90,
        Angle180 = 180,
        Angle270 = 270
    };
}

Qn::FixedRotation fixedRotationFromDegrees(qreal degrees);


class QnFixedRotationTransform: public QGraphicsRotation {
    Q_OBJECT;

    typedef QGraphicsRotation base_type;

public:
    QnFixedRotationTransform(QObject *parent = NULL);

    using base_type::setAngle;
    void setAngle(Qn::FixedRotation angle);
    
    QGraphicsWidget *target();
    void setTarget(QGraphicsWidget *target);

private slots:
    void updateOrigin();

private:
    QPointer<QGraphicsWidget> m_target;
};


#endif // QN_FIXED_ROTATION_H
