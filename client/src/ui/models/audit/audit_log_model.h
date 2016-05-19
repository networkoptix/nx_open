#ifndef QN_AUDIT_LOG_MODEL_H
#define QN_AUDIT_LOG_MODEL_H

#include <QtCore/QAbstractListModel>

#include <api/model/audit/audit_record.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/id.h>

class QnAuditLogModel: public Customized<QAbstractListModel>, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnAuditLogColors colors READ colors WRITE setColors)

    typedef Customized<QAbstractListModel> base_type;

public:
    enum Column
    {
        SelectRowColumn,
        TimestampColumn,
        EndTimestampColumn,
        DurationColumn,
        UserNameColumn,
        UserHostColumn,
        DateColumn,
        TimeColumn,
        UserActivityColumn,
        EventTypeColumn,
        DescriptionColumn,

        CameraNameColumn,
        CameraIpColumn,

        ColumnCount
    };

    static const QByteArray ChildCntParamName;
    static const QByteArray CheckedParamName;

    QnAuditLogModel(QObject *parent = NULL);
    virtual ~QnAuditLogModel();

    /*
    * Model uses reference to the data. Data MUST be alive till model clear call
    */
    virtual void setData(const QnAuditRecordRefList &data);

    /*
    * Interleave colors for grouping records from a same session
    */
    void calcColorInterleaving();
    void clearData();
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void setData(const QModelIndexList &indexList, const QVariant &value, int role = Qt::EditRole);
    QnAuditRecordRefList checkedRows();

    virtual void setColumns(const QList<Column> &columns);

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QnAuditLogColors colors() const;
    void setColors(const QnAuditLogColors &colors);
    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const;

    QString makeSearchPattern(const QnAuditRecord* record) const;

    void setHeaderHeight(int value);

    static bool hasDetail(const QnAuditRecord* record);
    static void setDetail(QnAuditRecord* record, bool showDetail);
    static QString eventTypeToString(Qn::AuditRecordType recordType);

    static QnVirtualCameraResourceList getCameras(const QnAuditRecord* record);

signals:
    void colorsChanged();

public slots:
    void clear();

protected:
    QnAuditRecord* rawData(int row) const;

private:
    class DataIndex;

    static QString getResourceNameById(const QnUuid &id);
    QString htmlData(const Column& column,const QnAuditRecord* data, int row, bool hovered) const;
    QString formatDateTime(int timestampSecs, bool showDate = true, bool showTime = true) const;
    static QString formatDateTime(const QDateTime& dateTime, bool showDate = true, bool showTime = true);
    static QString formatDuration(int durationSecs);
    QString eventDescriptionText(const QnAuditRecord* data) const;
    QVariant colorForType(Qn::AuditRecordType actionType) const;
    QString textData(const Column& column,const QnAuditRecord* action) const;
    bool skipDate(const QnAuditRecord *record, int row) const;
    static QString getResourcesString(const std::vector<QnUuid>& resources);
    static QnVirtualCameraResourceList getCameras(const std::vector<QnUuid>& resources);
    QString searchData(const Column& column, const QnAuditRecord* data) const;
    QString descriptionTooltip(const QnAuditRecord *record) const;
    bool isDetailDataSupported(const QnAuditRecord *record) const;

protected:
    QScopedPointer<DataIndex> m_index;
    QList<Column> m_columns;
    QnAuditLogColors m_colors;
    int m_headerHeight;
    QVector<int> m_interleaveInfo;
};

#endif // QN_AUDIT_LOG_MODEL_H
