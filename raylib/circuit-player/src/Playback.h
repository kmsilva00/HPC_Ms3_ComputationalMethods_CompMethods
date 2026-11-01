#pragma once

#include "Circuit.h"

#include <optional>

class PlaybackController {
public:
    explicit PlaybackController(double duration = 0.0);

    void Update(double deltaSeconds);
    void Toggle();
    void Play();
    void Pause();
    void Reset();
    void Seek(double seconds);
    void SetDuration(double seconds);
    void SetSpeed(double speed);
    void CycleSpeed();

    double Time() const { return time_; }
    double Duration() const { return duration_; }
    double Speed() const { return speed_; }
    bool IsPlaying() const { return playing_; }

    // Sample-and-hold: latest sample with timestamp <= currentTime.
    static std::optional<double> ValueAt(
        const std::vector<Measurement>& samples,
        double currentTime);

private:
    double time_ = 0.0;
    double duration_ = 0.0;
    double speed_ = 1.0;
    bool playing_ = false;
    std::size_t speedIndex_ = 2;
    const double speeds_[5] = {0.25, 0.5, 1.0, 2.0, 4.0};
};
