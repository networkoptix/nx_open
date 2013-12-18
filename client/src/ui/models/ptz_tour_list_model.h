#ifndef PTZ_TOUR_LIST_MODEL_H
#define PTZ_TOUR_LIST_MODEL_H

#include <QAbstractTableModel>

#include <core/ptz/ptz_fwd.h>

class QnPtzTourListModel : public QAbstractTableModel
{
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:
    enum Column {
        NameColumn,

        ColumnCount
    };

    explicit QnPtzTourListModel(QObject *parent = 0);
    virtual ~QnPtzTourListModel();

    const QnPtzTourList& tours() const;
    void setTours(const QnPtzTourList &tours);

    const QnPtzPresetList& presets() const;
    void setPresets(const QnPtzPresetList &presets);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Q_SLOT void updateTour(const QnPtzTour &tour);
private:
    QnPtzTourList m_tours;
    QnPtzPresetList m_presets;
};

#endif // PTZ_TOUR_LIST_MODEL_H
