
#include "camera_bookmarks_manager.h"

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/data/bookmark_camera_data.h>
#include <camera/loaders/bookmark_camera_data_loader.h>

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

    const static int defaultSearchLimit = 100;
}

class QnCameraBookmarksManager::Impl : public QObject
{
public:
    Impl(QObject *parent);

    virtual ~Impl();

    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const FilterParameters &filter, int limit, BookmarksCallbackType callback);

private:
    /// @brief Callback for bookmarks. If data is nullptr it means error occurred
    void bookmarksDataEvent(const QnCameraBookmarkList &bookmarks);

private:
    typedef std::list<BookmarkRequestHolder> RequestsContainer;
    typedef QHash<QnResourcePtr, QnBookmarksLoader *> LoadersContainer;

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

void QnCameraBookmarksManager::Impl::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                       , const FilterParameters &filter
                                                       , int limit
                                                       , BookmarksCallbackType callback)
{

    /*
    Here we should do the following:
    1) get list of already loaded bookmarks by this request.
    2) Select correct loader:
       * that may be the 'Search loader' that will support cameras list, text filter and start time only, also will not cache data and drop requests
       * or 'Timeline loader' - single camera, set period, cache data?, wait requests
    
    */



    BookmarkRequestHolder request = { filter.period
        , callback, QnCameraBookmarkList(), AnswersContainer(), true };

    /// Make Load request for any camera
    for(const auto &camera: cameras)
    {
        LoadersContainer::iterator it = m_loaders.find(camera);
        if (it == m_loaders.end())
        {
            QnBookmarksLoader *loader = new QnBookmarksLoader(camera, this);
                connect(loader, &QnBookmarksLoader::bookmarksChanged, this, &Impl::bookmarksDataEvent);
                it = m_loaders.insert(camera, loader);
        }

        QnBookmarksLoader * const loader = it.value();
        
            //TODO: #GDM #Bookmarks IMPLEMENT ME
        loader->load(request.targetPeriod);
      //  request.answers.insert(std::make_pair(loader, handle));
    }

    if (!request.answers.empty())
        m_requests.push_back(request);
}

void QnCameraBookmarksManager::Impl::bookmarksDataEvent(const QnCameraBookmarkList &bookmarks)
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

///

QnCameraBookmarksManager::QnCameraBookmarksManager(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

void QnCameraBookmarksManager::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                 , const FilterParameters &filter
                                                 , int limit
                                                 , BookmarksCallbackType callback)
{
    m_impl->getBookmarksAsync(cameras, filter, limit, callback);
}

QnCameraBookmarksManager::FilterParameters::FilterParameters():
    period(0, QnTimePeriod::infiniteDuration()),
    limit(defaultSearchLimit),
    strategy(Qn::EarliestFirst)
{

}
