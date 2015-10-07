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
    Q_PROPERTY(bool showOffline READ showOffline WRITE setShowOffline NOTIFY showOfflineChanged)
    Q_PROPERTY(QStringList hiddenCameras READ hiddenCameras WRITE setHiddenCameras NOTIFY hiddenCamerasChanged)
    Q_PROPERTY(bool hiddenCamerasOnly READ hiddenCamerasOnly WRITE setHiddenCamerasOnly NOTIFY hiddenCamerasOnlyChanged)

    Q_ENUMS(Qn::ResourceStatus)

public:
    QnCameraListModel(QObject *parent = 0);

    void setServerId(const QnUuid &id);
    QnUuid serverId() const;

    QString serverIdString() const;
    void setServerIdString(const QString &id);

    virtual QHash<int, QByteArray> roleNames() const;
    virtual QVariant data(const QModelIndex &index, int role) const override;

    bool showOffline() const;
    void setShowOffline(bool showOffline);

    QStringList hiddenCameras() const;
    void setHiddenCameras(const QStringList &hiddenCameras);

    bool hiddenCamerasOnly() const;
    void setHiddenCamerasOnly(bool hiddenCamerasOnly);

public slots:
    void refreshThumbnails(int from, int to);
    void updateLayout(int width, qreal desiredAspectRatio);

signals:
    void serverIdStringChanged();
    void showOfflineChanged();
    void hiddenCamerasChanged();
    void hiddenCamerasOnlyChanged();

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private slots:
    void at_thumbnailUpdated(const QnUuid &resourceId, const QString &thumbnailId);

private:
    QnFilteredCameraListModel *m_model;
    QHash<QnUuid, QSize> m_sizeById;
    int m_width;
    bool m_showOffline;
    QSet<QString> m_hiddenCameras;
    bool m_hiddenCamerasOnly;
};



#endif // QNCAMERASMODEL_H
