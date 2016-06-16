#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnCameraListModelPrivate;

class QnCameraListModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = QSortFilterProxyModel;

public:
    QnCameraListModel(QObject* parent = nullptr);
    ~QnCameraListModel();

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

public slots:
    void refreshThumbnail(int row);
    void refreshThumbnails(int from, int to);

signals:
    void layoutIdChanged();

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QScopedPointer<QnCameraListModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCameraListModel)
};
