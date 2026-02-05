// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_reader.h"

namespace nx::webrtc {

Reader::Iterator::Iterator(std::weak_ptr<Reader>&& reader, int id):
    m_reader(reader), m_id(id)
{
}

Reader::Iterator::Result Reader::Iterator::tryNext()
{
    return next(std::chrono::milliseconds::zero());
}

Reader::Iterator::Result Reader::Iterator::next(std::chrono::milliseconds timeout)
{
    auto reader = m_reader.lock();
    if (!reader)
        return {State::dead, QnAbstractMediaDataPtr()};

    return reader->next(m_id, timeout);
}

void Reader::Iterator::drop()
{
    auto reader = m_reader.lock();
    if (!reader)
        return;
    reader->drop(m_id);
}

Reader::Reader(size_t maxSize): m_maxSize(maxSize)
{
    NX_ASSERT(m_maxSize != 0);
}

Reader::~Reader()
{
    NX_ASSERT(!isStarted());
}

AudioLayoutConstPtr Reader::audioLayout() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_audioLayout;
}

void Reader::setAudioLayout(AudioLayoutConstPtr layout)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_audioLayout = layout;
}

void Reader::start()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_started = true;
}

void Reader::stop()
{
    *m_emitKeyframeCallback.lock() = nullptr;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_started = false;
        m_wait.wakeAll();
    }
}

bool Reader::isStarted() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_started;
}

void Reader::onKeyframeNeeded(std::function<void()>&& emitCallback)
{
    auto callback = m_emitKeyframeCallback.lock();
    *callback = std::move(emitCallback);
}

void Reader::emitKeyframeIfNeededUnsafe()
{
    if (!m_started)
        return;
    constexpr std::chrono::milliseconds kEmitTimeout{200};
    if (!m_emitKeyframeTimer.hasExpired(kEmitTimeout))
        return;
    m_emitKeyframeTimer.restart();

    if (auto callback = m_emitKeyframeCallback.lock(); *callback)
        (*callback)();
}

std::optional<Reader::Iterator> Reader::iter()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto id = emitIteratorId();
    if (!NX_ASSERT(id != -1))
        return std::nullopt;
    std::optional<size_t> position;
    if (m_keyIndex != -1)
       *position = m_counter + m_keyIndex;
    else
       emitKeyframeIfNeededUnsafe();

    m_iterators.emplace_back(IteratorInfo{position, id});
    return Reader::Iterator(weak_from_this(), id);
}

int Reader::emitIteratorId()
{
    if (m_iterators.size() == std::numeric_limits<decltype(m_idCounter)>::max())
    {
        return -1; //< Iterators limit count reached.
    }
    if (m_hasIdOverflow)
    {
        while (true)
        {
            if (std::find_if(m_iterators.begin(), m_iterators.end(),
                [this](const auto& info)
                {
                    return info.id == m_idCounter;
                })
                == m_iterators.end())
            {
                break;
            }
            ++m_idCounter;
        }
    }
    const int result = m_idCounter;
    if (m_idCounter == std::numeric_limits<decltype(m_idCounter)>::max())
    {
        m_idCounter = 0;
        m_hasIdOverflow = true;
    }
    else
    {
        ++m_idCounter;
    }
    return result;
}

void Reader::pushFrame(QnAbstractMediaDataPtr media)
{
    if (!media)
        return;
    NX_MUTEX_LOCKER lock(&m_mutex);
    const bool isKeyframe = media->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
    m_queue.emplace_back(std::move(media));
    if (m_queue.size() > m_maxSize)
    {
        m_queue.pop_front();
        for (auto it = m_iterators.begin(); it != m_iterators.end();)
        {
            if (it->position && *(it->position) == m_counter)
                it = m_iterators.erase(it);
            else
                ++it;
        }
        if (m_keyIndex == 0)
            m_keyIndex = -1;
        else if (m_keyIndex != -1)
            --m_keyIndex;
        ++m_counter;
    }
    if (isKeyframe)
        m_keyIndex = m_queue.size() - 1;

    m_wait.wakeAll();
}

Reader::Iterator::Result Reader::next(int id, std::chrono::milliseconds timeout)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    decltype(m_iterators)::iterator iter;

    while (true)
    {
        if (!m_started) //< TODO Maybe report dead only on last frame.
            return Reader::Iterator::Result{Iterator::State::dead, QnAbstractMediaDataPtr()};

        iter = std::find_if(m_iterators.begin(), m_iterators.end(),
            [id](const auto& info)
            {
                return info.id == id;
           });
        if (iter == m_iterators.end())
            return Iterator::Result{Iterator::State::gone, QnAbstractMediaDataPtr()};

        bool needWait = (!iter->position && m_keyIndex == -1)
            || (iter->position && *iter->position == m_counter + m_queue.size());
        if (needWait)
        {
            if (!iter->position)
                emitKeyframeIfNeededUnsafe();
            timeout = duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (timeout <= std::chrono::milliseconds::zero())
                return Reader::Iterator::Result{Iterator::State::again, QnAbstractMediaDataPtr()};
            m_wait.wait(&m_mutex, timeout.count());
            continue;
        }
        else
        {
            break;
        }
    }
    // `iter` wouldn't be initialized here.
    if (!iter->position)
    {
        NX_ASSERT(m_keyIndex != -1);
        iter->position = m_counter + m_keyIndex;
    }
    size_t positionInQueue = *iter->position - m_counter;
    NX_ASSERT(positionInQueue < m_queue.size());
    ++*iter->position;

    return Reader::Iterator::Result{Iterator::State::ok, m_queue[positionInQueue]};
}

void Reader::drop(int id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = std::find_if(m_iterators.begin(), m_iterators.end(),
        [id](const auto& info)
        {
            return info.id == id;
        });
    if (iter == m_iterators.end())
        return;

    m_iterators.erase(iter);
    m_wait.wakeAll();
}

} // namespace nx::webrtc
