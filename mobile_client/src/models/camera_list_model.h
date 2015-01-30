#ifndef QNCAMERASMODEL_H
#define QNCAMERASMODEL_H

#include <QtCore/QSortFilterProxyModel>

namespace {
    class QnFilteredCameraListModel;
}

class QnUuid;

class QnCameraListModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(QString serverIdString READ serverIdString WRITE setServerIdString NOTIFY serverIdStringChanged)
public:
    QnCameraListModel(QObject *parent = 0);

    void setServerId(const QnUuid &id);
    QnUuid serverId() const;

    QString serverIdString() const;
    void setServerIdString(const QString &id);

signals:
    void serverIdStringChanged();

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private slots:
    void at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId);

private:
    QnFilteredCameraListModel *m_model;
};



#endif // QNCAMERASMODEL_H
