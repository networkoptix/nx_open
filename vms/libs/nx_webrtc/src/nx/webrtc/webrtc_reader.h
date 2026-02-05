// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <core/resource/resource_media_layout_fwd.h>
#include <nx/media/media_data_packet.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/lockable.h>
#include <nx/utils/thread/mutex.h>

namespace nx::webrtc {

// Is a thread-safe class.
class NX_WEBRTC_API Reader: public std::enable_shared_from_this<Reader>
{
public:
    static constexpr size_t kMaxSize = 100;

    // Not a thread-safe class, but provides an interface to a thread-safe Reader class.
    class NX_WEBRTC_API Iterator
    {
    public:
        friend class Reader;
        enum State
        {
            ok, //< Success.
            again, //< No frames at this moment.
            gone, //< Iterator was invalidated.
            dead, //< Reader object is destroyed.
        };
        struct Result
        {
            State state = State::ok;
            QnAbstractMediaDataPtr media;
        };

    public:
        // Wait.
        Result next(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
        // Don't wait.
        Result tryNext();
        // Drop from Reader, and notify if waiting on this iterator.
        void drop();

    private:
        Iterator(std::weak_ptr<Reader>&& reader, int id);

    private:
        std::weak_ptr<Reader> m_reader;
        int m_id = 0;
    };

public:
    Reader(size_t maxSize = kMaxSize);
    virtual ~Reader();
    std::optional<Iterator> iter();
    void pushFrame(QnAbstractMediaDataPtr media);
    void onKeyframeNeeded(std::function<void()>&& callback);
    void start();
    void stop();
    bool isStarted() const;
    AudioLayoutConstPtr audioLayout() const;
    void setAudioLayout(AudioLayoutConstPtr layout);

private:
    struct IteratorInfo
    {
        std::optional<size_t> position;
        int id = 0;
    };

private:
    Iterator::Result next(int id, std::chrono::milliseconds timeout);
    void drop(int id);
    int emitIteratorId();
    void emitKeyframeIfNeededUnsafe();

private:
    mutable nx::Mutex m_mutex;
    nx::WaitCondition m_wait;
    std::deque<QnAbstractMediaDataPtr> m_queue;
    size_t m_maxSize = 0;
    int m_keyIndex = -1; //< Relative to queue start.
    size_t m_counter = 0; //< Count of dropped items.
    std::vector<IteratorInfo> m_iterators; //< TODO Only for a very limited count of iterators.
    int m_idCounter = 0; //< Use for iterator id emission.
    bool m_hasIdOverflow = false;
    bool m_started = false;
    nx::Lockable<std::function<void()>> m_emitKeyframeCallback;
    nx::utils::ElapsedTimer m_emitKeyframeTimer;
    AudioLayoutConstPtr m_audioLayout;
};

} // namespace nx::webrtc
