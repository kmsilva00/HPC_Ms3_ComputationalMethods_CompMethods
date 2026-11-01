#include "CsvLoader.h"
#include "Playback.h"
#include "Renderer.h"

#include <exception>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace {
bool PointInRect(Vector2 p, Rectangle r) {
    return CheckCollisionPointRec(p, r);
}

std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
}

int RecordVideo(const CircuitModel& circuit, const std::string& outputPath) {
    constexpr int framesPerSecond = 60;
    if (std::system("ffmpeg -version > /dev/null 2>&1") != 0) {
        std::cerr << "Recording requires FFmpeg installed and available on PATH.\n";
        return 1;
    }
    const std::filesystem::path frameDirectory =
        std::filesystem::temp_directory_path() / "circuit-player-frames";
    std::error_code error;
    std::filesystem::remove_all(frameDirectory, error);
    std::filesystem::create_directories(frameDirectory, error);
    if (error) {
        std::cerr << "Failed to create recording frame directory: "
                  << error.message() << '\n';
        return 1;
    }

    PlaybackController playback(circuit.Duration());
    Renderer renderer;
    renderer.LoadAssets(circuit);
    const int frameCount = static_cast<int>(
        std::ceil(circuit.Duration() * framesPerSecond)) + 1;

    SetTargetFPS(framesPerSecond);
    for (int frame = 0; frame < frameCount; ++frame) {
        playback.Seek(static_cast<double>(frame) / framesPerSecond);
        BeginDrawing();
        ClearBackground(Color{12, 18, 28, 255});
        renderer.Draw(circuit, playback);
        EndDrawing();

        char framePath[64];
        std::snprintf(framePath, sizeof(framePath), "frame_%05d.png", frame);
        TakeScreenshot((frameDirectory / framePath).string().c_str());
    }
    renderer.UnloadAssets();

    const std::string command =
        "ffmpeg -y -loglevel error -framerate 60 -i " +
        ShellQuote((frameDirectory / "frame_%05d.png").string()) +
        " -c:v libx264 -pix_fmt yuv420p " + ShellQuote(outputPath);
    if (std::system(command.c_str()) != 0) {
        std::cerr << "FFmpeg failed. Is ffmpeg installed and available on PATH?\n";
        std::filesystem::remove_all(frameDirectory, error);
        return 1;
    }
    std::filesystem::remove_all(frameDirectory, error);
    std::cout << "Recording written to " << outputPath << '\n';
    return 0;
}
}

int main(int argc, char** argv) {
    CircuitModel circuit;
    try {
        circuit = CsvLoader::Load(
            "data/components.csv",
            "data/connections.csv",
            "data/measurements.csv");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load circuit: " << e.what() << '\n';
        return 1;
    }

    const double duration = circuit.Duration();
    PlaybackController playback(duration);
    const bool recording = argc == 3 && std::string(argv[1]) == "--record";
    if (argc != 1 && !recording) {
        std::cerr << "Usage: " << argv[0] << " [--record output.mp4]\n";
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                   FLAG_MSAA_4X_HINT | (recording ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(1600, 1000, "Circuit Playback");
    SetWindowMinSize(1100, 720);
    if (recording) {
        const int result = RecordVideo(circuit, argv[2]);
        CloseWindow();
        return result;
    }
    SetTargetFPS(60);

    Renderer renderer;
    renderer.LoadAssets(circuit);

    while (!WindowShouldClose()) {
        const double dt = GetFrameTime();

        if (IsKeyPressed(KEY_SPACE)) playback.Toggle();
        if (IsKeyPressed(KEY_HOME)) playback.Reset();
        if (IsKeyPressed(KEY_LEFT)) playback.Seek(playback.Time() - 1.0);
        if (IsKeyPressed(KEY_RIGHT)) playback.Seek(playback.Time() + 1.0);
        if (IsKeyPressed(KEY_UP)) playback.CycleSpeed();
        if (IsKeyPressed(KEY_DOWN)) {
            // Cycle forward until the lower speed is selected.
            const double current = playback.Speed();
            if (current >= 4.0) playback.SetSpeed(2.0);
            else if (current >= 2.0) playback.SetSpeed(1.0);
            else if (current >= 1.0) playback.SetSpeed(0.5);
            else if (current >= 0.5) playback.SetSpeed(0.25);
            else playback.SetSpeed(4.0);
        }

        const int screenH = GetScreenHeight();
        const int panelY = screenH - 190;
        const int controlsX = GetScreenWidth() - 250;
        const int buttonY = 92;
        const Vector2 mouse = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (PointInRect(mouse, {static_cast<float>(controlsX + 18),
                                    static_cast<float>(buttonY), 90, 26})) {
                playback.Toggle();
            } else if (PointInRect(mouse, {static_cast<float>(controlsX + 118),
                                           static_cast<float>(buttonY), 75, 26})) {
                playback.Reset();
            } else if (PointInRect(mouse, {static_cast<float>(controlsX + 18),
                                           132, 115, 26})) {
                playback.CycleSpeed();
            } else {
                const int timelineX = controlsX + 18;
                const int timelineW = 250 - 36;
                const int timelineY = 180;
                if (mouse.y >= timelineY - 12 && mouse.y <= timelineY + 12 &&
                    mouse.x >= timelineX && mouse.x <= timelineX + timelineW) {
                    const double fraction = (mouse.x - timelineX) / timelineW;
                    playback.Seek(fraction * playback.Duration());
                }
            }
        }

        playback.Update(dt);

        BeginDrawing();
        ClearBackground(Color{12, 18, 28, 255});
        renderer.Draw(circuit, playback);
        EndDrawing();
    }

    renderer.UnloadAssets();
    CloseWindow();
    return 0;
}
