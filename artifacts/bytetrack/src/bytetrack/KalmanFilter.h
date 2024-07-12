#pragma once

#include "Eigen/Dense"

#include "Rect.h"

namespace byte_track
{
class KalmanFilter
{
public:
    using DetectBox = Xyah<float>;

    using StateMean = Eigen::Matrix<float, 1, 8, Eigen::RowMajor>;
    using StateCov = Eigen::Matrix<float, 8, 8, Eigen::RowMajor>;

    using StateHMean = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;
    using StateHCov = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

    KalmanFilter(const float& std_weight_position = 1. / 20,
                 const float& std_weight_velocity = 1. / 160);

    void initiate(StateMean& mean, StateCov& covariance, const DetectBox& measurement);

    // Predict state of the object based on motion model. Also update estimation of errors.
    void predict(StateMean& mean, StateCov& covariance);

    // Update state of the object based on new measurement. Also update estimation of errors.
    void update(StateMean& mean, StateCov& covariance, const DetectBox& measurement);

private:
    float std_weight_position_ = 0.0;
    float std_weight_velocity_ = 0.0;

    Eigen::Matrix<float, 8, 8, Eigen::RowMajor> motion_mat_;
    Eigen::Matrix<float, 4, 8, Eigen::RowMajor> update_mat_;

    void project(StateHMean &projected_mean, StateHCov &projected_covariance,
                 const StateMean& mean, const StateCov& covariance);
};
}