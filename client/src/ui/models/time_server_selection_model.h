#ifndef TIME_SERVER_SELECTION_MODEL_H
#define TIME_SERVER_SELECTION_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <utils/common/connective.h>
#include <utils/common/uuid.h>

#include <ui/workbench/workbench_context_aware.h>


struct QnPeerRuntimeInfo;

class QnTimeServerSelectionModel : public Connective<QAbstractTableModel>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QAbstractTableModel> base_type;
public:
    enum Columns {
        CheckboxColumn,
        NameColumn,
        TimeColumn,
        OffsetColumn,
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

    QnUuid selectedServer() const;
    void setSelectedServer(const QnUuid &serverId);

    static bool isSelected(quint64 priority);
    static QString formattedOffset(qint64 offsetMSec);

    bool sameTimezone() const;
private:
    void addItem(const QnPeerRuntimeInfo &info);
    void updateColumn(Columns column);

    bool calculateSameTimezone() const;
private:
    struct Item {
        QnUuid peerId;
        qint64 offset;
        quint64 priority;
        bool ready;
    };

    QList<Item> m_items;
    QnUuid m_selectedServer;

    /** Store received values to avoid long 'Synchronizing' in some cases. */
    QHash<QnUuid, qint64> m_serverOffsetCache;

    mutable bool m_sameTimezone;
    mutable bool m_sameTimezoneValid;
};

#endif // TIME_SERVER_SELECTION_MODEL_H
