#include "Playback.h"

#include <algorithm>

PlaybackController::PlaybackController(double duration) {
    SetDuration(duration);
}

void PlaybackController::Update(double deltaSeconds) {
    if (!playing_ || deltaSeconds <= 0.0) return;
    time_ += deltaSeconds * speed_;
    if (time_ >= duration_) {
        time_ = duration_;
        playing_ = false;
    }
}

void PlaybackController::Toggle() {
    if (playing_) Pause();
    else if (time_ >= duration_) Reset();
    else Play();
}

void PlaybackController::Play() {
    if (duration_ <= 0.0) {
        playing_ = false;
        return;
    }
    if (time_ >= duration_) time_ = 0.0;
    playing_ = true;
}

void PlaybackController::Pause() { playing_ = false; }

void PlaybackController::Reset() {
    time_ = 0.0;
    playing_ = false;
}

void PlaybackController::Seek(double seconds) {
    time_ = std::clamp(seconds, 0.0, duration_);
    if (time_ >= duration_) playing_ = false;
}

void PlaybackController::SetDuration(double seconds) {
    duration_ = std::max(0.0, seconds);
    time_ = std::clamp(time_, 0.0, duration_);
}

void PlaybackController::SetSpeed(double speed) {
    for (std::size_t i = 0; i < 5; ++i) {
        if (speeds_[i] == speed) {
            speedIndex_ = i;
            speed_ = speed;
            return;
        }
    }
    speed_ = std::max(0.01, speed);
}

void PlaybackController::CycleSpeed() {
    speedIndex_ = (speedIndex_ + 1) % 5;
    speed_ = speeds_[speedIndex_];
}

std::optional<double> PlaybackController::ValueAt(
    const std::vector<Measurement>& samples,
    double currentTime)
{
    if (samples.empty() || currentTime < samples.front().timestamp) {
        return std::nullopt;
    }

    const auto it = std::upper_bound(
        samples.begin(), samples.end(), currentTime,
        [](double t, const Measurement& sample) {
            return t < sample.timestamp;
        });

    if (it == samples.begin()) return std::nullopt;
    return std::prev(it)->value;
}
