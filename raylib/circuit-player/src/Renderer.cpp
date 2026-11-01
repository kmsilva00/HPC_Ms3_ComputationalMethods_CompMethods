#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>

namespace {
constexpr Color kGrid{42, 42, 42, 255};
constexpr Color kText{220, 220, 220, 255};
constexpr Color kMutedText{160, 160, 160, 255};
constexpr Color kWire{130, 130, 130, 255};
constexpr Color kParticle{255, 215, 45, 255};
constexpr int kControlPanelHeight = 190;
constexpr int kControlSidebarWidth = 250;

int Key(ComponentType type) { return static_cast<int>(type); }

std::string FormatValue(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}

std::string FormatComponentValue(const Component& component, double value) {
    if (component.type == ComponentType::Gate) {
        return value >= 0.5 ? "1" : "0";
    }
    return FormatValue(value);
}

Vector2 CenterOf(const Component& component, int w, int h) {
    return {component.position.x + w / 2.0f, component.position.y + h / 2.0f};
}

bool IsVerticalComponent(const CircuitModel& model, const Component& component) {
    if (component.type == ComponentType::PowerIn ||
        component.type == ComponentType::Resistor) {
        return true;
    }
    for (const auto& connection : model.connections) {
        std::string neighborId;
        if (connection.from == component.id) neighborId = connection.to;
        else if (connection.to == component.id) neighborId = connection.from;
        else continue;
        const auto neighborIt = model.componentIndex.find(neighborId);
        if (neighborIt == model.componentIndex.end()) continue;
        const auto& neighbor = model.components[neighborIt->second];
        if (std::abs(neighbor.position.y - component.position.y) >
            std::abs(neighbor.position.x - component.position.x)) {
            return true;
        }
    }
    return false;
}

Vector2 PortOf(const Component& component, bool output, bool vertical,
               int w, int h) {
    if (component.type == ComponentType::Junction) {
        return CenterOf(component, w, h);
    }
    if (vertical) {
        return {component.position.x + w / 2.0f,
                component.position.y + (output ? h : 0.0f)};
    }
    return {component.position.x + (output ? w : 0.0f),
            component.position.y + h / 2.0f};
}

Vector2 PointAlongPath(const std::vector<Vector2>& path, float distance,
                       Vector2& direction) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        const Vector2 delta{path[i].x - path[i - 1].x,
                            path[i].y - path[i - 1].y};
        const float segmentLength = std::sqrt(
            delta.x * delta.x + delta.y * delta.y);
        if (segmentLength <= 0.0f) continue;
        if (distance <= segmentLength || i == path.size() - 1) {
            direction = {delta.x / segmentLength, delta.y / segmentLength};
            const float fraction = std::clamp(distance / segmentLength, 0.0f, 1.0f);
            return {path[i - 1].x + delta.x * fraction,
                    path[i - 1].y + delta.y * fraction};
        }
        distance -= segmentLength;
    }
    direction = {0.0f, 0.0f};
    return path.back();
}

std::vector<Vector2> OutsideInputPath(Vector2 from, Vector2 to) {
    return {from, {from.x, to.y}, to};
}

std::vector<Vector2> ResistorPath(Vector2 from, Vector2 to) {
    return {from, {to.x, from.y}, to};
}

std::optional<double> VoltageAt(
    const CircuitModel& model, const std::string& id, double time,
    std::unordered_set<std::string>& visited) {
    if (!visited.insert(id).second) return std::nullopt;
    const auto componentIt = model.componentIndex.find(id);
    if (componentIt == model.componentIndex.end()) return std::nullopt;
    const Component& component = model.components[componentIt->second];
    const auto ownValue = PlaybackController::ValueAt(component.measurements, time);
    if (ownValue.has_value()) return ownValue;
    if (component.type != ComponentType::Junction) return std::nullopt;

    for (const auto& connection : model.connections) {
        if (connection.to == id) {
            auto upstream = VoltageAt(model, connection.from, time, visited);
            if (upstream.has_value()) return upstream;
        }
    }
    return std::nullopt;
}
}

void Renderer::LoadAssets(const CircuitModel& model) {
    UnloadAssets();
    if (FileExists("icons/switch_on.png")) switchOn_ = LoadTexture("icons/switch_on.png");
    if (FileExists("icons/switch_off.png")) switchOff_ = LoadTexture("icons/switch_off.png");
    const char* fontPaths[] = {
        "assets/Calibri.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Calibri.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/calibri.ttf"
    };
    for (const char* path : fontPaths) {
        if (FileExists(path)) {
            font_ = LoadFontEx(path, 32, nullptr, 0);
            hasFont_ = font_.texture.id != 0;
            if (hasFont_) break;
        }
    }
    for (const auto& component : model.components) {
        const int key = Key(component.type);
        if (textures_.count(key)) continue;

        const auto& definition = DefinitionFor(component.type);
        if (FileExists(definition.iconPath)) {
            Texture2D texture = LoadTexture(definition.iconPath);
            if (texture.id != 0) textures_[key] = texture;
        }
    }
}

void Renderer::UnloadAssets() {
    for (auto& item : textures_) UnloadTexture(item.second);
    textures_.clear();
    if (switchOn_.id != 0) UnloadTexture(switchOn_);
    if (switchOff_.id != 0) UnloadTexture(switchOff_);
    switchOn_ = {};
    switchOff_ = {};
    if (hasFont_) UnloadFont(font_);
    font_ = {};
    hasFont_ = false;
}

Texture2D* Renderer::TextureFor(ComponentType type) {
    auto it = textures_.find(Key(type));
    return it == textures_.end() ? nullptr : &it->second;
}

void Renderer::DrawLabel(const char* text, Vector2 position, float size,
                         Color color) const {
    if (hasFont_) DrawTextEx(font_, text, position, size, 1.0f, color);
    else DrawText(text, static_cast<int>(position.x), static_cast<int>(position.y),
                  static_cast<int>(size), color);
}

float Renderer::LabelWidth(const char* text, float size) const {
    return hasFont_ ? MeasureTextEx(font_, text, size, 1.0f).x
                    : static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

void Renderer::UpdateHoveredComponent(const CircuitModel& model,
                                      const PlaybackController& playback,
                                      Rectangle bounds) {
    const Vector2 mouse = GetMousePosition();
    int traceIndex = 0;
    const float duration = static_cast<float>(std::max(0.01, playback.Duration()));
    const float chartTop = bounds.y + 28.0f;
    const float chartBottom = bounds.y + bounds.height - 22.0f;
    const float chartHeight = chartBottom - chartTop;
    const float maxVoltage = 10.0f;

    for (const auto& component : model.components) {
        if (component.measurements.empty()) continue;
        const float legendY = bounds.y + 7.0f + traceIndex * 14.0f;
        if (mouse.x >= bounds.x + bounds.width - 55.0f &&
            mouse.x <= bounds.x + bounds.width - 4.0f &&
            mouse.y >= legendY - 2.0f && mouse.y <= legendY + 13.0f) {
            hoveredComponentId_ = component.id;
            return;
        }

        const float normalizedTime = (mouse.x - bounds.x - 34.0f) /
            (bounds.width - 44.0f);
        if (normalizedTime >= 0.0f && normalizedTime <= 1.0f &&
            mouse.y >= chartTop && mouse.y <= chartBottom) {
            const auto value = PlaybackController::ValueAt(
                component.measurements, normalizedTime * duration);
            if (value.has_value()) {
                const float normalized = std::clamp(
                    static_cast<float>(*value) / maxVoltage, 0.0f, 1.0f);
                const float traceY = chartBottom - normalized * chartHeight;
                if (std::abs(mouse.y - traceY) < 10.0f) {
                    hoveredComponentId_ = component.id;
                    return;
                }
            }
        }
        ++traceIndex;
    }
}

void Renderer::DrawVoltagePlot(const CircuitModel& model,
                               const PlaybackController& playback,
                               Rectangle bounds) {
    DrawRectangleLinesEx(bounds, 1.0f, Color{105, 105, 105, 255});
    DrawLabel("Voltage history", {bounds.x + 10, bounds.y + 6}, 16, kText);

    const float chartTop = bounds.y + 28.0f;
    const float chartBottom = bounds.y + bounds.height - 22.0f;
    const float chartHeight = chartBottom - chartTop;
    const float duration = static_cast<float>(std::max(0.01, playback.Duration()));
    const float maxVoltage = 10.0f;
    DrawLine(static_cast<int>(bounds.x + 34), static_cast<int>(chartTop),
             static_cast<int>(bounds.x + 34), static_cast<int>(chartBottom),
             Color{95, 95, 95, 255});
    DrawLine(static_cast<int>(bounds.x + 34), static_cast<int>(chartBottom),
             static_cast<int>(bounds.x + bounds.width - 10), static_cast<int>(chartBottom),
             Color{95, 95, 95, 255});
    DrawLabel("10 V", {bounds.x + 2, chartTop - 5}, 11, kMutedText);
    DrawLabel("0 V", {bounds.x + 8, chartBottom - 6}, 11, kMutedText);

    const Color traceColors[] = {
        {245, 245, 245, 255}, {180, 180, 180, 255},
        {235, 190, 65, 255}, {135, 135, 135, 255},
        {205, 205, 205, 255}, {110, 110, 110, 255}
    };
    int hoveredTrace = -1;
    int traceIndex = 0;
    for (const auto& component : model.components) {
        if (component.measurements.empty()) continue;
        if (component.id == hoveredComponentId_) hoveredTrace = traceIndex;
        const bool highlighted = hoveredTrace == traceIndex ||
            component.id == hoveredComponentId_;
        Color color = traceColors[traceIndex % (sizeof(traceColors) / sizeof(traceColors[0]))];
        if (!highlighted) color.a = 105;
        Vector2 previous{};
        bool hasPrevious = false;
        constexpr int sampleCount = 80;
        for (int i = 0; i <= sampleCount; ++i) {
            const float time = duration * i / sampleCount;
            const auto value = PlaybackController::ValueAt(component.measurements, time);
            if (!value.has_value()) continue;
            const float x = bounds.x + 34.0f +
                (bounds.width - 44.0f) * time / duration;
            const float normalized = std::clamp(
                static_cast<float>(*value) / maxVoltage, 0.0f, 1.0f);
            const Vector2 point{x, chartBottom - normalized * chartHeight};
            if (hasPrevious) DrawLineEx(previous, point, highlighted ? 3.5f : 1.5f, color);
            previous = point;
            hasPrevious = true;
        }
        DrawLabel(component.id.c_str(),
                  {bounds.x + bounds.width - 45.0f,
                   bounds.y + 7.0f + traceIndex * 14.0f},
                  11, highlighted ? kParticle : color);
        ++traceIndex;
    }

    const float currentX = bounds.x + 34.0f +
        (bounds.width - 44.0f) * static_cast<float>(playback.Time()) / duration;
    DrawLine(static_cast<int>(currentX), static_cast<int>(chartTop),
             static_cast<int>(currentX), static_cast<int>(chartBottom),
             Color{255, 215, 45, 150});
}

void Renderer::DrawComponent(const Component& component, double time, bool vertical) {
    if (component.type == ComponentType::Junction) {
        const Vector2 center{component.position.x + iconWidth_ / 2.0f,
                             component.position.y + iconHeight_ / 2.0f};
        const bool highlighted = component.id == hoveredComponentId_;
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        highlighted ? 7.0f : 5.0f,
                        highlighted ? kParticle : Color{190, 190, 190, 255});
        return;
    }

    const auto& definition = DefinitionFor(component.type);
    Texture2D* texture = TextureFor(component.type);
    if (component.type == ComponentType::Gate) {
        const auto state = PlaybackController::ValueAt(component.measurements, time);
        Texture2D* selected = state.has_value() && *state >= 0.5
            ? &switchOn_ : &switchOff_;
        texture = selected->id != 0 ? selected : nullptr;
    }
    const int w = iconWidth_;
    const int h = iconHeight_;
    const Rectangle bounds{
        component.position.x, component.position.y,
        static_cast<float>(w), static_cast<float>(h)
    };
    const bool highlighted = component.id == hoveredComponentId_;

    if (texture) {
        const Rectangle source{0.0f, 0.0f,
                               static_cast<float>(texture->width),
                               static_cast<float>(texture->height)};
        DrawRectangleRounded(bounds, 0.15f, 6, Color{225, 225, 225, 255});
        const float sourceAspect = source.width / source.height;
        const float boxAspect = bounds.width / bounds.height;
        float drawWidth = bounds.width;
        float drawHeight = bounds.height;
        if (sourceAspect > boxAspect) {
            drawHeight = drawWidth / sourceAspect;
        } else {
            drawWidth = drawHeight * sourceAspect;
        }
        const Rectangle destination{
            bounds.x + bounds.width / 2.0f,
            bounds.y + bounds.height / 2.0f,
            drawWidth, drawHeight
        };
        DrawTexturePro(*texture, source, destination,
                       {drawWidth / 2.0f, drawHeight / 2.0f},
                       vertical ? 90.0f : 0.0f, WHITE);
        if (highlighted) {
            DrawRectangleLinesEx(bounds, 3.0f, kParticle);
        }
    } else {
        DrawRectangleRounded(bounds, 0.15f, 6,
                             highlighted ? Color{92, 82, 42, 255}
                                         : Color{225, 225, 225, 255});
        DrawRectangleRoundedLinesEx(bounds, 0.15f, 6,
                                    2.0f, highlighted ? kParticle
                                                      : Color{150, 150, 150, 255});
        const char* fallback = definition.displayName;
        DrawLabel(fallback, {bounds.x + 4, bounds.y + 22}, 10, kText);
    }

    DrawLabel(component.id.c_str(),
              {component.position.x, component.position.y - 20}, 16, kText);

    const auto value = PlaybackController::ValueAt(component.measurements, time);
    std::string label = "—";
    if (value.has_value()) {
        label = FormatComponentValue(component, *value);
        if (definition.unit && definition.unit[0] != '\0') {
            label += " ";
            label += definition.unit;
        }
    }

    const bool isVoltage = definition.unit &&
        std::string(definition.unit) == "V";
    if (isVoltage) {
        const float outputX = component.position.x + w + 12.0f;
        const float unitX = outputX + 58.0f;
        std::string numeric = value.has_value() ? FormatValue(*value) : "—";
        const float numericWidth = LabelWidth(numeric.c_str(), 18);
        DrawLabel(numeric.c_str(),
                  {outputX + 48.0f - numericWidth,
                   component.position.y - 2.0f},
                  18, kText);
        DrawLabel(definition.unit,
                  {unitX, component.position.y - 2.0f}, 18, kText);
    } else {
        const float textWidth = LabelWidth(label.c_str(), 18);
        const Vector2 labelPosition{
            component.position.x + w / 2.0f - textWidth / 2.0f,
            component.position.y + h + 12.0f};
        DrawLabel(label.c_str(), labelPosition, 18, kText);
    }
}

void Renderer::Draw(const CircuitModel& model, const PlaybackController& playback) {
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    hoveredComponentId_.clear();
    const Vector2 mouse = GetMousePosition();
    for (const auto& component : model.components) {
        const Rectangle bounds{component.position.x, component.position.y,
                               static_cast<float>(iconWidth_),
                               static_cast<float>(iconHeight_)};
        if (CheckCollisionPointRec(mouse, bounds)) {
            hoveredComponentId_ = component.id;
            break;
        }
    }
    if (hoveredComponentId_.empty()) {
        UpdateHoveredComponent(model, playback,
                               {20.0f, static_cast<float>(screenH - kControlPanelHeight + 12),
                                static_cast<float>(screenW - 40), 166.0f});
    }
    for (int x = 0; x < screenW; x += 32) DrawLine(x, 0, x, screenH, kGrid);
    for (int y = 0; y < screenH; y += 32) DrawLine(0, y, screenW, y, kGrid);

    // Connections are drawn first so component icons appear above the wires.
    for (const auto& connection : model.connections) {
        const auto fromIt = model.componentIndex.find(connection.from);
        const auto toIt = model.componentIndex.find(connection.to);
        if (fromIt == model.componentIndex.end() || toIt == model.componentIndex.end()) continue;

        const Component& from = model.components[fromIt->second];
        const Component& to = model.components[toIt->second];
        const bool fromVertical = IsVerticalComponent(model, from);
        const bool toVertical = IsVerticalComponent(model, to);
        const Vector2 a = PortOf(from, true, fromVertical, iconWidth_, iconHeight_);
        const Vector2 b = PortOf(to, false, toVertical, iconWidth_, iconHeight_);
        const bool returnWire = connection.to == "VIN";
        const float busY = static_cast<float>(
            GetScreenHeight() - kControlPanelHeight - 85);
        const std::vector<Vector2> path = returnWire
            ? std::vector<Vector2>{{a.x, a.y}, {a.x, busY},
                                   {b.x, busY}, {b.x, b.y}}
            : (connection.from == "VIN" && connection.to == "L1"
                ? OutsideInputPath(a, b)
                : (connection.from == "J2" && connection.to == "LOAD"
                    ? ResistorPath(a, b)
                    : std::vector<Vector2>{a, b}));

        if (returnWire) {
            for (std::size_t i = 1; i < path.size(); ++i) {
                DrawLineEx(path[i - 1], path[i], 3.0f, kWire);
            }
        } else {
            for (std::size_t i = 1; i < path.size(); ++i) {
                DrawLineEx(path[i - 1], path[i], 3.0f, kWire);
            }
        }
        DrawCircleV(a, 4.0f, Color{80, 80, 80, 255});
        DrawCircleV(b, 4.0f, Color{80, 80, 80, 255});

        std::unordered_set<std::string> visited;
        const auto voltage = VoltageAt(model, from.id, playback.Time(), visited);
        const float volts = voltage.has_value()
            ? static_cast<float>(std::abs(*voltage)) : 0.0f;
        const float particleSpeed = std::clamp(volts / 5.0f, 0.12f, 2.5f);
        float pathLength = 0.0f;
        for (std::size_t i = 1; i < path.size(); ++i) {
            const Vector2 delta{path[i].x - path[i - 1].x,
                                path[i].y - path[i - 1].y};
            pathLength += std::sqrt(delta.x * delta.x + delta.y * delta.y);
        }
        if (pathLength > 0.0f && voltage.has_value() && volts > 0.0f) {
            for (int i = 0; i < 5; ++i) {
                const float distance = std::fmod(
                    static_cast<float>(playback.Time() * particleSpeed * 70.0) +
                    pathLength * static_cast<float>(i) / 5.0f, pathLength);
                Vector2 direction{};
                const Vector2 particle = PointAlongPath(path, distance, direction);
                DrawCircleV(particle, 4.0f, kParticle);
                DrawCircleV({particle.x - direction.x * 5.0f,
                             particle.y - direction.y * 5.0f}, 2.0f,
                            Color{205, 205, 205, 100});
            }
        }
    }

    for (const auto& component : model.components) {
        DrawComponent(component, playback.Time(),
                      IsVerticalComponent(model, component));
    }

    const int panelY = screenH - kControlPanelHeight;
    DrawRectangle(0, panelY, screenW, kControlPanelHeight, Color{28, 28, 28, 255});
    DrawLine(0, panelY, screenW, panelY, Color{100, 100, 100, 255});
    const int controlsX = screenW - kControlSidebarWidth;
    DrawRectangle(controlsX, 0, kControlSidebarWidth, panelY,
                  Color{28, 28, 28, 235});
    DrawLine(controlsX, 0, controlsX, panelY, Color{100, 100, 100, 255});

    char status[160];
    std::snprintf(status, sizeof(status), "%s   %.2f / %.2f s",
        playback.IsPlaying() ? "Playing" : "Paused",
        playback.Time(), playback.Duration());
    DrawLabel(status, {controlsX + 18.0f, 28.0f}, 18, kText);

    DrawLabel("Space: Play/Pause   Home: Reset   Left/Right: -/+1s   Up/Down: Speed",
              {controlsX + 18.0f, 58.0f}, 13, kMutedText);

    const int buttonY = 92;
    DrawRectangle(controlsX + 18, buttonY, 90, 26, Color{70, 70, 70, 255});
    DrawLabel(playback.IsPlaying() ? "Pause" : "Play",
              {controlsX + 41.0f, static_cast<float>(buttonY + 5)}, 16, kText);
    DrawRectangle(controlsX + 118, buttonY, 75, 26, Color{70, 70, 70, 255});
    DrawLabel("Reset", {controlsX + 136.0f, static_cast<float>(buttonY + 5)}, 16, kText);

    char speedText[32];
    std::snprintf(speedText, sizeof(speedText), "Speed: %.2fx", playback.Speed());
    DrawRectangle(controlsX + 18, 132, 115, 26, Color{70, 70, 70, 255});
    DrawLabel(speedText, {controlsX + 26.0f, 137.0f}, 15, kText);

    const int timelineX = controlsX + 18;
    const int timelineW = kControlSidebarWidth - 36;
    const int timelineY = 180;
    DrawLineEx({static_cast<float>(timelineX), static_cast<float>(timelineY)},
               {static_cast<float>(timelineX + timelineW), static_cast<float>(timelineY)},
               4.0f, Color{100, 100, 100, 255});

    const double duration = playback.Duration();
    const float fraction = duration > 0.0
        ? static_cast<float>(playback.Time() / duration) : 0.0f;
    const float knobX = timelineX + fraction * timelineW;
    DrawLineEx({static_cast<float>(timelineX), static_cast<float>(timelineY)},
               {knobX, static_cast<float>(timelineY)}, 4.0f, kParticle);
    DrawCircle(static_cast<int>(knobX), timelineY, 7.0f, kParticle);

    DrawVoltagePlot(model, playback,
                    {20.0f, static_cast<float>(panelY + 12),
                     static_cast<float>(screenW - 40), 166.0f});
}
