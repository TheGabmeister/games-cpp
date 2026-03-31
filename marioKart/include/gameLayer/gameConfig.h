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
constexpr float KART_STEER_RATE = 2.8f;
constexpr float KART_STEER_SPEED_REDUCTION = 0.4f;

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
constexpr float MINI_TURBO_MIN_DRIFT_TIME = 0.65f;
constexpr float MINI_TURBO_DURATION = 0.6f;

// Boost
constexpr float BOOST_EXTRA_SPEED = 6.f;
constexpr float BOOST_PAD_DURATION = 0.5f;

// Items
constexpr float ITEM_BOX_RADIUS = 1.5f;
constexpr float ITEM_BOX_RESPAWN_TIME = 4.f;
constexpr float MUSHROOM_BOOST_DURATION = 1.0f;
constexpr float BANANA_HIT_SPEED_MULT = 0.15f;
constexpr float BANANA_SPINOUT_TIME = 0.8f;
constexpr float GREEN_SHELL_SPEED = 25.f;
constexpr float GREEN_SHELL_LIFETIME = 5.f;
constexpr float GREEN_SHELL_RADIUS = 0.6f;
constexpr int GREEN_SHELL_MAX_BOUNCES = 3;
constexpr float RED_SHELL_SPEED = 22.f;
constexpr float RED_SHELL_LIFETIME = 8.f;
constexpr float RED_SHELL_RADIUS = 0.6f;
constexpr float RED_SHELL_HOMING_RATE = 3.5f;
constexpr float KART_HIT_RADIUS = 1.2f;
constexpr float AI_ITEM_USE_DELAY = 2.f;

// Menu
constexpr int KART_PALETTE_COUNT = 8;
constexpr float MENU_KART_SPIN_SPEED = 1.5f;
constexpr float MENU_PULSE_SPEED = 3.f;

// Recovery
constexpr float PLAYER_OOB_MARGIN = 4.f;
constexpr float PLAYER_STUCK_TIME = 3.f;
constexpr float PLAYER_STUCK_SPEED_THRESHOLD = 0.5f;
constexpr float RESPAWN_FREEZE_TIME = 1.f;

// AI
constexpr float AI_CORNER_LOOK_AHEAD = 8.f;
constexpr float AI_CORNER_SPEED_FACTOR = 0.6f;
constexpr float AI_CORNER_ANGLE_THRESHOLD = 0.3f;
constexpr float AI_STUCK_TIME = 2.f;
constexpr float AI_STUCK_SPEED_THRESHOLD = 0.5f;
constexpr float AI_RUBBER_BAND_DISTANCE = 25.f;
constexpr float AI_RUBBER_BAND_BOOST = 2.2f;
constexpr float AI_RUBBER_BAND_SLOW = 1.8f;
