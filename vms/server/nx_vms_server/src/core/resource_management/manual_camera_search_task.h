#pragma once

#include <nx/network/socket_common.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_searcher.h>
#include <api/model/manual_camera_seach_reply.h>

// TODO: The way to cancel task should be added. Inherit from QnStoppableAsync and implement pleaseStop,
// should call pleaseStop of searchers.
class QnSearchTask
{
public:
    typedef std::vector<QnAbstractNetworkResourceSearcher*> SearcherList;
    typedef std::function<void(
        QnManualResourceSearchList results, QnSearchTask* const task)> SearchDoneCallback;

    QnSearchTask() = delete;
    QnSearchTask(
        QnCommonModule* commonModule,
        nx::utils::Url url,
        bool breakOnGotResult = false);

    void setSearchers(const SearcherList& searchers);
    void setSearchDoneCallback(const SearchDoneCallback& callback);
    void setBlocking(bool isBlocking);
    void setInterruptTaskProcessing(bool interrupt);

    bool isBlocking() const;
    bool doesInterruptTaskProcessing() const;

    void start();

    nx::utils::Url url();
    QString toString() const;

private:
    QnCommonModule * m_commonModule = nullptr;
    nx::utils::Url m_url;

    /**
     * If one of the searchers in task found a resource than other searchers in the same task
     * won't be launched.
     */
    bool m_breakIfGotResult = false;

    // Need to wait while task will be done before launching next task in queue.
    bool m_blocking = false;

    // If we got some results from this task then other tasks in queue shouldn't be launched.
    bool m_interruptTaskProcesing = false;
    SearchDoneCallback m_callback;
    SearcherList m_searchers;
};
