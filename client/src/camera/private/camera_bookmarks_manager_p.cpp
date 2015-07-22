#include "camera_bookmarks_manager_p.h"

#include <camera/camera_bookmarks_manager.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/loaders/bookmark_camera_data_loader.h>

namespace
{

    const static int defaultSearchLimit = 100;
}

QnCameraBookmarksManagerPrivate::QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent)
    : QObject(parent)
    , q_ptr(parent)
    , m_loaderByResource()
    , m_requests()
{
    //TODO: #GDM #Bookmarks clear cache on time changed and on history change. Should we refresh the cache on timer?

}

QnCameraBookmarksManagerPrivate::~QnCameraBookmarksManagerPrivate()
{
}

void QnCameraBookmarksManagerPrivate::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                        , const QnCameraBookmarkSearchFilter &filter
                                                        , const QUuid &requestId
                                                        , BookmarksCallbackType callback)
{

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

void QnCameraBookmarksManagerPrivate::bookmarksDataEvent(const QnCameraBookmarkList &bookmarks)
{
   /* const QnAbstractCameraDataLoader * const loader = static_cast<QnAbstractCameraDataLoader *>(sender());
    AnswersContainer::iterator itAnswer;
    const RequestsContainer::iterator itRequest = std::find_if(m_requests.begin(), m_requests.end()
        , [loader, handle, &itAnswer](RequestsContainer::value_type &request)
    {
        AnswersContainer &answers = request.answers;
        itAnswer = answers.find(loader);
        return (itAnswer != answers.end() && (itAnswer->second == handle));     
    });

    if (itRequest == m_requests.end()) /// We don't wait for such answer
        return; 

    bool &success = itRequest->success;
    success = success && (data ? true : false);

    QnCameraBookmarkList &bookmarks = itRequest->bookmarks;

    if (data)   /// if it is successful callback
    {
        typedef QSharedPointer<QnBookmarkCameraData> QnBookmarkCameraDataPtr;
        const QnBookmarkCameraDataPtr bookmarkData = data.dynamicCast<QnBookmarkCameraData>();
        QnCameraBookmarkList newBookmarks = bookmarkData->data(itRequest->targetPeriod);
    
        {
            /// TODO: #ynikitenkov Remove this section when cameraId will be set on bookmark creation
            const auto id = loader->resource()->getUniqueId();
            for(QnCameraBookmark& bookmark: newBookmarks) 
            {
                bookmark.cameraId = id; 
            }
        }

        bookmarks.append(newBookmarks);
    }

    AnswersContainer &answers = itRequest->answers;
    answers.erase(itAnswer);    /// Answer processed, we should remove it
    if (answers.empty())
    {
        itRequest->callback(success, bookmarks);
        m_requests.erase(itRequest);
    }*/
}

QnBookmarksLoader * QnCameraBookmarksManagerPrivate::loader(const QnVirtualCameraResourcePtr &camera, bool createIfNotExists) {
    LoadersContainer::const_iterator pos = m_loaderByResource.find(camera);
    if(pos != m_loaderByResource.cend())
        return *pos;

    if (!createIfNotExists)
        return nullptr;
    
    QnBookmarksLoader *loader = new QnBookmarksLoader(camera, this);
    connect(loader, &QnBookmarksLoader::bookmarksChanged, [this, camera] {
        Q_Q(QnCameraBookmarksManager);
        emit q->bookmarksChanged(camera);
    });

    m_loaderByResource[camera] = loader;
    return loader;
}

QnCameraBookmarkList QnCameraBookmarksManagerPrivate::bookmarks(const QnVirtualCameraResourcePtr &camera) const {
    LoadersContainer::const_iterator pos = m_loaderByResource.find(camera);
    if(pos == m_loaderByResource.cend())
        return QnCameraBookmarkList();

    auto loader = *pos;
    return loader->bookmarks();
}

void QnCameraBookmarksManagerPrivate::clearCache() {
    for (QnBookmarksLoader* loader: m_loaderByResource)
        if (loader)
            loader->discardCachedData();
}

void QnCameraBookmarksManagerPrivate::loadBookmarks(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod &period) {
    if (!camera || !period.isValid())
        return;

    auto loaderForCamera = loader(camera);
    Q_ASSERT_X(loaderForCamera, Q_FUNC_INFO, "Loader must be created here");
    if (loaderForCamera)
        loaderForCamera->load(period);
}
