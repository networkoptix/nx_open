#pragma once

#include "STrack.h"
#include "lapjv.h"
#include "Object.h"

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <vector>
#include <optional>

namespace byte_track
{
using ConfidenceMap = std::unordered_map<int, float>; // ObjectClass to confidence.

struct ByteTrackerConfig
{
    // Timestamp difference if timestamp is not provided
    int64_t defaultDtUs = 1.0E6 / 7.0;
    // Threshold for good and poor detections
    float goodDetectionThreshold = 0.5;
    // Threshold for birth tracks from remained good detections
    float trackBirthThreshold = 0.6;
    // IOU threshold for matching good detections to tracks
    float iouMatchingGoodDetectionsThreshold = 0.8;
    // IOU threshold for matching  poor detections to remained tracks
    float iouMatchingPoorDetectionsThreshold = 0.5;
    // IOU threshold for matching good detections to non active tracks
    float iouMatchingNonActiveTracksThreshold = 0.7;
    // IOU for removing duplicates among active and lost stracks
    float iouRemoveDuplicateThreshold = 0.15;
    int maxTimeSinceUpdateMs = 600;
    int maxTimeWithoutGoodUpdateMs = 10000;

    virtual ~ByteTrackerConfig() = default;
};

class BYTETracker
{
public:
    BYTETracker(const ByteTrackerConfig& config);
    ~BYTETracker();

    std::vector<STrackPtr> update(const std::vector<Object>& objects, std::optional<int64_t> timestampUs = std::nullopt);

    void setConfidences(float defaultTrackBirthConfidence,
        std::shared_ptr<ConfidenceMap> trackBirthConfidences,
        float defaultGoodDetectionConfidence,
        std::shared_ptr<ConfidenceMap> goodDetectionConfidences);
private:
    std::vector<STrackPtr> jointStracks(const std::vector<STrackPtr> &a_tlist,
                                        const std::vector<STrackPtr> &b_tlist) const;

    std::vector<STrackPtr> subStracks(const std::vector<STrackPtr> &a_tlist,
                                      const std::vector<STrackPtr> &b_tlist) const;

    // Remove overlapping from input containers.
    void removeDuplicateStracks(const std::vector<STrackPtr> &a_stracks,
                                const std::vector<STrackPtr> &b_stracks,
                                std::vector<STrackPtr> &a_res,
                                std::vector<STrackPtr> &b_res) const;

    // Solver linear assignment problem to find best association between new detections and existing tracks.
    void linearAssignment(const std::vector<std::vector<float>> &cost_matrix,
                          const int &cost_matrix_size,
                          const int &cost_matrix_size_size,
                          const float &thresh,
                          std::vector<std::vector<int>> &matches,
                          std::vector<int> &b_unmatched,
                          std::vector<int> &a_unmatched) const;

    // Calculate metric IOU (Intersection over Union) for input containers.
    std::vector<std::vector<float>> calcIouDistance(const std::vector<STrackPtr> &a_tracks,
                                                    const std::vector<STrackPtr> &b_tracks,
                                                    bool checkTrackClass=true) const;

    std::vector<std::vector<float>> calcIous(const std::vector<Rect<float>> &a_rect,
                                             const std::vector<Rect<float>> &b_rect) const;

    // Wrapper for Linear Assignment Solver using Jonker-Volgenant algorithm.
    double execLapjv(const std::vector<std::vector<float> > &cost,
                     std::vector<int> &rowsol,
                     std::vector<int> &colsol,
                     bool extend_cost = false,
                     float cost_limit = std::numeric_limits<float>::max(),
                     bool return_cost = true) const;

    bool isGoodDetection(STrackPtr detection);
    bool isBirthDetection(STrackPtr detection);

private:
    ByteTrackerConfig m_config;

    // Object class to confidence;
    std::shared_ptr<ConfidenceMap> m_trackBirthConfidences = std::make_shared<ConfidenceMap>();
    std::shared_ptr<ConfidenceMap> m_goodDetectionConfidences = std::make_shared<ConfidenceMap>();

    uint64_t frame_id_ = 0;
    uint64_t track_id_count_ = 0;
    int64_t m_timestampUs = 0;

    std::vector<STrackPtr> tracked_stracks_;
    std::vector<STrackPtr> lost_stracks_;
};

}
