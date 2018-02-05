#pragma once

#include <bitset>

#include <QtCore/QAbstractListModel>
#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <utils/common/connective.h>

class QnResourceListModel: public ScopedModelOperations<Connective<QAbstractItemModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<Connective<QAbstractItemModel>>;

public:
    enum Column
    {
        NameColumn,
        CheckColumn,
        StatusColumn,
        ColumnCount
    };

    // TODO: #GDM really these options must be present in delegate, not in model.
    enum Option
    {
        HideStatusOption                = 0x01,
        ServerAsHealthMonitorOption     = 0x02,
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit QnResourceListModel(QObject *parent = nullptr);
    virtual ~QnResourceListModel();

    const QnResourceList &resources() const;
    void setResources(const QnResourceList &resources);
    void addResource(const QnResourcePtr &resource);
    void removeResource(const QnResourcePtr &resource);

    QSet<QnUuid> checkedResources() const;
    void setCheckedResources(const QSet<QnUuid>& ids);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    bool hasCheckboxes() const;
    void setHasCheckboxes(bool value);

    bool userCheckable() const;
    void setUserCheckable(bool value);

    bool hasStatus() const;
    void setHasStatus(bool value);

    Options options() const;
    void setOptions(Options value);

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;

    /**
     * Custom column accessor
     * Overrides a logic of QVariant data(const QModelIndex &index, int role) for specified column
     * Accessor gets a pointer to appropriate resource according to index.row()
     */
    using ColumnDataAccessor = std::function<QVariant(QnResourcePtr, int)>;
    void setCustomColumnAccessor(int column, ColumnDataAccessor dataAccessor);

private slots:
    void at_resource_resourceChanged(const QnResourcePtr& resource);

private:
    QIcon resourceIcon(const QnResourcePtr& resource) const;

private:
    bool m_readOnly = false;
    bool m_hasCheckboxes = false;
    bool m_userCheckable = true;
    bool m_hasStatus = false;
    Options m_options;
    QnResourceList m_resources;
    QSet<QnUuid> m_checkedResources;

    QMap<int, ColumnDataAccessor> m_customAccessors;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceListModel::Options)
