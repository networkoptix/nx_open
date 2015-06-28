#ifndef IOPORTS_VIEW_MODEL_H
#define IOPORTS_VIEW_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <business/business_fwd.h>

#include <ui/models/business_rule_view_model.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/id.h>
#include "api/model/api_ioport_data.h"

class QnIOPortsViewModel : public QAbstractItemModel //, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QAbstractItemModel base_type;
public:

    enum Columns {
        IdColumn,
        TypeColumn,
        DefaultStateColumn,
        NameColumn,
        AutoResetColumn,
        ColumnCount,
        SupportedTypesColumn
    };


    explicit QnIOPortsViewModel(QObject *parent = 0);
    virtual ~QnIOPortsViewModel();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void clear();
    void setModelData(const QnIOPortDataList& data);
    QnIOPortDataList modelData() const;
private:
    QString textData(const QModelIndex &index) const;
    QVariant editData(const QModelIndex &index) const;
    bool isDisabledData(const QModelIndex &index) const;
private:
    QnIOPortDataList m_data;
};



#endif // IOPORTS_VIEW_MODEL_H
