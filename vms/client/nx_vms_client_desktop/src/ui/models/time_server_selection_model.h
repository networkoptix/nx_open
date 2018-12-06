#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QTimer>

#include <client/client_color_types.h>
#include <utils/common/connective.h>
#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <api/server_rest_connection.h>

struct QnPeerRuntimeInfo;

class QnTimeServerSelectionModel :
    public ScopedModelOperations<Customized<Connective<QAbstractTableModel>>>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(QVector<QColor> colors READ colors WRITE setColors)
    using base_type = ScopedModelOperations<Customized<Connective<QAbstractTableModel>>>;

public:
    enum Columns
    {
        CheckboxColumn,
        NameColumn,
        DateColumn,
        ZoneColumn,
        TimeColumn,
        OffsetColumn,
        ColumnCount
    };

    explicit QnTimeServerSelectionModel(QObject* parent = nullptr);

    void updateTime();

    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QnUuid selectedServer() const;
    void setSelectedServer(const QnUuid& serverId);

    static QString formattedOffset(qint64 offsetMs);
    QVariant offsetForeground(qint64 offsetMs) const;

    bool sameTimezone() const;

    const QVector<QColor>& colors() const;
    void setColors(const QVector<QColor>& colors);
    void updateTimeOffset();

private:
    void addItem(QnMediaServerResourcePtr server);
    void updateColumn(Columns column);

    bool calculateSameTimezone() const;
    void resetData(qint64 currentSyncTime);

    void updateFirstItemCheckbox();

private:
    struct Item
    {
        QnUuid peerId;
        qint64 offset = 0;
        bool ready = false;
    };

    QList<Item> m_items;
    QnUuid m_selectedServer;

    /** Store received values to avoid long 'Synchronizing' in some cases. */
    QHash<QnUuid, qint64> m_serverOffsetCache;

    QVector<QColor> m_colors;

    mutable bool m_sameTimezone;
    mutable bool m_sameTimezoneValid;

    rest::Handle m_currentRequest;
};
