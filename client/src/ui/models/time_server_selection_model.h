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

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;
private:
    struct Item {
        QUuid peerId;
        qint64 time;
        quint64 priority;
    };

    QList<Item> m_items;
    qint64 m_timeBase;
};

#endif // TIME_SERVER_SELECTION_MODEL_H
