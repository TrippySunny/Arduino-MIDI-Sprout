const int sensorPin = A0;
const long baudRate = 115200;

const int scaleLen = 7;
const int keyCount = 3;
const int scales[keyCount][scaleLen] = {
  {0, 2, 4, 5, 7, 9, 11},
  {7, 9, 11, 0, 2, 4, 6},
  {2, 4, 6, 7, 9, 11, 1}
};

const int midiMin = 36;
const int midiMax = 84;

const unsigned long sendInterval = 90;
const int noteChangeThreshold = 1;
const int quietSpan = 2;

const int windowSize = 10;
int history[windowSize];
int historyIndex = 0;
int prevRaw = 0;

int lastNote = -1;
int currentKey = 0;
unsigned long lastSendTime = 0;
unsigned long lastNoteTime = 0;
unsigned long nextKeyChange = 0;
const unsigned long retriggerInterval = 1800;
const unsigned long keyHoldMin = 5000;
const unsigned long keyHoldMax = 12000;

int wrapPcDist(int a, int b) {
  int d = abs(a - b);
  if (12 - d < d) {
    d = 12 - d;
  }
  return d;
}

int snapToKey(int midi, int key) {
  midi = constrain(midi, midiMin, midiMax);
  int octave = midi / 12;
  int pc = midi % 12;
  int bestPc = scales[key][0];
  int bestDist = 12;
  for (int i = 0; i < scaleLen; i++) {
    int d = wrapPcDist(scales[key][i], pc);
    if (d < bestDist) {
      bestDist = d;
      bestPc = scales[key][i];
    }
  }
  int snapped = octave * 12 + bestPc;
  if (pc >= 11 && bestPc <= 1) {
    snapped = (octave + 1) * 12 + bestPc;
  }
  if (pc <= 1 && bestPc >= 10) {
    snapped = (octave - 1) * 12 + bestPc;
  }
  return constrain(snapped, midiMin, midiMax);
}

void scheduleKeyChange(unsigned long now) {
  nextKeyChange = now + (unsigned long)random(keyHoldMin, keyHoldMax + 1);
}

void changeKey() {
  currentKey = (currentKey + 1 + random(2)) % keyCount;
}

int midiFromScaleIndex(int idx) {
  return snapToKey(midiMin + idx, currentKey);
}

void sendMIDI(byte status, byte data1, byte data2) {
  Serial.write(status);
  Serial.write(data1);
  Serial.write(data2);
}

void playNote(int note) {
  note = snapToKey(note, currentKey);
  if (lastNote != -1) {
    sendMIDI(0x80, lastNote, 0);
  }
  sendMIDI(0x90, note, 100);
  lastNote = note;
  lastNoteTime = millis();
}

void setup() {
  Serial.begin(baudRate);
  randomSeed((unsigned long)analogRead(sensorPin) ^ micros());
  currentKey = random(keyCount);
  int initial = analogRead(sensorPin);
  prevRaw = initial;
  for (int i = 0; i < windowSize; i++) {
    history[i] = initial;
  }
  scheduleKeyChange(millis());
}

void loop() {
  unsigned long now = millis();
  if (now - lastSendTime < sendInterval) {
    return;
  }
  lastSendTime = now;

  if (now >= nextKeyChange) {
    changeKey();
    scheduleKeyChange(now);
  }

  int raw = analogRead(sensorPin);
  int delta = raw - prevRaw;
  prevRaw = raw;
  history[historyIndex] = raw;
  historyIndex = (historyIndex + 1) % windowSize;

  int sigMin = history[0];
  int sigMax = history[0];
  for (int i = 1; i < windowSize; i++) {
    if (history[i] < sigMin) {
      sigMin = history[i];
    }
    if (history[i] > sigMax) {
      sigMax = history[i];
    }
  }

  int span = midiMax - midiMin;
  int scaleIndex;
  if (sigMax - sigMin < quietSpan) {
    scaleIndex = span / 2;
  } else {
    scaleIndex = map(raw, sigMin, sigMax, 0, span);
  }
  scaleIndex += constrain(delta / 6, -4, 4);
  scaleIndex = constrain(scaleIndex, 0, span);

  int note = midiFromScaleIndex(scaleIndex);

  bool changed = lastNote == -1 || abs(note - lastNote) >= noteChangeThreshold;
  bool silentTooLong = now - lastNoteTime > retriggerInterval;

  if (changed || silentTooLong) {
    playNote(note);
  }
}
