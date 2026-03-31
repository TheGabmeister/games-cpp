#pragma once

constexpr int WINDOW_DEFAULT_WIDTH  = 500;
constexpr int WINDOW_DEFAULT_HEIGHT = 500;
constexpr const char *WINDOW_TITLE  = "mariokart";
constexpr int VSYNC_ON = 1;

constexpr int SIMULATION_HZ = 60;
constexpr float FIXED_DT = 1.f / SIMULATION_HZ;
constexpr float MAX_FRAME_TIME = 0.1f;
