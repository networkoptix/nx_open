#ifndef TIME_SERVER_SELECTION_MODEL_H
#define TIME_SERVER_SELECTION_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <utils/common/connective.h>

#include <ui/workbench/workbench_context_aware.h>

class QnTimeServerSelectionModel : public Connective<QAbstractTableModel>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QAbstractTableModel> base_type;
public:
    enum Columns {
        CheckboxColumn,
        NameColumn,
        TimeColumn,
        ColumnCount
    };

    explicit QnTimeServerSelectionModel(QObject* parent = NULL);

    void updateTime();

    virtual int columnCount(const QModelIndex &parent) const override;
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
private:
    struct Item {
        QUuid peerId;
        qint64 time;
        quint64 priority;
    };

    QList<Item> m_items;
    qint64 m_timeBase;
    Qt::CheckState m_checked;
};

#endif // TIME_SERVER_SELECTION_MODEL_H
