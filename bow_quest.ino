#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <IRremote.hpp>

// Serial:
// TX 1
// RX 3

#define SPARE 3
#define LED_PIN 1
#define TRIGGER_PIN 0

#define IR_SEND_PIN 4
#define IR_RECEIVE_PIN 14

// My ID.
// Doesn't need to be unique between modules.
int me = 8;

Adafruit_NeoPixel pixels(24, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.end();
  WiFi.mode(WIFI_OFF);

  pinMode(TRIGGER_PIN, INPUT);

  pixels.begin();

  IrReceiver.begin(IR_RECEIVE_PIN, false);
  IrSender.setSendPin(IR_SEND_PIN);
  IrSender.enableIROut(38);
}

/// State machine.

// Idle state.
// Trigger is up.
// 'energy' will gradually replenish.
// Listens on IR receiver.
// Receptive to being shot.
#define STATE_IDLE 0

// Charging state.
// Trigger is down.
// Transferring from 'energy' to 'charge'.
#define STATE_CHARGING 1

// Firing state.
// Trigger has just been released.
// Drains 'charge' then sends an IR burst.
#define STATE_FIRING 2

// Dead state.
// Health has all gone.
// Terminal.
#define STATE_DEAD 3

int state = STATE_IDLE;

/// Charge

// The 'strength' of a shot.
// Corresponds to the length of the Infrared burst.
// Longer burst has a better chance of a hit.
int charge = 0;

// Rate at which energy is replenished.
float energyReplenishPerSecond = 2.0;

/// Health
// Depletes when shot.
#define MAX_HEALTH 10
int health = 10;

#define IMMUNITY_MS 100
unsigned long immuneUntil = millis();

/// Energy
// Amount available to charge and shoot.
#define MAX_ENERGY 100
float energy = MAX_ENERGY;

#define LOOP_DELAY 10

#define MAX_SEND_REPEATS 10

/// Blocking actions

void fire() {
  // Firing animation
  for (int r = 0; r < 5; r++) {
    for (int i = r; i < 12; i++) {
      pixels.clear();
      pixels.setPixelColor(i, pixels.Color(r * 20, 200, (10 - r) * 20));
      pixels.setPixelColor(24 - i, pixels.Color(r * 20, 200, (10 - r) * 20));
      pixels.show();
      delay(10);
    }
  }

  int fireAmount = ((energy * MAX_SEND_REPEATS) / MAX_ENERGY) + 1;
  IrSender.sendNEC(0x0102, me, fireAmount);
  state = STATE_IDLE;
}

void wasHit() {
  health = health - 1;

  // Hit animation.
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536;
       firstPixelHue += 1024) {
    pixels.rainbow(firstPixelHue);
    pixels.show();
    delay(10);
  }
}


/// Event loop functions

// State transitions for firing depending on trigger.
void tickTrigger() {
  // If the trigger is down, start or continue charging.
  if (digitalRead(TRIGGER_PIN) == LOW) {

    // Transition to charging state.
    if (state == STATE_IDLE) {
      state = STATE_CHARGING;
    }

    // If in charging, continue to transfer.
    if (state == STATE_CHARGING) {
      if (energy > 0) {
        energy--;
        charge++;
      }
    }

  } else {
    // Trigger up.

    if (state == STATE_CHARGING) {
      state = STATE_FIRING;
    }

    if (state == STATE_FIRING) {
      if (charge > 0) {
        charge -= 2;
        charge = max(charge, 0);
      } else {
        fire();
      }
    }
  }
}

void tickRecharge() {
  // Continual trickle recharge.
  // With a delay of 10, 0.01 is 1 per second.
  // 1 per second.
  if (energy < MAX_ENERGY) {
    energy += energyReplenishPerSecond / (1000.0 / (float)LOOP_DELAY);
  }
}

// Update LEDs depending on state.
void tickLed() {
  pixels.clear();

  if (state == STATE_IDLE) {
    // Idle show health on first bar.
    int healthValue = (float)health / (float)MAX_HEALTH * 12.0;
    for (int i = 0; i < 12; i++) {
      if (healthValue >= i) {
        pixels.setPixelColor(i, pixels.Color(30, 5, 5));
      }
    }

    // Idle show energy on second bar.
    int energyValue = (float)energy / (float)MAX_ENERGY * 12.0;
    for (int i = 12; i < 24; i++) {
      if (energyValue >= i - 12) {
        pixels.setPixelColor(i, pixels.Color(5, 5, 30));
      }
    }
  }

  if (state == STATE_CHARGING) {
    int chargeValue = ((float)charge / (float)MAX_ENERGY) * 12;
    for (int i = 0; i < 12; i++) {
      if (chargeValue >= i) {
        pixels.setPixelColor(i, pixels.Color(0, 100, 0));
        pixels.setPixelColor(24 - i, pixels.Color(0, 100, 0));
      }
    }
  }

  if (state == STATE_DEAD) {
    bool flash = (millis() % 500) > 250;

    for (int i = 0; i < 24; i++) {
      if ((flash && i % 2 == 0) || (!flash && i % 2 == 1)) {
        pixels.setPixelColor(i, pixels.Color(200, 0, 0));
      }
    }
  }

  pixels.show();
}

void tickListen() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.command != me) {
      if (health >= 0) {
        unsigned long now = millis();
        if (now > immuneUntil) {
          wasHit();
          immuneUntil = now + IMMUNITY_MS;
        }
      }
    }
    IrReceiver.resume();
  }
}

void tickHealth() {
  if (state == STATE_IDLE) {
    if (health <= 1) {
      state = STATE_DEAD;
    }
  }
}

void loop() {

  tickListen();

  tickTrigger();

  tickRecharge();

  tickLed();

  tickHealth();

  delay(LOOP_DELAY);
}
