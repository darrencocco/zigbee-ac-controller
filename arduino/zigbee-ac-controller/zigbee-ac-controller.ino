#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "src/abominable-zigbee-esp32/endpoint/AbominableZigbeeMultistateOutput.h"
#include "src/abominable-zigbee-esp32/endpoint/AbominableZigbeeAnalogOutput.h"
#include "src/abominable-zigbee-esp32/endpoint/AbominableZigbeeBinaryOutput.h"
#include "src/abominable-zigbee-esp32/endpoint/AbominableZigbeeBinaryInput.h"
#include "src/abominable-zigbee-esp32/endpoint/AbominableZigbeeEP.h"

#include <Preferences.h>

#define ENDPOINT_MODE 10
#define ENDPOINT_SET_POINT 11
#define ENDPOINT_EXTERNAL_TEMP 12
#define ENDPOINT_EXTERNAL_TEMP_TOGGLE 13
#define ENDPOINT_SWING_TOGGLE 14
#define ENDPOINT_SLEEP_MODE 15
#define ENDPOINT_FAN_SPEED 16
#define ENDPOINT_WAITING_FOR_REQUEST_FINISH 17
#define ENDPOINT_COMMAND_COUNTDOWN_LENGTH 18

#define MIN_SETPOINT 16
#define MAX_SETPOINT 30

struct storageKeys_s {
  char *MODE = "mode";
  char *FAN_SPEED = "fanSpeed";
  char *SET_POINT = "setPoint";
  char *EXTERNAL_TEMP = "externalTemp";
  char *FOLLOW_EXTERNAL_TEMP = "trackExtTemp";
  char *SWING = "swing";
  char *SLEEP_MODE = "sleepMode";
  char *COMMAND_COUNTDOWN_LENGTH = "commCountdown";
} storageKeys;

typedef enum {
  AUTO = 0x0,
  COOL = 0x1,
  DEHUMIDIFY = 0x2,
  FAN = 0x3,
  HEAT = 0x4,
  OFF = 0x5,
} applianceMode_t;

typedef enum {
  SLOW = 0x0,
  MEDIUM = 0x1,
  FAST = 0x2,
  LUDICROUS = 0x3,
  TURBO_LUDICROUS = 0x4,
} fanSpeed_t;

struct acConfig_s {
  applianceMode_t mode;
  fanSpeed_t fanSpeed;
  uint8_t setPointOffset; // 16c + 4 bit integer pattern, maximum 30c at 0b1111
  bool turbo; // Not 100% sure, made it level 5(Turbo Ludicrous) on fan speed in UI
  bool swing;
  bool xFan; // Moisture removal cycle when turned off
  bool eco; // Sleep mode
  bool iFeel; // External temperature sensor
  int8_t externalThermometer;
} acConfig;

int16_t sendIrCommandCountdown = -1;
int16_t sendIrCommandCountdownReset;

AbominableZigbeeMultistateOutput mode = AbominableZigbeeMultistateOutput(ENDPOINT_MODE);
AbominableZigbeeAnalogOutput setPoint = AbominableZigbeeAnalogOutput(ENDPOINT_SET_POINT);
AbominableZigbeeAnalogOutput externalTemp = AbominableZigbeeAnalogOutput(ENDPOINT_EXTERNAL_TEMP);
AbominableZigbeeBinaryOutput followExternalTemp = AbominableZigbeeBinaryOutput(ENDPOINT_EXTERNAL_TEMP_TOGGLE);
AbominableZigbeeBinaryOutput swing = AbominableZigbeeBinaryOutput(ENDPOINT_SWING_TOGGLE);
AbominableZigbeeBinaryOutput sleepMode = AbominableZigbeeBinaryOutput(ENDPOINT_SLEEP_MODE);
AbominableZigbeeMultistateOutput fanSpeed = AbominableZigbeeMultistateOutput(ENDPOINT_FAN_SPEED);
AbominableZigbeeBinaryInput requestFinished = AbominableZigbeeBinaryInput(ENDPOINT_WAITING_FOR_REQUEST_FINISH);
AbominableZigbeeAnalogOutput commandCountdown = AbominableZigbeeAnalogOutput(ENDPOINT_COMMAND_COUNTDOWN_LENGTH);

Preferences storage;

uint16_t regularReportInterval;
uint16_t loopDelay;

applianceMode_t translateMode(uint16_t zigbeeMode) {
  switch (zigbeeMode) {
    case 0x00:
      return OFF;
    case 0x01:
      return AUTO;
    case 0x02:
      return FAN;
    case 0x03:
      return DEHUMIDIFY;
    case 0x04:
      return COOL;
    case 0x05:
      return HEAT;
    default:
      // TODO: throw exception
      return OFF;
  }
}

uint16_t translateModeAgain(applianceMode_t internalMode) {
  switch (internalMode) {
    case OFF:
      return 0x00;
    case AUTO:
      return 0x01;
    case FAN:
      return 0x02;
    case DEHUMIDIFY:
      return 0x03;
    case COOL:
      return 0x04;
    case HEAT:
      return 0x05;
    default:
      // TODO: throw exception
      return 0x00;
  }
}

void printAcConfigState() {
  log_d("AC Config state");
  log_d("Mode: %u", acConfig.mode);
  log_d("Fan speed: %u", acConfig.fanSpeed);
  log_d("Set point offset: %u", acConfig.setPointOffset);
  log_d("Turbo: %u", acConfig.turbo);
  log_d("Swing louvre: %u", acConfig.swing);
  log_d("X-Fan: %u", acConfig.swing);
  log_d("Eco: %u", acConfig.eco);
  log_d("iFeel: %u", acConfig.iFeel);
  log_d("Externally measured temperature: %i", acConfig.externalThermometer);
}

void resetSendCountdown() {
  sendIrCommandCountdown = sendIrCommandCountdownReset;
}

void sendIrCommand() {
  printAcConfigState();
  log_i("IR Command sent");
  // TODO: Actually send the appropriately composed command.
}

bool acceptModeSet(applianceMode_t mode) {
  log_d("Processing request for mode change to: %u", mode);
  switch (mode) {
    case AUTO:
      setPoint.setValue(25.0);
      setPoint.reportValue();
      acConfig.setPointOffset = 9;
      if (fanSpeed.getSelection() == 4) {
        fanSpeed.setSelection(3);
        fanSpeed.reportSelection();
        acConfig.fanSpeed = LUDICROUS;
        acConfig.turbo = false;
        storage.putUShort(storageKeys.FAN_SPEED, 3);
      }
      acConfig.xFan = false;
      acConfig.eco = false;
      sleepMode.setValue(acConfig.eco);
      sleepMode.reportValue();
      storage.putFloat(storageKeys.SET_POINT, 25.0);
      storage.putBool(storageKeys.SLEEP_MODE, acConfig.eco);
      // Set mode to AUTO in acConfig
      // Set setPointOffset to 9c in acConfig
      // Set setPoint Zigbee cluster to 25c
      // Decrease FAN speed from 5(Turbo Ludicrous) to 4(Ludicrous) if required.
      // Disable xfan sleep mode 
      break;
    case DEHUMIDIFY:
      fanSpeed.setSelection(2);
      fanSpeed.reportSelection();
      acConfig.fanSpeed = FAST;
      acConfig.turbo = false;
      sleepMode.setValue(false);
      sleepMode.reportValue();
      acConfig.eco = false;
      acConfig.xFan = true;
      storage.putUShort(storageKeys.FAN_SPEED, 2);
      storage.putBool(storageKeys.SLEEP_MODE, false);
      // Set mode to dehumidify
      // Set fan speed to 2
      // Disable sleep mode
      break;
    case COOL:
    case FAN:
    case HEAT:
      acConfig.xFan = true;
      break;
  }
  acConfig.mode = mode;
  storage.putUShort(storageKeys.MODE, translateModeAgain(mode));
  resetSendCountdown();
  return true;
}

bool acceptFanSpeedSet(fanSpeed_t fanSpeed) {
  log_d("Processing request for fan speed change to: %u", fanSpeed);
  switch (fanSpeed) {
    case SLOW:
    case FAST:
    case LUDICROUS:
      if (acConfig.mode != DEHUMIDIFY) {
        acConfig.fanSpeed = fanSpeed;
        acConfig.turbo = false;
        break;
      }
      return false;
    case MEDIUM:
      acConfig.fanSpeed = fanSpeed;
      acConfig.turbo = false;
      break;
    case TURBO_LUDICROUS:
      switch (acConfig.mode) {
        case COOL:
        case FAN:
        case HEAT:
          acConfig.fanSpeed = TURBO_LUDICROUS;
          acConfig.turbo = true;
          break;
        default:
          return false;
      }
      break;
  }
  storage.putUShort(storageKeys.FAN_SPEED, (uint8_t)fanSpeed);
  resetSendCountdown();
  return true;
}

bool acceptSetPointSet(uint8_t target) {
  log_d("Processing request for set point change to: %u", target);
  if (target < MIN_SETPOINT || target > MAX_SETPOINT) {
    log_d("Outside allowed range");
    return false;
  }
  if (acConfig.mode == AUTO) {
    log_d("In auto mode");
    return false;
  }
  acConfig.setPointOffset = target - MIN_SETPOINT;
  storage.putFloat(storageKeys.SET_POINT, (float_t)target);
  resetSendCountdown();
  return true;
}

bool acceptExternalTemperatureFollowingSet(bool enabled) {
  acConfig.iFeel = enabled;
  storage.putBool(storageKeys.FOLLOW_EXTERNAL_TEMP, enabled);
  resetSendCountdown();
  return true;
}

bool acceptSwingLouvreSet(bool enabled) {
  acConfig.swing = enabled;
  storage.putBool(storageKeys.SWING, enabled);
  resetSendCountdown();
  return true;
}

bool acceptSleepModeSet(bool enabled) {
  if (enabled && (acConfig.mode == OFF || acConfig.mode == AUTO || acConfig.mode == DEHUMIDIFY)) {
    return false;
  }
  acConfig.eco = enabled;
  storage.putBool(storageKeys.SLEEP_MODE, enabled);
  resetSendCountdown();
  return true;
}

void modeUpdatedCallback(uint16_t value) {
  bool accepted = acceptModeSet(translateMode(value));
  log_d("Change accepted: %s", accepted ? "true" : "false");
}

void fanSpeedUpdatedCallback(uint16_t value) {
  bool accepted = acceptFanSpeedSet((fanSpeed_t)value);
  if (!accepted) {
    fanSpeed.setSelection((uint16_t)value);
    fanSpeed.reportSelection();
  }
  log_d("Change accepted: %s", accepted ? "true" : "false");
}

void setPointUpdatedCallback(float_t value) {
  bool accepted = acceptSetPointSet((uint8_t)value);
  if (!accepted) {
    setPoint.setValue((float_t)(acConfig.setPointOffset + MIN_SETPOINT));
    setPoint.reportValue();
  } else {
    storage.putFloat(storageKeys.SET_POINT, value);
  }
  log_d("Change accepted: %s", accepted ? "true" : "false");
}

void followExternalTempUpdatedCallback(bool enabled) {
  acceptExternalTemperatureFollowingSet(enabled);
}

void sleepModeUpdatedCallback(bool enabled) {
  bool accepted = acceptSleepModeSet(enabled);
  if (!accepted) {
    sleepMode.setValue(false);
    sleepMode.reportValue();
  }
  log_d("Change accepted: %s", accepted ? "true" : "false");
} 

void swingUpdatedCallback(bool enabled) {
  acceptSwingLouvreSet(enabled);
}
/********************* Arduino functions **************************/
void setup() {
  storage.begin("stateData", false);

  mode = AbominableZigbeeMultistateOutput(ENDPOINT_MODE);
  setPoint = AbominableZigbeeAnalogOutput(ENDPOINT_SET_POINT);
  externalTemp = AbominableZigbeeAnalogOutput(ENDPOINT_EXTERNAL_TEMP);
  followExternalTemp = AbominableZigbeeBinaryOutput(ENDPOINT_EXTERNAL_TEMP_TOGGLE);
  swing = AbominableZigbeeBinaryOutput(ENDPOINT_SWING_TOGGLE);
  sleepMode = AbominableZigbeeBinaryOutput(ENDPOINT_SLEEP_MODE);
  fanSpeed = AbominableZigbeeMultistateOutput(ENDPOINT_FAN_SPEED);
  requestFinished = AbominableZigbeeBinaryInput(ENDPOINT_WAITING_FOR_REQUEST_FINISH);
  commandCountdown = AbominableZigbeeAnalogOutput(ENDPOINT_COMMAND_COUNTDOWN_LENGTH);

  bool storageInitialised = storage.isKey("init");

  if (!storageInitialised) {
    storage.putUShort(storageKeys.MODE, 0);
    storage.putUShort(storageKeys.FAN_SPEED, 0);
    storage.putFloat(storageKeys.SET_POINT, 25.0);
    storage.putFloat(storageKeys.EXTERNAL_TEMP, 25.0);
    storage.putBool(storageKeys.FOLLOW_EXTERNAL_TEMP, false); 
    storage.putBool(storageKeys.SWING, false);
    storage.putBool(storageKeys.SLEEP_MODE, false);
    storage.putShort(storageKeys.COMMAND_COUNTDOWN_LENGTH, 3000);
    storage.putBool("init", true);
  }

  const char* manufacturer = "Abominable Inc";
  const char* model = "AC Controller";
  mode.presetManufacturerAndModel(manufacturer, model);
  mode.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  mode.presetDescription("Mode");
  const char *options[] = {"Off", "Auto", "Fan", "Dehumidify", "Cool", "Heat"};
  mode.presetOptions(options, 6);
  uint16_t modeState = storage.getUShort(storageKeys.MODE);
  mode.presetSelection(modeState);
  mode.onSelectionSet(modeUpdatedCallback);

  Zigbee.addEndpoint(&mode);
  
  setPoint.presetManufacturerAndModel(manufacturer, model);
  setPoint.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  setPoint.presetDescription("Set Point");
  setPoint.presetMin((float_t)MIN_SETPOINT);
  setPoint.presetMax((float_t)MAX_SETPOINT);
  setPoint.presetResolution(1.0);
  float_t setPointState = storage.getFloat(storageKeys.SET_POINT);
  setPoint.presetValue(setPointState);
  setPoint.onValueSet(setPointUpdatedCallback);
  
  Zigbee.addEndpoint(&setPoint);

  externalTemp.presetManufacturerAndModel(manufacturer, model);
  externalTemp.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  externalTemp.presetDescription("Ext Temp");
  externalTemp.presetMin(-10.0);
  externalTemp.presetMax(45.0);
  externalTemp.presetResolution(0.1);
  float_t externalTempState = storage.getFloat(storageKeys.EXTERNAL_TEMP);
  externalTemp.presetValue(externalTempState);
  // externalTemp.onValueSet(); FIXME: Callback not implemented

  Zigbee.addEndpoint(&externalTemp);

  followExternalTemp.presetManufacturerAndModel(manufacturer, model);
  followExternalTemp.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  followExternalTemp.presetDescription("Temp Measurement");
  followExternalTemp.presetTrueText("Other temp");
  followExternalTemp.presetFalseText("Internal temp");
  bool followExternalTempState = storage.getBool(storageKeys.FOLLOW_EXTERNAL_TEMP);
  followExternalTemp.presetValue(followExternalTempState);
  followExternalTemp.onValueSet(followExternalTempUpdatedCallback);

  Zigbee.addEndpoint(&followExternalTemp);

  swing.presetManufacturerAndModel(manufacturer, model);
  swing.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  swing.presetDescription("Swing louvre");
  swing.presetTrueText("Swinging");
  swing.presetFalseText("Stationary");
  bool swingState = storage.getBool(storageKeys.SWING);
  swing.presetValue(swingState);
  swing.onValueSet(swingUpdatedCallback);

  Zigbee.addEndpoint(&swing);

  sleepMode.presetManufacturerAndModel(manufacturer, model);
  sleepMode.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  sleepMode.presetDescription("Sleep mode");
  sleepMode.presetTrueText("Enabled");
  sleepMode.presetFalseText("Disabled");
  bool sleepModeState = storage.getBool(storageKeys.SLEEP_MODE);
  sleepMode.presetValue(sleepModeState);
  sleepMode.onValueSet(sleepModeUpdatedCallback);
  
  Zigbee.addEndpoint(&sleepMode);

  fanSpeed.presetManufacturerAndModel(manufacturer, model);
  fanSpeed.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  fanSpeed.presetDescription("Fan speed");
  const char *fanOptions[] = {"Slow", "Medium", "Fast", "Ludicrous", "Turbo Ludicrous"};
  fanSpeed.presetOptions(fanOptions, 5);
  uint16_t fanSpeedState = storage.getUShort(storageKeys.FAN_SPEED);
  fanSpeed.presetSelection(fanSpeedState);
  fanSpeed.onSelectionSet(fanSpeedUpdatedCallback);

  Zigbee.addEndpoint(&fanSpeed);

  requestFinished.presetManufacturerAndModel(manufacturer, model);
  requestFinished.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  requestFinished.presetDescription("Request pending");
  requestFinished.presetTrueText("Waiting");
  requestFinished.presetFalseText("Sent");
  requestFinished.presetValue(false);

  Zigbee.addEndpoint(&requestFinished);

  commandCountdown.presetManufacturerAndModel(manufacturer, model);
  commandCountdown.presetDeviceType(ESP_ZB_HA_WHITE_GOODS_DEVICE_ID);
  commandCountdown.presetDescription("Countdown");
  commandCountdown.presetMin(1000.0);
  commandCountdown.presetMax(15000.0);
  commandCountdown.presetResolution(1.0);
  sendIrCommandCountdownReset = storage.getShort(storageKeys.COMMAND_COUNTDOWN_LENGTH);
  float_t commandCountdownLength = (float_t)sendIrCommandCountdownReset;
  commandCountdown.presetValue(commandCountdownLength);
  // commandCountdown.onValueSet(); FIXME: Callback doesn't exist
  
  Zigbee.addEndpoint(&commandCountdown);

  log_d("Calling Zigbee.begin()");
  Zigbee.begin();

  // mode.setReporting(0, 30);
  // fanSpeed.setReporting(0, 30);
  setPoint.setReporting(0, 30, 1.0);
  externalTemp.setReporting(0, 30, 1.0);
  // followExternalTemp.setReporting(0, 30);
  // swing.setReporting(0, 30);
  // sleepMode.setReporting(0, 30);
  requestFinished.setReporting(0, 30);
  commandCountdown.setReporting(0, 30, 1.0);

  modeUpdatedCallback(mode.getSelection());
  fanSpeedUpdatedCallback(fanSpeed.getSelection());
  setPointUpdatedCallback(setPoint.getValue());
  followExternalTempUpdatedCallback(followExternalTemp.getValue());
  swingUpdatedCallback(swing.getValue());
  sleepModeUpdatedCallback(sleepMode.getValue());

  bool coordinatorBound = false;
  while (!coordinatorBound) {
    if (Zigbee.connected()) {
      mode.bindToCoordinator();
      setPoint.bindToCoordinator();
      externalTemp.bindToCoordinator();
      followExternalTemp.bindToCoordinator();
      swing.bindToCoordinator();
      sleepMode.bindToCoordinator();
      fanSpeed.bindToCoordinator();
      requestFinished.bindToCoordinator();
      commandCountdown.bindToCoordinator();
      delay(500);
    }
    coordinatorBound = mode.isBoundToCoordinator() &&
                       setPoint.isBoundToCoordinator() &&
                       externalTemp.isBoundToCoordinator() &&
                       followExternalTemp.isBoundToCoordinator() &&
                       swing.isBoundToCoordinator() &&
                       sleepMode.isBoundToCoordinator() &&
                       fanSpeed.isBoundToCoordinator() &&
                       requestFinished.isBoundToCoordinator() &&
                       commandCountdown.isBoundToCoordinator();
    delay(3000);
  }

  regularReportInterval = 30000;
  loopDelay = 1000;
}

int32_t reportIntervalTracker = -1;

void loop() {
  if (sendIrCommandCountdown > 0) {
    sendIrCommandCountdown -= loopDelay;
    if (sendIrCommandCountdown <= 0) {
      sendIrCommand();
    }
  }
  reportIntervalTracker -= loopDelay;
  if (reportIntervalTracker < 0) {
    reportIntervalTracker = regularReportInterval;
    mode.reportSelection();
    setPoint.reportValue();
    externalTemp.reportValue();
    followExternalTemp.reportValue();
    swing.reportValue();
    sleepMode.reportValue();
    fanSpeed.reportSelection();
    requestFinished.reportValue();
    commandCountdown.reportValue();
  }
  delay(loopDelay);
}