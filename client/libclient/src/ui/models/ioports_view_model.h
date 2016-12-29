#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <api/model/api_ioport_data.h>

class QnIOPortsViewModel : public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    enum Columns
    {
        NumberColumn,
        IdColumn,
        TypeColumn,
        DefaultStateColumn,
        NameColumn,
        ActionColumn,
        DurationColumn,
        ColumnCount
    };

    enum Action
    {
        NoAction,
        ToggleState,
        Impulse
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

    static QString portTypeToString(Qn::IOPortType portType);
    static QString stateToString(Qn::IODefaultState state);
    static QString actionToString(Action action);

private:
    QString textData(const QModelIndex &index) const;
    QVariant editData(const QModelIndex &index) const;
    bool isDisabledData(const QModelIndex &index) const;
    Action actionFor(const QnIOPortData& value) const;

private:
    QnIOPortDataList m_data;
};
