#ifndef QN_PTZ_PRESET_LIST_MODEL_H
#define QN_PTZ_PRESET_LIST_MODEL_H

#include <QAbstractTableModel>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_hotkey.h>

class QnPtzPresetListModel: public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    typedef QAbstractTableModel base_type;

public:
    enum Column {
        NameColumn,
        HotkeyColumn
    };

    QnPtzPresetListModel(QObject *parent = NULL);
    virtual ~QnPtzPresetListModel();

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    const QnPtzPresetList &presets() const;
    void setPresets(const QnPtzPresetList &presets);

    QnHotkeysHash hotkeys() const;
    void setHotkeys(const QnHotkeysHash &value);

    int column(Column column) const;
    const QList<Column> &columns() const;
    void setColumns(const QList<Column> &columns);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

private:
    QString columnTitle(Column column) const;

private:
    bool m_readOnly;
    QnPtzPresetList m_presets;
    QnHotkeysHash m_hotkeys;
    QList<Column> m_columns;
};


#endif // QN_PTZ_PRESET_LIST_MODEL_H

