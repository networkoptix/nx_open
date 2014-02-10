#ifndef PTZ_TOUR_LIST_MODEL_H
#define PTZ_TOUR_LIST_MODEL_H

#include <QAbstractTableModel>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_tour.h>

struct QnPtzTourItemModel {
    QnPtzTourItemModel(const QnPtzTour& tour):
        tour(tour), modified(false)
    {}
    QnPtzTourItemModel(const QString &name):
        tour(QString(), name, QnPtzTourSpotList()), modified(true)
    {}

    QnPtzTour tour;
    bool modified;
};

class QnPtzTourListModel : public QAbstractTableModel
{
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:
    enum Column {
        ModifiedColumn,
        NameColumn,
        DetailsColumn,

        ColumnCount
    };

    explicit QnPtzTourListModel(QObject *parent = 0);
    virtual ~QnPtzTourListModel();

    const QList<QnPtzTourItemModel> &tourModels() const;
    const QStringList &removedTours() const;
    void setTours(const QnPtzTourList &tours);

    const QnPtzPresetList& presets() const;
    void setPresets(const QnPtzPresetList &presets);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    Q_SLOT void updateTour(const QnPtzTour &tour);
private:
    qint64 estimatedTimeSecs(const QnPtzTour &tour) const;

    QList<QnPtzTourItemModel> m_tours;
    QStringList m_removedTours;
    QnPtzPresetList m_presets;
};

#endif // PTZ_TOUR_LIST_MODEL_H
