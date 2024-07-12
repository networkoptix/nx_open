#pragma once

#include <cstddef>
#include <memory>

#include "KalmanFilter.h"
#include "Rect.h"

namespace byte_track
{
enum class STrackState {
    New = 0,
    Tracked = 1,
    Lost = 2,
    Removed = 3,
};

class STrack
{
public:
    STrack(const Rect<float>& rect, const float& score, uint64_t timestamp, int objectClass);
    ~STrack();

    const Rect<float>& getRect() const;
    const STrackState& getSTrackState() const;

    const bool& isActivated() const;
    const float& getScore() const;
    const uint64_t& getTrackId() const;
    const uint64_t& getFrameId() const;
    const uint64_t& getStartFrameId() const;
    const uint64_t& getTrackletLength() const;
    const uint64_t getUpdateTimestamp() const { return m_updateTimestamp; }

    void activate(const uint64_t& frame_id, const uint64_t& track_id);
    void reActivate(const STrack &new_track, const uint64_t &frame_id, const int &new_track_id = -1);

    // Predict the track state using a Kalman filter.
    void predict(uint64_t timestamp);
    // Update the track state using a Kalman filter.
    void update(const STrack &new_track, const uint64_t &frame_id);
    int getObjectClass() const { return m_objectClass; }

    void markAsLost();
    void markAsRemoved();

private:
    KalmanFilter kalman_filter_;
    // This vector has [x-box-center, y-box-center, ratio width/height, height, ... their velocities].
    KalmanFilter::StateMean mean_;
    KalmanFilter::StateCov covariance_;

    Rect<float> rect_;
    STrackState state_;

    bool is_activated_;
    float score_;
    uint64_t track_id_;
    uint64_t frame_id_;
    uint64_t start_frame_id_;
    uint64_t tracklet_len_;
    uint64_t m_timestamp = 0;
    uint64_t m_updateTimestamp = 0; // Timestamp when rect_ was updated.

    int m_objectClass = 0;

    void updateRect();
};

using STrackPtr = std::shared_ptr<STrack>;
}
