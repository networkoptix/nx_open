#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <api/model/api_ioport_data.h>

#include <common/common_globals.h>

class QnIOPortsViewModel : public QAbstractListModel
{
    Q_OBJECT

    typedef QAbstractListModel base_type;
public:

    enum Columns {
        IdColumn,
        TypeColumn,
        DefaultStateColumn,
        NameColumn,
        AutoResetColumn,
        ColumnCount
    };


    explicit QnIOPortsViewModel(QObject *parent = 0);
    virtual ~QnIOPortsViewModel();

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

    QString portTypeToString(Qn::IOPortType portType) const; 
    QString stateToString(Qn::IODefaultState state) const;
private:
    QnIOPortDataList m_data;
};
