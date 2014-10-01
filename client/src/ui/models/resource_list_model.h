#ifndef QN_RESOURCE_LIST_MODEL_H
#define QN_RESOURCE_LIST_MODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>

class QnResourceListModel: public QAbstractListModel {
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    QnResourceListModel(QObject *parent = NULL);
    virtual ~QnResourceListModel();

    const QnResourceList &resouces() const;
    void setResources(const QnResourceList &resouces);
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResources();
    void submitToResources();

    void setShowIp(bool showIp);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

private slots:
    void at_resource_resourceChanged(const QnResourcePtr &resource);

protected:
    bool m_readOnly;
    bool m_showIp;
    QnResourceList m_resources;
    QStringList m_names;
};


#endif // QN_RESOURCE_LIST_MODEL_H

