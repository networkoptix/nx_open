#ifndef __CAMERA_LIST_MODEL_H__
#define __CAMERA_LIST_MODEL_H__

#include <ui/models/resource_list_model.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;

class QnCameraListModel: public QnResourceListModel, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnResourceListModel base_type;

public:
    enum Column {
        RecordingColumn,
        NameColumn,
        VendorColumn,
        ModelColumn,
        FirmwareColumn,
        DriverColumn,
        IPColumn,
        UniqIdColumn,
        ServerColumn,
        ColumnCount
    };

    QnCameraListModel(QObject *parent = NULL);
    virtual ~QnCameraListModel();

    const QList<Column> &columns() const;
    void setColumns(const QList<Column> &columns);

    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QString columnTitle(Column column) const;

private:
    QList<Column> m_columns;
};

#endif // __CAMERA_LIST_MODEL_H__
