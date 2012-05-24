#ifndef QN_RESOURCE_LIST_MODEL_H
#define QN_RESOURCE_LIST_MODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>

class AbstractAccessor;

class QnResourceListModelColumn {
public:
    enum ItemDataRole {
        FlagsRole = -1
    };

    QnResourceListModelColumn() {}

    AbstractAccessor *accessor(int role) const {
        return m_accessorByRole.value(role);
    }


private:
    QSet<AbstractAccessor *> m_accessors;
    QHash<int, AbstractAccessor *> m_accessorByRole;
    QSet<QByteArray> m_signals;
};


class QnResourceListModel: public QAbstractListModel {
    Q_OBJECT;

    typedef QAbstractListModel base_type;

public:
    QnResourceListModel(QObject *parent = NULL);
    virtual ~QnResourceListModel();

    const QnResourceList &resouces() const;
    void setResources(const QnResourceList &resouces);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResources();
    void submitToResources();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

private slots:
    void at_resource_resourceChanged(const QnResourcePtr &resource);
    void at_resource_resourceChanged();

private:
    bool m_readOnly;
    QnResourceList m_resources;
    QStringList m_names;
};


#endif // QN_RESOURCE_LIST_MODEL_H

