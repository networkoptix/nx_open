#ifndef PTZ_TOUR_LIST_MODEL_H
#define PTZ_TOUR_LIST_MODEL_H

#include <QtCore/QAbstractTableModel>
#include <QtCore/QUuid>

#include <core/ptz/ptz_tour.h>
#include <core/ptz/ptz_preset.h>

#include <client/client_model_types.h>

struct QnPtzTourItemModel {
    QnPtzTourItemModel(){}

    QnPtzTourItemModel(const QnPtzTour& tour):
        tour(tour),
        modified(false),
        local(false)
    {}

    QnPtzTourItemModel(const QString &name):
        tour(QUuid::createUuid().toString(), name, QnPtzTourSpotList()),
        modified(true),
        local(true)
    {}

    QnPtzTour tour;

    /** Tour is modified. */
    bool modified;

    /** Tour is just created locally, does not exists on server. */
    bool local;
};

struct QnPtzPresetItemModel {
    QnPtzPresetItemModel() {}

    QnPtzPresetItemModel(const QnPtzPreset& preset):
        preset(preset),
        modified(false),
        local(false)
    {}

    QnPtzPresetItemModel(const QString &name):
        preset(QUuid::createUuid().toString(), name),
        modified(true),
        local(true)
    {}

    QnPtzPreset preset;

    /** Preset name is modified. */
    bool modified;

    /** Preset is just created locally, does not exists on server. */
    bool local;
};

class QnPtzManageModel : public QAbstractTableModel
{
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:
    enum Column {
        ModifiedColumn,
        NameColumn,
        HotkeyColumn,
        HomeColumn,
        DetailsColumn,

        ColumnCount
    };

    enum RowType {
        PresetTitleRow,
        PresetRow,
        TourTitleRow,
        TourRow,

        InvalidRow
    };

    struct RowData {
        RowType rowType;
        QnPtzPresetItemModel presetModel;
        QnPtzTourItemModel tourModel;

        RowData(): rowType(InvalidRow) {}
    };

    explicit QnPtzManageModel(QObject *parent = 0);
    virtual ~QnPtzManageModel();

    const QList<QnPtzTourItemModel> &tourModels() const;
    const QStringList &removedTours() const;
    void setTours(const QnPtzTourList &tours);
    void addTour();
    void removeTour(const QString &id);
    
    const QList<QnPtzPresetItemModel> &presetModels() const;
    const QStringList &removedPresets() const;
    
    void setPresets(const QnPtzPresetList &presets);
    void addPreset();
    void removePreset(const QString &id);

    const QnPtzHotkeyHash& hotkeys() const;
    void setHotkeys(const QnPtzHotkeyHash & hotkeys);

    const QString homePosition() const;
    void setHomePosition(const QString &homePosition);

    RowData rowData(int row) const;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    Q_SLOT void updateTourSpots(const QString tourId, const QnPtzTourSpotList &spots);

    bool synchronized() const;
    Q_SLOT void setSynchronized();
signals:
    void presetsChanged(const QnPtzPresetList &presets);
private:
    const QnPtzPresetList& presets() const;

    qint64 estimatedTimeSecs(const QnPtzTour &tour) const;
    bool tourIsValid(const QnPtzTourItemModel &tourModel) const;
    void updatePresetsCache();

    QVariant titleData(RowType rowType,  int column, int role) const;
    QVariant presetData(const QnPtzPresetItemModel &presetModel, int column, int role) const;
    QVariant tourData(const QnPtzTourItemModel &tourModel, int column, int role) const;

    QList<QnPtzPresetItemModel> m_presets;
    QStringList m_removedPresets;

    QList<QnPtzTourItemModel> m_tours;
    QStringList m_removedTours;

    QnPtzHotkeyHash m_hotkeys;
    QString m_homePosition;

    QnPtzPresetList m_ptzPresetsCache;
};

#endif // PTZ_TOUR_LIST_MODEL_H
