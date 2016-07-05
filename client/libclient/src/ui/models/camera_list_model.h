#ifndef __CAMERA_LIST_MODEL_H__
#define __CAMERA_LIST_MODEL_H__

#include <QtCore/QAbstractItemModel>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchContext;

class QnCameraListModel: public Connective<QAbstractItemModel>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QAbstractItemModel> base_type;
public:
    enum Column {
        RecordingColumn,
        NameColumn,
        VendorColumn,
        ModelColumn,
        FirmwareColumn,
        IpColumn,
        MacColumn,
        ServerColumn,
        ColumnCount
    };

    QnCameraListModel(QObject *parent = NULL);
    virtual ~QnCameraListModel();
    
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    //virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QnMediaServerResourcePtr server() const;
    Q_SLOT void setServer(const QnMediaServerResourcePtr & server);
signals:
    void serverChanged();

private slots:
    void addCamera(const QnResourcePtr &resource);
    void removeCamera(const QnResourcePtr &resource);

    void at_resource_parentIdChanged(const QnResourcePtr &resource);
    void at_resource_resourceChanged(const QnResourcePtr &resource);
    

private:
    bool cameraFits(const QnVirtualCameraResourcePtr &camera) const;

    QList<QnVirtualCameraResourcePtr> m_cameras;
    QnMediaServerResourcePtr m_server;
    
};

#endif // __CAMERA_LIST_MODEL_H__
