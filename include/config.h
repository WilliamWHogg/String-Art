#pragma once

// ─── WiFi Credentials (Station mode) ───────────────────────────────────────
#define WIFI_SSID "VitaminWater"
#define WIFI_PASS "onetwothreefour"

// ─── Pin Assignments ────────────────────────────────────────────────────────
// Turntable stepper (TB6600 STEP/DIR)
#define TT_STEP_PIN 26
#define TT_DIR_PIN 27

// Threader stepper (TB6600 STEP/DIR)
#define TH_STEP_PIN 25
#define TH_DIR_PIN 33

// Threader limit switch (normally-open, wired to GND)
#define LIMIT_SW_PIN 32

// ─── Motor Parameters ───────────────────────────────────────────────────────
#define MOTOR_STEPS_PER_REV 200 // 1.8° per full step

// Turntable
#define TT_GEAR_RATIO_DEFAULT 16     // calibrated: 16-lobe cycloidal (nominal 16:1)
#define TT_MICROSTEPS_DEFAULT 1      // full-step default (1/2/4/8/16/32)
#define TT_SPEED_DEFAULT 1000        // steps/sec
#define TT_ACCEL_DEFAULT 1500        // steps/sec²
#define TT_BACKLASH_DEG_DEFAULT 0.0f // backlash compensation (degrees)

// Threader
#define TH_MICROSTEPS_DEFAULT 16
#define TH_SPEED_DEFAULT 1000       // steps/sec
#define TH_ACCEL_DEFAULT 1000       // steps/sec²
#define TH_HOME_SPEED 30            // steps/sec  (default homing speed)
#define TH_HOME_BACKOFF 10          // steps to back off after switch trigger
#define TH_UP_DEG_DEFAULT 5.0f      // threader "up" position (degrees from home)
#define TH_CENTER_DEG_DEFAULT 16.0f // threader "center" position (degrees from home)
#define TH_DOWN_DEG_DEFAULT 20.0f   // threader "down" position (degrees from home)

// ─── Debug / Execution Defaults ─────────────────────────────────────────────
#define CMD_DELAY_DEFAULT 0.0f // inter-command delay (seconds)
#define SPEED_PCT_DEFAULT 100  // speed multiplier (%)

// ─── Limits ─────────────────────────────────────────────────────────────────
#define MAX_PEGS 1024
#define DEFAULT_PEGS 200
#define MAX_COMMANDS 20000

// ─── Command Sentinels ──────────────────────────────────────────────────────
#define CMD_THREADER_DOWN 0xFFFE
#define CMD_THREADER_UP 0xFFFF
#define CMD_THREADER_CENTER 0xFFFD

// ─── Log Buffer ─────────────────────────────────────────────────────────────
#define LOG_BUF_SIZE 200
#define LOG_MSG_LEN 96
