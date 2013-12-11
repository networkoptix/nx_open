#ifndef PTZ_TOUR_MODEL_H
#define PTZ_TOUR_MODEL_H

#include <QStandardItemModel>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_tour.h>

class QnPtzTourModel : public QAbstractTableModel
{
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:
    enum Column {
        NameColumn,
        TimeColumn,
        SpeedColumn,

        ColumnCount
    };

    explicit QnPtzTourModel(QObject *parent = 0);
    virtual ~QnPtzTourModel();

    const QnPtzTour& tour() const;
    void setTour(const QnPtzTour &tour);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
private:
    QnPtzTour m_tour;
};

#endif // PTZ_TOUR_MODEL_H
