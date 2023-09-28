// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PTZ_MANAGE_MODEL_H
#define QN_PTZ_MANAGE_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/ptz/types.h>

struct QnPtzTourItemModel {
    QnPtzTourItemModel(){}

    QnPtzTourItemModel(const QnPtzTour& tour):
        tour(tour),
        modified(false),
        local(false)
    {}

    QnPtzTourItemModel(const QString &name):
        tour(QnUuid::createUuid().toString(), name, QnPtzTourSpotList()),
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
        initialName(preset.name),
        modified(false),
        local(false)
    {}

    QnPtzPresetItemModel(const QString &name):
        preset(QnUuid::createUuid().toString(), name),
        initialName(name),
        modified(true),
        local(true)
    {}

    bool isNameModified() const { return preset.name != initialName; }

    QnPtzPreset preset;
    QString initialName;

    /** Preset name is modified. */
    bool modified;

    /** Preset is just created locally, does not exists on server. */
    bool local;
};


class QnPtzManageModel : public QAbstractTableModel {
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
        RowData(const QnPtzPresetItemModel &presetModel): rowType(PresetRow), presetModel(presetModel) {}
        RowData(const QnPtzTourItemModel &tourModel): rowType(TourRow), tourModel(tourModel) {}

        QString id() const;
        QString name() const;
        bool modified() const;
        bool local() const;
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

    QnPtzPresetList presets() const;
    void setPresets(const QnPtzPresetList &presets);
    void addPreset();
    void removePreset(const QString &id);

    using PresetIdByHotkey = nx::vms::client::core::ptz::PresetIdByHotkey;
    const PresetIdByHotkey& hotkeys() const;
    void setHotkeys(const PresetIdByHotkey & hotkeys);

    const QString homePosition() const;
    void setHomePosition(const QString &homePosition);
    bool isHomePositionChanged() const;

    RowData rowData(int row) const;
    RowData rowData(const QString &id, int *index = nullptr) const;
    int rowNumber(const RowData &rowData) const;

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

    // TODO: #sivanov Implement properly.
    bool tourIsValid(const QnPtzTourItemModel &tourModel) const;

signals:
    void presetsChanged(const QnPtzPresetList &presets);

private:
    enum TourState {
        ValidTour,
        IncompleteTour,
        DuplicatedLinesTour,
        OtherInvalidTour
    };

    qint64 estimatedTimeSecs(const QnPtzTour &tour) const;
    TourState tourState(const QnPtzTourItemModel &tourModel, QString *stateString = nullptr) const;
    void updatePresetsCache();

    QVariant titleData(RowType rowType,  int column, int role) const;
    QVariant presetData(const QnPtzPresetItemModel &presetModel, int column, int role) const;
    QVariant tourData(const QnPtzTourItemModel &tourModel, int column, int role) const;

    int presetIndex(const QString &id) const;
    int tourIndex(const QString &id) const;

    void setHomePositionInternal(const QString &homePosition, bool setChanged);
    bool setHotkeyInternal(int hotkey, const QString &id);

    QStringList collectTourNames() const;
    QStringList collectPresetNames() const;

    /** Check if some hotkeys are not valid anymore and clean them from inner structure. */
    void cleanHotkeys();
private:
    QList<QnPtzPresetItemModel> m_presets;
    QStringList m_removedPresets;

    QList<QnPtzTourItemModel> m_tours;
    QStringList m_removedTours;

    PresetIdByHotkey m_hotkeys;
    QString m_homePosition;
    bool m_homePositionChanged;

    QnPtzPresetList m_ptzPresetsCache;
};

#endif // QN_PTZ_MANAGE_MODEL_H
