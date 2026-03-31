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

constexpr float WALL_BOUNCE_SPEED_RETAIN = 0.4f;
constexpr float OFF_ROAD_SPEED_FACTOR = 0.5f;
constexpr float OFF_ROAD_ACCEL_FACTOR = 0.4f;
constexpr float OFF_ROAD_EXTRA_DRAG = 12.f;
constexpr float WRONG_WAY_TIME = 0.5f;

// Drift
constexpr float DRIFT_HOP_DURATION = 0.15f;
constexpr float DRIFT_STEER_RATE = 1.5f;
constexpr float DRIFT_STEER_BIAS = 1.8f;
constexpr float DRIFT_LATERAL_SLIP = 3.f;
constexpr float DRIFT_MIN_SPEED = 4.f;

// Mini-turbo
constexpr float MINI_TURBO_MIN_DRIFT_TIME = 0.8f;
constexpr float MINI_TURBO_DURATION = 0.6f;

// Boost
constexpr float BOOST_EXTRA_SPEED = 6.f;
constexpr float BOOST_PAD_DURATION = 0.5f;

// AI
constexpr float AI_CORNER_LOOK_AHEAD = 8.f;
constexpr float AI_CORNER_SPEED_FACTOR = 0.6f;
constexpr float AI_CORNER_ANGLE_THRESHOLD = 0.3f;
constexpr float AI_STUCK_TIME = 2.f;
constexpr float AI_STUCK_SPEED_THRESHOLD = 0.5f;
constexpr float AI_RUBBER_BAND_DISTANCE = 30.f;
constexpr float AI_RUBBER_BAND_BOOST = 2.5f;
constexpr float AI_RUBBER_BAND_SLOW = 1.5f;
