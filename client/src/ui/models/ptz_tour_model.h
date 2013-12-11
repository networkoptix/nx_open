#ifndef PTZ_TOUR_MODEL_H
#define PTZ_TOUR_MODEL_H

#include <QStandardItemModel>

#include <core/ptz/ptz_fwd.h>

class QnPtzTourModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit QnPtzTourModel(QObject *parent = 0);

signals:

public slots:
    void setTour(QnPtzTour *tour);
};

#endif // PTZ_TOUR_MODEL_H
