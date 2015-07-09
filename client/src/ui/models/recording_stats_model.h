#ifndef IOPORTS_VIEW_MODEL_H
#define IOPORTS_VIEW_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/QList>

#include <utils/common/id.h>
#include "api/model/api_ioport_data.h"
#include "api/model/recording_stats_reply.h"

class QnRecordingStatsModel : public QAbstractItemModel
{
    Q_OBJECT

    typedef QAbstractItemModel base_type;
public:

    enum Columns {
        CameraNameColumn,
        BytesColumn,
        DurationColumn,
        BitrateColumn,
        ColumnCount
    };


    explicit QnRecordingStatsModel(QObject *parent = 0);
    virtual ~QnRecordingStatsModel();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void clear();
    void setModelData(const QnRecordingStatsReply& data);
private:
    QString textData(const QModelIndex &index) const;
private:

    QnRecordingStatsReply m_data;
};



#endif // IOPORTS_VIEW_MODEL_H
