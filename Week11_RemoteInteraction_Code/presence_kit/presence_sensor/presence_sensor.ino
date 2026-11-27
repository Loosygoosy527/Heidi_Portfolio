#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Same wiring: CS 25, DC 27, RST 26, SCL 18, SDA 23.
Adafruit_ST7735 screen(25, 27, 26);
GFXcanvas16 frame(160, 128);

const int TRIG_PIN = 32;
const int ECHO_PIN = 33;
const float MIN_DISTANCE_CM = 2.30;
const float BLOW_THRESHOLD_CM = 2.90;

const unsigned long HOLD_MS = 500;
const unsigned long ECHO_GRACE_MS = 160;
const unsigned long SENSOR_INTERVAL_MS = 75;
const unsigned long FRAME_INTERVAL_MS = 33;

const int START_X = 22;
const int HOME_X = 134;
const uint16_t SAND = 0xB489;
const uint16_t FISH = 0xFE06;
const uint16_t BELLY = 0xFFE9;
const uint16_t BUBBLE = 0x7DFF;

unsigned long lastSensorMs = 0;
unsigned long lastFrameMs = 0;
unsigned long blowStartedMs = 0;
unsigned long lastValidBlowMs = 0;
bool trackingBlow = false;

float fishX = START_X;
float swimPhase = 0;

void readFlap(unsigned long now) {
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long echoTime = pulseIn(ECHO_PIN, HIGH, 3000);
  float distanceCm = echoTime * 0.01715f;

  if (echoTime == 0) {
    Serial.println("No echo");
  } else {
    Serial.print(distanceCm, 2);
    Serial.println(" cm");
  }

  bool inRange = distanceCm >= MIN_DISTANCE_CM &&
                 distanceCm < BLOW_THRESHOLD_CM;

  if (trackingBlow && now - lastValidBlowMs > ECHO_GRACE_MS) {
    trackingBlow = false;
  }

  if (inRange) {
    if (!trackingBlow) {
      blowStartedMs = now;
      trackingBlow = true;
    }
    lastValidBlowMs = now;
  }
}

void drawOcean(unsigned long now) {
  // Darker blue water toward the seabed.
  frame.fillScreen(0x0437);
  frame.fillRect(0, 26, 160, 24, 0x0334);
  frame.fillRect(0, 50, 160, 24, 0x0271);
  frame.fillRect(0, 74, 160, 54, 0x01AD);

  // Sand and small pebbles.
  frame.fillRect(0, 108, 160, 20, SAND);
  frame.fillCircle(13, 107, 5, SAND);
  frame.fillCircle(65, 108, 7, SAND);
  frame.fillCircle(114, 109, 6, SAND);
  for (int i = 0; i < 15; i++) {
    frame.drawPixel(5 + i * 10, 113 + (i * 7) % 12, 0x83A6);
  }

  // Seaweed sways gently.
  for (int i = 0; i < 4; i++) {
    int x = 8 + i * 9;
    int tipY = 81 + (i % 3) * 6;
    int sway = roundf(3 * sinf(now * 0.003f + i));
    frame.drawLine(x, 109, x + sway, 96, 0x3D88);
    frame.drawLine(x + sway, 96, x + sway + 3, tipY, 0x3D88);
  }

  // Rocky cave on the right.
  frame.fillCircle(HOME_X, 82, 23, 0x7B6A);
  frame.fillRect(HOME_X - 23, 82, 47, 27, 0x7B6A);
  frame.fillCircle(HOME_X, 84, 14, 0x0845);
  frame.fillRect(HOME_X - 14, 84, 29, 25, 0x0845);

  // Bubbles continue rising while the fish rests.
  for (int i = 0; i < 7; i++) {
    int y = 102 - ((now / (30 + i * 4) + i * 17) % 78);
    int x = 14 + i * 21 + roundf(2 * sinf(now * 0.003f + i));
    int radius = 1 + i % 3;
    frame.drawCircle(x, y, radius, BUBBLE);
    if (radius == 3) {
      frame.drawPixel(x - 1, y - 1, ST77XX_WHITE);
    }
  }
}

void drawFish(bool swimming, unsigned long now) {
  bool home = fishX >= HOME_X;
  int x = roundf(fishX);
  int y = home ? 91 : 69 + roundf(2 * sinf(swimPhase));
  int tail = swimming
    ? roundf(4 * sinf(swimPhase * 2.0f))
    : roundf(sinf(now * 0.005f));

  frame.fillTriangle(x - 7, y, x - 19, y - 5 + tail,
                     x - 19, y + 5 + tail, 0xFCA0);

  // Small spikes give the fish its pufferfish outline.
  frame.fillTriangle(x - 7, y - 7, x - 6, y - 14, x - 1, y - 8, FISH);
  frame.fillTriangle(x, y - 8, x + 4, y - 14, x + 8, y - 6, FISH);
  frame.fillTriangle(x - 7, y + 6, x - 6, y + 13, x, y + 8, FISH);

  frame.fillCircle(x, y, 10, FISH);
  frame.fillCircle(x + 1, y + 3, 6, BELLY);
  frame.fillTriangle(x - 3, y + 2, x - 10, y + 5 + tail,
                     x - 4, y + 6, 0xFCA0);

  if (home) {
    frame.drawLine(x + 3, y - 2, x + 7, y - 2, ST77XX_BLACK);
  } else {
    frame.fillCircle(x + 5, y - 2, 3, ST77XX_WHITE);
    frame.fillCircle(x + 6, y - 2, 1, ST77XX_BLACK);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  SPI.begin(18, -1, 23, 25);
  screen.initR(INITR_BLACKTAB);
  screen.setRotation(1);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;
    readFlap(now);
  }

  unsigned long frameTime = now - lastFrameMs;
  if (frameTime < FRAME_INTERVAL_MS) return;
  lastFrameMs = now;

  bool swimming = trackingBlow &&
                  now - lastValidBlowMs <= ECHO_GRACE_MS &&
                  now - blowStartedMs >= HOLD_MS &&
                  fishX < HOME_X;

  if (swimming) {
    float seconds = min(frameTime, 50UL) / 1000.0f;
    fishX += 20.0f * seconds;
    if (fishX > HOME_X) fishX = HOME_X;
    swimPhase += 9.0f * seconds;
  }

  drawOcean(now);
  drawFish(swimming, now);

  frame.setTextColor(ST77XX_WHITE);
  frame.setCursor(7, 8);
  if (fishX >= HOME_X) {
    frame.print("Safe in the cave");
  } else if (swimming) {
    frame.print("Swimming...");
  } else if (trackingBlow) {
    frame.print("Keep blowing gently");
  } else {
    frame.print("Blow gently to swim");
  }

  screen.drawRGBBitmap(0, 0, frame.getBuffer(), 160, 128);
}