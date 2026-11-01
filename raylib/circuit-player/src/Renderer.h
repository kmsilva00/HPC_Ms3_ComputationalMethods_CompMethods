#pragma once

#include "Circuit.h"
#include "Playback.h"

#include <unordered_map>

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    void LoadAssets(const CircuitModel& model);
    void UnloadAssets();
    void Draw(const CircuitModel& model, const PlaybackController& playback);

private:
    std::unordered_map<int, Texture2D> textures_;
    Texture2D switchOn_{};
    Texture2D switchOff_{};
    Font font_{};
    bool hasFont_ = false;
    std::string hoveredComponentId_;
    int iconWidth_ = 64;
    int iconHeight_ = 64;

    Texture2D* TextureFor(ComponentType type);
    void DrawComponent(const Component& component, double time, bool vertical);
    void DrawVoltagePlot(const CircuitModel& model, const PlaybackController& playback,
                         Rectangle bounds);
    void UpdateHoveredComponent(const CircuitModel& model,
                                const PlaybackController& playback,
                                Rectangle bounds);
    void DrawLabel(const char* text, Vector2 position, float size, Color color) const;
    float LabelWidth(const char* text, float size) const;
};
