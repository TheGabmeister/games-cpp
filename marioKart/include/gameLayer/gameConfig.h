#pragma once

constexpr int WINDOW_DEFAULT_WIDTH  = 500;
constexpr int WINDOW_DEFAULT_HEIGHT = 500;
constexpr const char *WINDOW_TITLE  = "mariokart";
constexpr int VSYNC_ON = 1;

constexpr int SIMULATION_HZ = 60;
constexpr float FIXED_DT = 1.f / SIMULATION_HZ;
constexpr float MAX_FRAME_TIME = 0.1f;

constexpr float KART_MAX_SPEED = 18.f;
constexpr float KART_ACCELERATION = 14.f;
constexpr float KART_BRAKE_DECEL = 22.f;
constexpr float KART_REVERSE_MAX_SPEED = 6.f;
constexpr float KART_REVERSE_ACCEL = 8.f;
constexpr float KART_DRAG = 8.f;
constexpr float KART_STEER_RATE = 2.5f;
constexpr float KART_STEER_SPEED_REDUCTION = 0.5f;

constexpr float CAMERA_POSITION_SMOOTH = 5.f;
constexpr float CAMERA_TARGET_SMOOTH = 8.f;
constexpr float CAMERA_LOOK_AHEAD = 4.f;
