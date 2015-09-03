
#pragma once

#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    class ModelChangeHelper;
    struct BaseServerInfo;

    class ChangesProgressModel : public QAbstractListModel
    {
        Q_OBJECT

    public:
        ChangesProgressModel(QObject *parent = nullptr);

        virtual ~ChangesProgressModel();

        ///

        void addChangeProgress(ApplyChangesTaskPtr &&task);

        void serverDiscovered(const rtu::BaseServerInfo &info);
        
        void serversDisappeared(const IDsVector &ids);

        void unknownAdded(const QString &ip);

        void removeChangeProgress(QObject *object);

    public slots:

        QObject *taskAtIndex(int index);

    private:
        int rowCount(const QModelIndex &parent = QModelIndex()) const;
        
        QVariant data(const QModelIndex &index
            , int role = Qt::DisplayRole) const;
        
        Roles roleNames() const;

    private:
        void taskStateChanged(const ApplyChangesTask *task
            , int role);

    private:
        typedef std::vector<ApplyChangesTaskPtr> TasksContainer;

        const ModelChangeHelper * const m_changeHelper;
        TasksContainer m_tasks;
    };
}