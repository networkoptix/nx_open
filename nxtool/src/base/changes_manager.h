
#pragma once

#include <QObject>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;

    class RtuContext;
    class HttpClient;
    class ServersSelectionModel;
    class ApplyChangesTask;
    class ServersFinder;

    class ChangesManager : public QObject
    {
        Q_OBJECT

    public:
        ChangesManager(RtuContext *context
            , HttpClient *httpClient
            , ServersSelectionModel *selectionModel
            , ServersFinder *serversFinder
            , QObject *parent);
        
        virtual ~ChangesManager();
        
        ChangesProgressModel *changesProgressModel();

    public slots:
        QObject *changesProgressModelObject();

        QObject *changeset();

    public slots:  
        void applyChanges();
        
        void clearChanges();

    private:
        ChangesetPointer &getChangeset();

    private:
        RtuContext * const m_context;
        HttpClient * const m_httpClient;
        ServersSelectionModel * const m_selectionModel;
        ServersFinder * const m_serversFinder;

        const ChangesProgressModelPtr m_changesModel;
        ChangesetPointer m_changeset;
    };
}
