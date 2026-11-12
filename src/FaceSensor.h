// 内蔵カメラ（GC0308）で顔を検出する
//  - 別タスク（コア0）で撮影 → esp-dl の顔検出（2段階：MSR01 → MNP01）
//  - 結果は最新の1件だけを保持し、get() で取り出す
//  - 画像は検出に使うだけで、保存も送信もしない
#pragma once
#include <M5Unified.h>
#include <esp_camera.h>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

namespace face_sensor {

struct Result {
  int count = 0;          // 検出した顔の数
  int x = 0, y = 0;       // 一番大きい顔の中心（画像座標 320x240）
  int w = 0, h = 0;       // その顔の幅・高さ
  uint32_t atMs = 0;      // 検出した時刻
  uint32_t detectMs = 0;  // 1回の検出にかかった時間
  uint32_t frames = 0;    // 起動からの検出回数
};

namespace detail {
inline Result latest;
inline SemaphoreHandle_t lock = nullptr;
inline volatile uint32_t intervalMs = 200;  // 検出の間隔（負荷調整用）
inline volatile bool paused = false;

inline void task(void*) {
  HumanFaceDetectMSR01 stage1(0.1F, 0.5F, 10, 0.2F);
  HumanFaceDetectMNP01 stage2(0.5F, 0.3F, 5);
  for (;;) {
    if (paused) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    const uint32_t t0 = millis();
    std::vector<int> shape = {(int)fb->height, (int)fb->width, 3};
    auto& candidates = stage1.infer((uint16_t*)fb->buf, shape);
    auto& faces = stage2.infer((uint16_t*)fb->buf, shape, candidates);
    Result r;
    r.detectMs = millis() - t0;
    r.count = faces.size();
    int best = 0;
    for (auto& f : faces) {
      const int w = f.box[2] - f.box[0], h = f.box[3] - f.box[1];
      if (w * h > best) {
        best = w * h;
        r.x = (f.box[0] + f.box[2]) / 2;
        r.y = (f.box[1] + f.box[3]) / 2;
        r.w = w;
        r.h = h;
      }
    }
    esp_camera_fb_return(fb);
    r.atMs = millis();
    xSemaphoreTake(lock, portMAX_DELAY);
    r.frames = latest.frames + 1;
    latest = r;
    xSemaphoreGive(lock);
    vTaskDelay(pdMS_TO_TICKS(intervalMs));
  }
}
}  // namespace detail

// CoreS3 のカメラ配線
inline bool begin() {
  camera_config_t c = {};
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.pin_xclk = 2;
  c.pin_sccb_sda = 12;
  c.pin_sccb_scl = 11;
  c.pin_d7 = 47; c.pin_d6 = 48; c.pin_d5 = 16; c.pin_d4 = 15;
  c.pin_d3 = 42; c.pin_d2 = 41; c.pin_d1 = 40; c.pin_d0 = 39;
  c.pin_vsync = 46;
  c.pin_href = 38;
  c.pin_pclk = 45;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_RGB565;
  c.frame_size = FRAMESIZE_QVGA;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  c.sccb_i2c_port = M5.In_I2C.getPort();

  // フレームの取りこぼし時にカメラ内部タスクが出す警告ログは、そのタスクのスタックをあふれさせるので止める
  esp_log_level_set("cam_hal", ESP_LOG_NONE);
  // カメラの SCCB は内部 I2C（G11/G12）を共有するので、M5Unified から一度手放す
  M5.In_I2C.release();
  const esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("camera init failed: 0x%x\n", err);
    return false;
  }
  detail::lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(detail::task, "faceSensor", 8192, nullptr, 1, nullptr, 0);
  return true;
}

inline Result get() {
  Result r;
  if (!detail::lock) return r;
  xSemaphoreTake(detail::lock, portMAX_DELAY);
  r = detail::latest;
  xSemaphoreGive(detail::lock);
  return r;
}

inline void setInterval(uint32_t ms) { detail::intervalMs = ms; }
inline void pause(bool p) { detail::paused = p; }

}  // namespace face_sensor
