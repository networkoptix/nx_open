#ifndef QN_EVENT_LOG_MODEL_H
#define QN_EVENT_LOG_MODEL_H

#include <QtGui/QStandardItemModel>
#include "business/actions/abstract_business_action.h"

class QnEventLogModel: public QStandardItemModel {
    Q_OBJECT
    typedef QStandardItemModel base_type;

public:
    enum Roles {
         ItemLinkRole = Qt::UserRole + 500
    };

    enum Column {
        DateTimeColumn,
        EventColumn,
        EventCameraColumn,
        ActionColumn,
        ActionCameraColumn,
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
    void rebuild();
    void clear();

    virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const override;
private:

    static QString columnTitle(Column column);
    QStandardItem *createItem(Column column, const QnAbstractBusinessActionPtr &action) const;
    
    QVariant textData(const Column& column,const QnAbstractBusinessActionPtr &action) const;
    QVariant iconData(const Column& column, const QnAbstractBusinessActionPtr &action) const;
    QVariant fontData(const Column& column, const QnAbstractBusinessActionPtr &action) const;
    QVariant foregroundData(const Column& column, const QnAbstractBusinessActionPtr &action) const;
    QVariant mouseCursorData(const Column& column, const QnAbstractBusinessActionPtr &action) const;
private:
    QList<Column> m_columns;
    QnAbstractBusinessActionList m_events;
    QBrush m_linkBrush;
    QFont m_linkFont;
};

#endif // QN_EVENT_LOG_MODEL_H
