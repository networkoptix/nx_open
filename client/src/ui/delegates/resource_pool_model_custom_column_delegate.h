#pragma once

#include <QtCore/QAbstractItemModel>

class QnResourcePoolModelCustomColumnDelegate: public QObject {
    Q_OBJECT
public:
    QnResourcePoolModelCustomColumnDelegate(QObject* parent = nullptr):
        QObject(parent){}
    virtual ~QnResourcePoolModelCustomColumnDelegate(){}

    virtual Qt::ItemFlags flags(const QModelIndex &index) const = 0;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const = 0;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) = 0;

signals:
    void notifyDataChanged();
};
