#include "camera_bookmarks_manager_p.h"

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/loaders/bookmark_camera_data_loader.h>

/* Temporary includes */
#include <common/common_module.h>
#include <api/helpers/bookmark_request_data.h>

namespace
{

    const static int defaultSearchLimit = 100;
}

QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent)
    : QObject(parent)
    , q_ptr(parent)
    , m_requests()
{
    //TODO: #GDM #Bookmarks clear cache on time changed and on history change. Should we refresh the cache on timer?

}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{
}

void QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                        , const QnCameraBookmarkSearchFilter &filter
                                                        , BookmarksCallbackType callback)
{

    auto server = qnCommon->currentServer();
    if (!server)
        return;

    auto connection = server->apiConnection();
    if (!connection)
        return;

    QnBookmarkRequestData requestData;
    requestData.cameras = cameras;
    requestData.format = Qn::JsonFormat;
    requestData.filter = filter;

    //TODO: #GDM #Bookmarks #IMPLEMENT ME
    int handle = connection->getBookmarksAsync(requestData, this, SLOT(handleDataLoaded(int, const QnCameraBookmarkList &, int)));
    if (callback)
        m_requests[handle] = callback;

    /*
    Here we should do the following:
    1) get list of already loaded bookmarks by this request.
    2) Select correct loader:
       * that may be the 'Search loader' that will support cameras list, text filter and start time only, also will not cache data and drop requests
       * or 'Timeline loader' - single camera, set period, cache data?, wait requests
    
    */

// 
// 
//     BookmarkRequestHolder request = { QnTimePeriod(filter.startTimeMs, filter.endTimeMs - filter.startTimeMs)
//         , callback, QnCameraBookmarkList(), AnswersContainer(), true };
// 
//     /// Make Load request for any camera
//     for(const auto &camera: cameras)
//     {
//         LoadersContainer::iterator it = m_loaderByResource.find(camera);
//         if (it == m_loaderByResource.end())
//         {
//             QnBookmarksLoader *loader = new QnBookmarksLoader(camera, this);
//                 connect(loader, &QnBookmarksLoader::bookmarksChanged, this, &QnCameraBookmarksManagerPrivate::bookmarksDataEvent);
//                 it = m_loaderByResource.insert(camera, loader);
//         }
// 
//         QnBookmarksLoader * const loader = it.value();
//         
//             //TODO: #GDM #Bookmarks IMPLEMENT ME
//         loader->load(request.targetPeriod);
//       //  request.answers.insert(std::make_pair(loader, handle));
//     }
// 
//     if (!request.answers.empty())
//         m_requests.push_back(request);
}

void QnCameraBookmarksManagerPrivate::handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int handle) {
    if (!m_requests.contains(handle))
        return;

    auto callback = m_requests.take(handle);
    callback(status == 0, bookmarks);
}



void QnCameraBookmarksManagerPrivate::clearCache() {
    m_requests.clear();

    for (auto query: m_queries)
        emit query->bookmarksChanged(QnCameraBookmarkList());

  /*  for (QnBookmarksLoader* loader: m_loaderByResource)
        if (loader)
            loader->discardCachedData();*/
}

void QnCameraBookmarksManagerPrivate::registerAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    m_queries << query;
}

void QnCameraBookmarksManagerPrivate::unregisterAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    m_queries.removeOne(query);
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::executeQuery(const QnCameraBookmarksQueryPtr &query) const {
    return QnCameraBookmarkList();
}
