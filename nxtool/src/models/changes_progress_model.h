
#pragma once

#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;
    class ModelChangeHelper;

    class ChangesProgressModel : public QAbstractListModel
    {
        Q_OBJECT

        Q_PROPERTY(int completedCount READ completedCount NOTIFY completedCountChanged)
    public:
        ChangesProgressModel(QObject *parent = nullptr);

        virtual ~ChangesProgressModel();

    public:
        /// Property getters
        int completedCount() const;

    public:
        bool addChangeProgress(const ApplyChangesTaskPtr &task);

        void removeByIndex(int index);

        void removeByTask(ApplyChangesTask *task);

        void serverDiscovered(const rtu::BaseServerInfo &info);
        
        void serversDisappeared(const IDsVector &ids);

        void unknownAdded(const QString &ip);

        ApplyChangesTaskPtr atIndex(int index);

    signals:
        void completedCountChanged();

    private:
        int rowCount(const QModelIndex &parent = QModelIndex()) const;
        
        QVariant data(const QModelIndex &index
            , int role = Qt::DisplayRole) const;
        
        Roles roleNames() const;

        void taskStateChanged(const ApplyChangesTask *task
            , int role);

    private:
        typedef std::vector<ApplyChangesTaskPtr> TasksContainer;
        TasksContainer::iterator findInsertPosition();

    private:
        const ModelChangeHelper * const m_changeHelper;
        TasksContainer m_tasks;
        int m_completedCount;
    };
}