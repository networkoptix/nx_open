#pragma once

#include <QtCore/QAbstractItemModel>

class QnResourceTreeModelCustomColumnDelegate: public QObject {
    Q_OBJECT
public:
    QnResourceTreeModelCustomColumnDelegate(QObject* parent = nullptr):
        QObject(parent){}
    virtual ~QnResourceTreeModelCustomColumnDelegate(){}

    virtual Qt::ItemFlags flags(const QModelIndex &index) const = 0;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const = 0;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) = 0;

signals:
    void notifyDataChanged();
};
