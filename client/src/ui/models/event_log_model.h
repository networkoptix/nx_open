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

    void setEvents(const QVector<QnBusinessActionDataListPtr> &events);

    QList<Column> columns() const;
    void setColumns(const QList<Column> &columns);

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    bool hasMotionUrl(const QModelIndex &index) const;

    BusinessEventType::Value eventType(int row) const;
    QnResourcePtr eventResource(int row) const;
    qint64 eventTimestamp(int row) const;

    class DataIndex;
public slots:
    void clear();
    void rebuild();
private:
    QnResourcePtr getResource(const Column &column, const QnBusinessActionData &action) const;
    QVariant fontData(const Column& column, const QnBusinessActionData &action) const;
    QVariant foregroundData(const Column& column, const QnBusinessActionData &action) const;

    static QString columnTitle(Column column);
    
    static QVariant iconData(const Column& column, const QnBusinessActionData &action);
    static QVariant mouseCursorData(const Column& column, const QnBusinessActionData &action);
    static QString textData(const Column& column,const QnBusinessActionData &action);

    static QString motionUrl(Column column, const QnBusinessActionData& action);
    static QString formatUrl(const QString& url);
    static QnResourcePtr getResourceById(const QnId& id);
    static QString getResourceNameString(QnId id);
    static QString getUserGroupString(QnBusinessActionParameters::UserGroup value);
private slots:
    void at_resource_removed(QnResourcePtr res);
private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    QFont m_linkFont;
    DataIndex* m_index;
    static QHash<QnId, QnResourcePtr> m_resourcesHash;
};

#endif // QN_EVENT_LOG_MODEL_H
