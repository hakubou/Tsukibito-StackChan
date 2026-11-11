// ｽﾀｯｸﾁｬﾝ ステップ1: CoreS3 の画面に顔を表示する
//  - 何もしなくても、まばたきと呼吸の動きを続ける
//  - 画面をタッチすると表情が切り替わり、セリフを話す
#include <M5Unified.h>
#include <Avatar.h>

using namespace m5avatar;

Avatar avatar;

struct Mood {
  Expression expression;
  const char* text;
};

// 吹き出しのフォントは英数字のみ対応なので、セリフはローマ字にしている
const Mood moods[] = {
  {Expression::Neutral, "Hello!"},
  {Expression::Happy,   "Yay!"},
  {Expression::Sleepy,  "Zzz..."},
  {Expression::Doubt,   "Hmm?"},
  {Expression::Sad,     "Sad..."},
  {Expression::Angry,   "Grr!"},
};
const int moodCount = sizeof(moods) / sizeof(moods[0]);
int moodIndex = 0;
uint32_t speechUntil = 0;

void showMood(int index) {
  avatar.setExpression(moods[index].expression);
  avatar.setSpeechText(moods[index].text);
  speechUntil = millis() + 2000;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;  // 0のままだとSerial.beginされずログが出ない
  M5.begin(cfg);
  M5.Display.setBrightness(128);
  Serial.println("Stack-chan step1 start");

  avatar.init();  // まばたき・呼吸のアニメーションは内部タスクが自動で動かす
  avatar.setBatteryIcon(true);
  showMood(moodIndex);
}

// バッテリーの状態を画面のアイコンとシリアルに出す
void updateBattery() {
  static uint32_t nextCheck = 0;
  if (millis() < nextCheck) return;
  nextCheck = millis() + 2000;

  int32_t level = M5.Power.getBatteryLevel();      // 残量 [%]
  int16_t vbat = M5.Power.getBatteryVoltage();     // バッテリー電圧 [mV]
  int32_t current = M5.Power.getBatteryCurrent();  // 充電なら正、放電なら負 [mA]
  int16_t vbus = M5.Power.getVBUSVoltage();        // USBからの入力電圧 [mV]
  auto charging = M5.Power.isCharging();

  const char* state = charging == m5::Power_Class::is_charging      ? "charging"
                    : charging == m5::Power_Class::is_discharging   ? "discharging"
                                                                    : "unknown";
  Serial.printf("battery: %d%% %dmV %dmA vbus=%dmV state=%s\n",
                level, vbat, current, vbus, state);
  avatar.setBatteryStatus(charging == m5::Power_Class::is_charging, level);
}

void loop() {
  M5.update();
  updateBattery();

  if (M5.Touch.getDetail().wasPressed()) {
    moodIndex = (moodIndex + 1) % moodCount;
    showMood(moodIndex);
    Serial.printf("mood: %s\n", moods[moodIndex].text);
  }

  // 吹き出しは2秒で消す
  if (speechUntil != 0 && millis() > speechUntil) {
    avatar.setSpeechText("");
    speechUntil = 0;
  }

  // 口をゆっくりパクパクさせて「生きている感」を出す（AI会話では音声に合わせる）
  float t = millis() / 1000.0f;
  avatar.setMouthOpenRatio(speechUntil ? (sinf(t * 12.0f) + 1.0f) * 0.4f : 0.0f);

  delay(20);
}
