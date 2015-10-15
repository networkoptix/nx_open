#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include "api/model/storage_space_reply.h"

struct BackupSettingsData
{
    BackupSettingsData(): isChecked(false), backupType(Qn::CameraBackup_Disabled) {}

    bool isChecked;
    QnUuid id;
    Qn::CameraBackupTypes backupType;
};
typedef QList<BackupSettingsData> BackupSettingsDataList;
Q_DECLARE_METATYPE(BackupSettingsData)

class QnBackupSettingsModel : public Customized<QAbstractListModel> 
{
    Q_OBJECT
    typedef Customized<QAbstractListModel> base_type;

public:
    enum Columns {
        CheckBoxColumn,
        CameraNameColumn,
        BackupTypeColumn,

        ColumnCount
    };

    QnBackupSettingsModel(QObject *parent = 0);
    ~QnBackupSettingsModel();

    void setModelData(const BackupSettingsDataList& data);
    BackupSettingsDataList modelData() const;
    
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
private:
    QString displayData(const QModelIndex &index) const;
    QVariant fontData(const QModelIndex &index) const;
    QVariant foregroundData(const QModelIndex &index) const;
    QVariant decorationData(const QModelIndex &index) const;
    QVariant checkstateData(const QModelIndex &index) const;
    QString backupTypeToString(Qn::CameraBackupTypes value) const;
private:
    BackupSettingsDataList m_data;
};
