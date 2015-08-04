#ifndef QN_AUDIT_LOG_MODEL_H
#define QN_AUDIT_LOG_MODEL_H

#include <QtCore/QAbstractItemModel>

#include <utils/common/id.h>

#include <core/resource/resource_fwd.h>

#include "api/model/audit/audit_record.h"
#include "client/client_color_types.h"
#include <ui/customization/customized.h>

class QnAuditLogModel: public Customized<QAbstractItemModel>
{
    Q_OBJECT
    Q_PROPERTY(QnAuditLogColors colors READ colors WRITE setColors)
    
    typedef Customized<QAbstractItemModel> base_type;
public:
    enum Column {
        SelectRowColumn,
        TimestampColumn,
        EndTimestampColumn,
        DurationColumn,
        UserNameColumn,
        UserHostColumn,
        DateColumn,
        TimeColumn,
        UserActivityColumn, // not implemented yet
        EventTypeColumn,
        DescriptionColumn,
        PlayButtonColumn,
        ColumnCount
    };

    QnAuditLogModel(QObject *parent = NULL);
    virtual ~QnAuditLogModel();

    /*
    * Model uses reference to the data. Data MUST be alive till model clear call
    */
    void setData(const QnAuditRecordRefList &data);
    
    /*
    * Interleave colors for grouping records from a same session
    */
    void calcColorInterleaving();
    void clearData();
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void setData(const QModelIndexList &indexList, const QVariant &value, int role = Qt::EditRole);
    QnAuditRecordRefList data() const;
    QnAuditRecordRefList checkedRows();
    

    QList<Column> columns() const;
    virtual void setColumns(const QList<Column> &columns);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override  { return m_columns.size(); }

    QnAuditLogColors colors() const;
    void setColors(const QnAuditLogColors &colors);
    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const;

    static QString makeSearchPattern(const QnAuditRecord* record);

    void setheaderHeight(int value) { m_headerHeight = value; }

    static bool hasDetail(const QnAuditRecord* record);
    static void setDetail(QnAuditRecord* record, bool showDetail);
signals:
    void colorsChanged();
public slots:
    void clear();
protected:
    QnAuditRecord* rawData(int row) const;
private:
    class DataIndex;

    static QString getResourceNameString(QnUuid id);
    QString htmlData(const Column& column,const QnAuditRecord* data, int row, bool hovered) const;
    static QString formatDateTime(int timestampSecs, bool showDate = true, bool showTime = true);
    static QString formatDuration(int durationSecs);
    static QString eventTypeToString(Qn::AuditRecordType recordType);
    static QString eventDescriptionText(const QnAuditRecord* data);
    QVariant colorForType(Qn::AuditRecordType actionType) const;
    static QString buttonNameForEvent(Qn::AuditRecordType eventType);
    static QString textData(const Column& column,const QnAuditRecord* action);
    bool skipDate(const QnAuditRecord *record, int row) const;
    
    int minWidthForColumn(const Column &column) const;
    QSize sectionSizeFromContents(int logicalIndex) const;
    static QString getResourcesString(const std::vector<QnUuid>& resources);
    static QString searchData(const Column& column, const QnAuditRecord* data);
    QString descriptionTooltip(const QnAuditRecord *record) const;
protected:
    DataIndex* m_index;
    QList<Column> m_columns;
    QnAuditLogColors m_colors;
    int m_headerHeight;
    QVector<int> m_interleaveInfo;
private:
};

#endif // QN_AUDIT_LOG_MODEL_H
