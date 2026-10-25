// Merged firmware
#include <Arduino.h>
#include <stdarg.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <LovyanGFX.hpp>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <USB.h>
#include <USBMIDI.h>

#include <driver/i2s.h>
#include <esp_heap_caps.h>

// board
#pragma once
// ===== Hardware map =====
#include <Arduino.h>

// ---- Audio (PCM5102A, I2S) ----
#define AUDIO_SR     44100
#define AUDIO_BLOCK  128
#define MAX_VOICES   16
constexpr int PIN_I2S_BCLK = 11, PIN_I2S_LRCK = 12, PIN_I2S_DOUT = 13;

// ---- I2C / MCP23017 ----
constexpr int PIN_I2C_SDA = 1, PIN_I2C_SCL = 2, PIN_MCP_INT = 6;
constexpr uint8_t MCP_ADDR = 0x20;
#define ENC_STEPS_PER_DETENT 4
#define ENC_INVERT           0
constexpr uint8_t ENCODER_DIR_INVERT_MASK = 0b00000;

// ---- microSD ----
constexpr int PIN_SD_MISO = 3, PIN_SD_MOSI = 4, PIN_SD_CS = 5, PIN_SD_SCLK = 8;

// ---- ST7789 ----
constexpr int PIN_TFT_CS = 7, PIN_TFT_DC = 15, PIN_TFT_RST = 16, PIN_TFT_MOSI = 17, PIN_TFT_SCLK = 18;
constexpr int TFT_WIDTH = 320;
constexpr int TFT_HEIGHT = 170;
constexpr int TFT_COL_OFFSET = 0;
constexpr int TFT_ROW_OFFSET = 35;
constexpr uint8_t TFT_ROTATION = 1;

// ---- SK6812 ----
constexpr int PIN_LED_DATA = 21;
#define LED_COUNT 35
#define LED_MAX_BRIGHTNESS 90

constexpr uint8_t kMacroPadLedIndex[4] = {0, 1, 2, 3};
constexpr uint8_t kNoteKeyLedIndex[26]  = {4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
constexpr uint8_t kEncoderLedIndex[5]  = {30, 31, 32, 33, 34};

constexpr int NUM_ROWS = 5, NUM_COLS = 6, NUM_KEYS = 30, NUM_NOTE_KEYS = 26;
constexpr uint8_t ROW_PINS[NUM_ROWS] = {9, 10, 14, 38, 39};
constexpr uint8_t COL_PINS[NUM_COLS] = {40, 41, 42, 47, 48, 45};
#define MATRIX_SETTLE_US 10
#define KEY_DEBOUNCE_SCANS 3

enum MacroPadKey : uint8_t {
  PAD_MODE = 26,
  PAD_HOLD = 27,
  PAD_OCT_DOWN = 28,
  PAD_OCT_UP_SHIFT = 29
};
constexpr int KEY_MODE = PAD_MODE;
constexpr int KEY_LATCH = PAD_HOLD;
constexpr int KEY_OCTDN = PAD_OCT_DOWN;
constexpr int KEY_OCTUP = PAD_OCT_UP_SHIFT;
extern const uint8_t kMatrixMap[NUM_ROWS][NUM_COLS];
inline int keyId(int row, int col) {
  if (row < 0 || row >= NUM_ROWS || col < 0 || col >= NUM_COLS) return 255;
  return kMatrixMap[row][col];
}

// types
enum : uint8_t { FAM_SYNTH = 0, FAM_GUITAR = 1, FAM_SAMPLER = 2, FAM_COUNT = 3 };
enum : uint8_t { MODE_POLY = 0, MODE_MONO = 1, MODE_ARP = 2, MODE_COUNT = 3 };
enum : uint8_t { W_SINE = 0, W_TRI, W_SAW, W_SQUARE, W_COUNT };

struct Patch {
  char    name[12];
  uint8_t family, wave1, wave2;
  int8_t  semi2;
  float   detune;
  float   mix;
  float   cutoff, reso, envAmt;
  float   a, d, s, r;
  float   fx;
};

struct Sample { int16_t* data; uint32_t len; uint32_t rate; uint8_t root; char name[12]; };

struct SynthState {
  Patch   patch;
  float   master;
  uint8_t fxType;
  uint8_t mode;
  float   bpm;
  int8_t  octave;
  bool    latch;
  int     sampleIdx;
};
extern SynthState g_state;

struct UiState {
  uint8_t family = 0;
  int16_t patchIdx[3] = {0, 0, 0};
  uint8_t presetSlot = 0;
  uint8_t envSel = 0;
  bool    fxVol = false;
  bool    shift = false, shiftUsed = false;
};
extern UiState g_ui;

#define PRESET_MAGIC 0x31534E53u
struct PresetFile {
  uint32_t magic; uint16_t version, size;
  Patch patch; float master; float bpm; uint8_t fxType, mode; int32_t sampleIdx;
};

enum : uint8_t { EV_KEY_DOWN, EV_KEY_UP, EV_ENC_TURN, EV_ENC_DOWN, EV_ENC_UP };
struct InputEvent { uint8_t type; uint8_t id; int8_t val; };

// patches
static const Patch kSynth[] = {
  {"SawLead",  FAM_SYNTH, W_SAW,    W_SAW,     0,  8.f, .5f, .65f, .20f, .30f, .005f, .30f, .70f, .25f, .15f},
  {"SubBass",  FAM_SYNTH, W_SAW,    W_SQUARE, -12, 4.f, .6f, .35f, .25f, .50f, .002f, .20f, .50f, .10f, .00f},
  {"WarmPad",  FAM_SYNTH, W_SAW,    W_SAW,     0, 14.f, .5f, .45f, .10f, .15f, .600f, .80f, .80f, 1.2f, .35f},
  {"PluckSyn", FAM_SYNTH, W_SAW,    W_TRI,     0,  6.f, .4f, .30f, .30f, .70f, .001f, .25f, .00f, .20f, .20f},
  {"E-Piano",  FAM_SYNTH, W_SINE,   W_TRI,    12,  3.f, .3f, .70f, .00f, .10f, .002f, 1.2f, .20f, .50f, .20f},
  {"PwrSquare",FAM_SYNTH, W_SQUARE, W_SQUARE,  0,  7.f, .5f, .60f, .15f, .25f, .003f, .20f, .80f, .20f, .10f},
  {"Organ",    FAM_SYNTH, W_SINE,   W_SINE,   12,  0.f, .5f, 1.0f, .00f, .00f, .005f, .05f, 1.0f, .06f, .10f},
  {"Brass",    FAM_SYNTH, W_SAW,    W_SAW,     0,  5.f, .5f, .40f, .15f, .50f, .080f, .20f, .70f, .15f, .10f},
};
static const Patch kGuitar[] = {
  {"Nylon", FAM_GUITAR, 0,0,0, 0,0, .45f, 0,0, .001f, 3.0f, 1.f, .12f, .20f},
  {"Steel", FAM_GUITAR, 0,0,0, 0,0, .80f, 0,0, .001f, 4.0f, 1.f, .15f, .20f},
  {"Muted", FAM_GUITAR, 0,0,0, 0,0, .25f, 0,0, .001f, 0.8f, 1.f, .08f, .10f},
};
static const Patch kSamplerBase = {"Sample", FAM_SAMPLER, 0,0,0, 0,0, 1.0f, 0,0, .001f, .10f, 1.f, .15f, .15f};
