
#include "camera_bookmarks_manager.h"

#include <limits>

#include <core/resource/camera_bookmark.h>
#include <camera/loaders/generic_camera_data_loader.h>

class QnCameraBookmarksManager::Impl : public QObject
{
public:
    Impl(QObject *parent);

    virtual ~Impl();

    void getBookmarksAsync(const FilterParameters &filter
        , const BookmarksCallbackType &callback);

private:

private:
    typedef int RequestIdType, HandleType;
    typedef std::map<QnAbstractCameraDataLoader *, HandleType> AnswersContainer;
    typedef std::map<RequestIdType, AnswersContainer> RequestsContainer;
    typedef QHash<QnResourcePtr, QnGenericCameraDataLoader *> LoadersContainer;

    LoadersContainer m_loaders;
    RequestsContainer m_requests;
    RequestIdType m_requestIdCounter;
};

QnCameraBookmarksManager::Impl::Impl(QObject *parent)
    : QObject(parent)
    , m_loaders()
    , m_requests()
    , m_requestIdCounter(0)
{
}

QnCameraBookmarksManager::Impl::~Impl() {}

void QnCameraBookmarksManager::Impl::getBookmarksAsync(const FilterParameters &filter
    , const BookmarksCallbackType &callback)
{
    const RequestIdType requestId = ++m_requestIdCounter;

    auto insertionResult = m_requests.insert(std::make_pair(requestId, AnswersContainer()));
    AnswersContainer &answers = insertionResult.first->second;
    for(const auto &camera: filter.cameras)
    {
        LoadersContainer::iterator it = m_loaders.find(camera);
        if (it == m_loaders.end())
        {
            if (QnGenericCameraDataLoader *loader = QnGenericCameraDataLoader::newInstance(m_server, camera, Qn::BookmarkData, this))
            {
                static const QnTimePeriod largestBoundingPeriod(0, std::numeric_limits<qint64>::max());
                connect(loader, QnGenericCameraDataLoader::ready)
                it = m_loaders.insert(camera, loader);
            }
            else
            {
                continue;
            }
        }

        enum { kMinimumResolution = 0 };
        QnGenericCameraDataLoader * const loader = it.value();
        const int handle = loader->load(QnTimePeriod(filter.startTime, filter.finishTime), filter.text, kMinimumResolution);
        answers.insert(std::make_pair(loader, handle));
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
    , const BookmarksCallbackType &callback)
{
    m_impl->getBookmarksAsync(filter, callback);
}
