#ifndef QN_RESOURCE_LIST_MODEL_H
#define QN_RESOURCE_LIST_MODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnResourceListModel: public Connective<QAbstractItemModel>
{
    Q_OBJECT
    typedef Connective<QAbstractItemModel> base_type;

public:
    enum Column
    {
        NameColumn,
        CheckColumn,
        ColumnCount
    };

    QnResourceListModel(QObject *parent = NULL);
    virtual ~QnResourceListModel();

    const QnResourceList &resources() const;
    void setResources(const QnResourceList &resources);
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);

    QSet<QnUuid> checkedResources() const;
    void setCheckedResources(const QSet<QnUuid>& ids);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    bool isCheckable() const;
    void setCheckable(bool value);

    bool isStatusIgnored() const;
    void setStatusIgnored(bool value);

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;

private slots:
    void at_resource_resourceChanged(const QnResourcePtr& resource);

private:
    QIcon resourceIcon(const QnResourcePtr& resource) const;

private:
    bool m_readOnly;
    bool m_checkable;
    bool m_statusIgnored;
    QnResourceList m_resources;
    QSet<QnUuid> m_checkedResources;
};


#endif // QN_RESOURCE_LIST_MODEL_H

