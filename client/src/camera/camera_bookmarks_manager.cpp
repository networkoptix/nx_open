
#include "camera_bookmarks_manager.h"

#include <core/resource/resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/loaders/generic_camera_data_loader.h>
#include <camera/data/bookmark_camera_data.h>

namespace
{
    typedef int HandleType;
    typedef std::map<const QnAbstractCameraDataLoader *, HandleType> AnswersContainer;

    /// @brief Structure holds data for the request of the bookmarks
    struct BookmarkRequestHolder
    {
        QnTimePeriod targetPeriod;
        QnCameraBookmarksManager::BookmarksCallbackType callback;
        QnCameraBookmarkList bookmarks;
        AnswersContainer answers;
        bool success;
    };
}

class QnCameraBookmarksManager::Impl : public QObject
{
public:
    Impl(QObject *parent);

    virtual ~Impl();

    void getBookmarksAsync(const FilterParameters &filter
        , const BookmarksCallbackType &callback
        , bool clearBookmarksCache);

private:
    /// @brief Callback for bookmarks. If data is nullptr it means error occurred
    void bookmarksDataEvent(const QnAbstractCameraDataPtr &data
        , int handle);

private:
    typedef std::list<BookmarkRequestHolder> RequestsContainer;
    typedef QHash<QnResourcePtr, QnGenericCameraDataLoader *> LoadersContainer;

    LoadersContainer m_loaders;
    RequestsContainer m_requests;
};

QnCameraBookmarksManager::Impl::Impl(QObject *parent)
    : QObject(parent)
    , m_loaders()
    , m_requests()
{
}

QnCameraBookmarksManager::Impl::~Impl()
{
}

void QnCameraBookmarksManager::Impl::getBookmarksAsync(const FilterParameters &filter
    , const BookmarksCallbackType &callback
    , bool clearBookmarksCache)
{
    BookmarkRequestHolder request = { QnTimePeriod(filter.startTime, filter.finishTime - filter.startTime)
        , callback, QnCameraBookmarkList(), AnswersContainer(), true };

    /// Make Load request for any camera
    for(const auto &camera: filter.cameras)
    {
        LoadersContainer::iterator it = m_loaders.find(camera);
        if (it == m_loaders.end())
        {
            const QnMediaServerResourcePtr mediaServer = camera->getParentResource().dynamicCast<QnMediaServerResource>();
            const QnNetworkResourcePtr networkResource = camera.dynamicCast<QnNetworkResource>();
            if (QnGenericCameraDataLoader *loader = QnGenericCameraDataLoader::newInstance(
                mediaServer, networkResource, Qn::BookmarkData, this))
            {
                connect(loader, &QnAbstractCameraDataLoader::ready, this, &Impl::bookmarksDataEvent);
                connect(loader, &QnAbstractCameraDataLoader::failed, this
                    , [this](int, int handle) { bookmarksDataEvent(QnAbstractCameraDataPtr(), handle); } );

                it = m_loaders.insert(camera, loader);
            } 
            else
            {
                /// If we don't hav such loader and can't create it we should mark request as failed and skip this camera
                request.success = false;
                continue;
            }
        }

        enum { kMinimumResolution = 1 };    /// We should use not zero resolution to 
                                            /// prevent all cashes to be discarded when discardCachedData() called
        QnGenericCameraDataLoader * const loader = it.value();
        
        if (clearBookmarksCache)
            loader->discardCachedData(kMinimumResolution);

        const int handle = loader->load(request.targetPeriod, filter.text, kMinimumResolution);
        request.answers.insert(std::make_pair(loader, handle));
    }

    if (!request.answers.empty())
        m_requests.push_back(request);
}

void QnCameraBookmarksManager::Impl::bookmarksDataEvent(const QnAbstractCameraDataPtr &data
    , int handle)
{
    const QnAbstractCameraDataLoader * const loader = static_cast<QnAbstractCameraDataLoader *>(sender());
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
    }
}

///

QnCameraBookmarksManager::QnCameraBookmarksManager(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

void QnCameraBookmarksManager::getBookmarksAsync(const FilterParameters &filter
    , bool clearBookmarksCache
    , const BookmarksCallbackType &callback)
{
    m_impl->getBookmarksAsync(filter, callback, clearBookmarksCache);
}
