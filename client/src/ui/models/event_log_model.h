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

    const QVector<QnLightBusinessActionVectorPtr> &events() const;
    void setEvents(const QVector<QnLightBusinessActionVectorPtr> &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    //QnLightBusinessActionPtr getEvent(const QModelIndex &index) const;
    void rebuild();
    void clear();

    virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const override;
    virtual void sort ( int column, Qt::SortOrder order = Qt::AscendingOrder ) override;

    BusinessEventType::Value eventType(const QModelIndex & index) const;
    QnResourcePtr eventResource(const QModelIndex & index) const;
    qint64 eventTimestamp(const QModelIndex & index) const;

    class DataIndex;
private:

    static QString columnTitle(Column column);
    
    QString textData(const Column& column,const QnLightBusinessAction &action) const;
    QVariant iconData(const Column& column, const QnLightBusinessAction &action) const;
    QVariant fontData(const Column& column, const QnLightBusinessAction &action) const;
    QVariant foregroundData(const Column& column, const QnLightBusinessAction &action) const;
    QVariant mouseCursorData(const Column& column, const QnLightBusinessAction &action) const;
    QVariant resourceData(const Column& column, const QnLightBusinessAction &action) const;
    QString formatUrl(const QString& url) const;

    QnLightBusinessActionVectorPtr mergeEvents(const QList <QnLightBusinessActionVectorPtr>& eventsList) const;
    QnLightBusinessActionVectorPtr merge2(const QList <QnLightBusinessActionVectorPtr>& eventsList) const;
    QnLightBusinessActionVectorPtr mergeN(const QList <QnLightBusinessActionVectorPtr>& eventsList) const;
private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    QFont m_linkFont;
    DataIndex* m_index;
};

#endif // QN_EVENT_LOG_MODEL_H
