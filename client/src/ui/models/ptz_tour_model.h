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

    static QString speedToString(qreal speed);
    static QList<qreal> speedValues();

    static QString timeToString(quint64 time);
    static QList<quint64> stayTimeValues();

    const QnPtzTour& tour() const;
    Q_SLOT void setTour(const QnPtzTour &tour);

    const QString tourName() const;
    Q_SLOT void setTourName(const QString &name);

    QnPtzPresetList sortedPresets() const;
    const QnPtzPresetList &presets() const;
    Q_SLOT void setPresets(const QnPtzPresetList &presets);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

signals:
    void tourChanged(const QnPtzTour &tour);

private slots:
    void at_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    bool isPresetValid(const QString &presetId) const;

private:
    QnPtzTour m_tour;
    QnPtzPresetList m_presets;
};

#endif // PTZ_TOUR_MODEL_H
