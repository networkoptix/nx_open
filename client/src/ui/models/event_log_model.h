#ifndef QN_EVENT_LOG_MODEL_H
#define QN_EVENT_LOG_MODEL_H

#include <QtGui/QStandardItemModel>
#include "business/actions/abstract_business_action.h"

class QnEventLogModel: public QStandardItemModel {
    Q_OBJECT
    typedef QStandardItemModel base_type;

public:
    enum Column {
        DateTimeColumn,
        EventColumn,
        EventCameraColumn,
        ActionColumn,
        ActionCameraColumn,
        RepeatCountColumn,
        DescriptionColumn,
        ColumnCount
    };

    QnEventLogModel(QObject *parent = NULL);
    virtual ~QnEventLogModel();

    const QnAbstractBusinessActionList &events() const;
    void setEvents(const QnAbstractBusinessActionList &licenses);
    void addEvents(const QnAbstractBusinessActionList &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    QnAbstractBusinessActionPtr getEvent(const QModelIndex &index) const;

private:
    void rebuild();

    static QString columnTitle(Column column);
    static QStandardItem *createItem(Column column, const QnAbstractBusinessActionPtr &license);

private:
    QList<Column> m_columns;
    QnAbstractBusinessActionList m_events;
};

#endif // QN_EVENT_LOG_MODEL_H
