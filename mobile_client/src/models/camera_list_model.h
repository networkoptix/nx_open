#ifndef QNCAMERASMODEL_H
#define QNCAMERASMODEL_H

#include <QtCore/QSortFilterProxyModel>
#include <utils/common/id.h>

namespace {
    class QnFilteredCameraListModel;
}

class QnCameraListModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(QString serverIdString READ serverIdString WRITE setServerIdString NOTIFY serverIdStringChanged)
public:
    QnCameraListModel(QObject *parent = 0);

    void setServerId(const QnUuid &id);
    QnUuid serverId() const;

    QString serverIdString() const;
    void setServerIdString(const QString &id);

    virtual QHash<int, QByteArray> roleNames() const;
    virtual QVariant data(const QModelIndex &index, int role) const override;

public slots:
    void refreshThumbnails(int from, int to);
    void updateLayout(int width, qreal desiredAspectRatio);

signals:
    void serverIdStringChanged();

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private slots:
    void at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId);

private:
    QnFilteredCameraListModel *m_model;
    QHash<QnUuid, QSize> m_sizeById;
    int m_width;
};



#endif // QNCAMERASMODEL_H
