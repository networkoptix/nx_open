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

    void setEvents(const QVector<QnLightBusinessActionVectorPtr> &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    //QnLightBusinessActionPtr getEvent(const QModelIndex &index) const;
    void rebuild();
    void clear();

    virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const override;
    virtual void sort ( int column, Qt::SortOrder order = Qt::AscendingOrder ) override;

    bool isMotionUrl(const QModelIndex & index) const;
    QnResourcePtr getResource(const QModelIndex& idx) const;

    BusinessEventType::Value eventType(int row) const;
    QnResourcePtr eventResource(int row) const;
    qint64 eventTimestamp(int row) const;

    class DataIndex;
private:
    QVariant fontData(const Column& column, const QnLightBusinessAction &action) const;
    QVariant foregroundData(const Column& column, const QnLightBusinessAction &action) const;

    static QString columnTitle(Column column);
    
    static QVariant iconData(const Column& column, const QnLightBusinessAction &action);
    static QVariant mouseCursorData(const Column& column, const QnLightBusinessAction &action);
    static QVariant resourceData(const Column& column, const QnLightBusinessAction &action);
    static QString textData(const Column& column,const QnLightBusinessAction &action);

    static QString motionUrl(Column column, const QnLightBusinessAction& action);
    static QString formatUrl(const QString& url);
    static QnResourcePtr getResourceById(const QnId& id);
private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    QFont m_linkFont;
    DataIndex* m_index;
};

#endif // QN_EVENT_LOG_MODEL_H
