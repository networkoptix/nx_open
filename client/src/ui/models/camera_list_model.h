#ifndef __CAMERA_LIST_MODEL_H__
#define __CAMERA_LIST_MODEL_H__

#include <ui/models/resource_list_model.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;

class QnCameraListModel: public QnResourceListModel, public QnWorkbenchContextAware
{
public:

    enum Column {
        RecordingColumn,
        NameColumn,
        VendorColumn,
        ModelColumn,
        FirmwareColumn,
        IPColumn,
        UniqIdColumn,
        ServerColumn,
        ColumnCount
    };

    QnCameraListModel(QObject *parent = NULL, QnWorkbenchContext* context = NULL);
    virtual QVariant data(const QModelIndex &index, int role) const override;

    void setColumns(const QList<Column>& columns);
private:
    QString columnTitle(Column column) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
private:
    QList<Column> m_columns;
};

#endif // __CAMERA_LIST_MODEL_H__
