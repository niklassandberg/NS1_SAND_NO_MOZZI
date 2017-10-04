#include <SPI.h>
#include <DAC_MCP49xx.h>
#include <TimerOne.h>

#include <bomidi2.h>
#include <ccdigipots.h>
#include <bodigitalgate2.h>
#include <bomode.h>
#include <bofilters.h>
#include <botogglepin.h>
#include <bopins.h>

#define NS1_DAC_SS 4

// ------------------- PROJECT CONSTANTS ---------------------------
// -------------------- can be changed -----------------------------
// -----------------------------------------------------------------

#define NOTES_BUFFER 127

#define MIN_NOTE 36
#define MAX_NOTE MIN_NOTE+61

#define MIN_CC 34
#define MAX_CC 37

#define PITCH_RANGE 2

#define TRIGGER_PIN 5
#define CLICK_TRIG 6
#define BUTTON_1_PIN 7 

#define KNOB_1_PIN A1
#define KNOB_2_PIN A2



#include "botonehandler.h"

// -----------------------------------------------------------------
// --------------------- IMPL --------------------------------------
// -----------------------------------------------------------------

namespace midi
{
  void noteon(uint8_t note, uint8_t velocity);
  void noteoff(uint8_t note, uint8_t velocity);
  void pitch(uint8_t lsb, uint8_t msb);
  void changedMod(uint8_t cc, uint8_t value);
  void changedCC(uint8_t cc, uint8_t value);
};

namespace gate {
  void changed(bool high);
  void notChanged();
};

namespace mode {
  void singleKey();
  void allpegiatorNormal();
  void allpegiatorRandom();
  void allpegiatorUpDown();
};

namespace pin {
void glideFactor(uint16_t factor);
void keysHold(bool state);
};

Mode<4> gKeyMode(KNOB_2_PIN,mode::singleKey);

void outputNotes();

Pots gPots;
ToneHandler<NOTES_BUFFER, MIN_NOTE, MAX_NOTE> gNotes(PITCH_RANGE);

AnalogPin gGlidePin(KNOB_1_PIN, pin::glideFactor);
TogglePin gHoldPin(BUTTON_1_PIN,pin::keysHold,100);

DigitalGate mClockTrig(CLICK_TRIG, gate::changed, gate::notChanged);

BoMidi <
BoMidiFilter<1, MIDITYPE::NOTEON, midi::noteon, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::NOTEOFF, midi::noteoff, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::PB, midi::pitch > ,
BoMidiFilter<1, MIDITYPE::CC, midi::changedCC, controlBetween<MIN_CC, MAX_CC> >,
BoMidiFilter<1, MIDITYPE::CC, midi::changedMod, controlBetween<1,1> >
> gMidi;

DAC_MCP49xx gDAC(DAC_MCP49xx::MCP4922, NS1_DAC_SS, -1);

void mode::singleKey()
{
  gNotes.mode(NORMAL);
  gNotes.allpegiatorOff();
}
void mode::allpegiatorNormal()
{
  gNotes.mode(ALLPEG_UP);
  gNotes.allpegiatorOn();
}
void mode::allpegiatorRandom()
{
  gNotes.mode(ALLPEG_RANDOM);
  gNotes.allpegiatorOn();
}
void mode::allpegiatorUpDown()
{
  gNotes.mode(ALLPEG_UPDOWN);
  gNotes.allpegiatorOn();
}

void gate::changed(bool high)
{
  if(high) gNotes.gate(IS_HIGH);
  else gNotes.gate(IS_LOW);
  gNotes.gate(true);
}

void gate::notChanged()
{
  gNotes.gate(false);
}

void pin::glideFactor(uint16_t factor)
{
  gNotes.glide(factor);
}

void pin::keysHold(bool state)
{
  //TODO: not rigth now, wait for this feature.
  //gNotes.hold(state);
}

void midi::noteon(uint8_t note, uint8_t velocity)
{
  if(velocity) gNotes.addNote(note);
  else gNotes.removeNote(note);
}
void midi::noteoff(uint8_t note, uint8_t velocity)
{
  gNotes.removeNote(note);
}
void midi::pitch(uint8_t lsb, uint8_t msb)
{
  uint16_t fullValue = (((uint16_t)msb) << 7) + lsb;
  gNotes.addPitch(fullValue);
}

void midi::changedMod(uint8_t cc, uint8_t value)
{
  if (value <= 3) gDAC.outputB(0);
  else gDAC.outputB(value * 32);
}

void midi::changedCC(uint8_t cc, uint8_t value)
{
  gPots.read(cc, value);
}

void outputNotes()
{
  bool gateOn = gNotes.gateOn();
  digitalWrite( TRIGGER_PIN, (gateOn) ? HIGH : LOW );
  if ( ! gateOn ) return;
  uint16_t tone = gNotes.currentTone();
  if (tone > DAC_MAX_VALUE) tone = DAC_MAX_VALUE;
  gDAC.outputA( tone );
}

void modesSetup()
{
  gKeyMode[0] = mode::singleKey;
  gKeyMode[1] = mode::allpegiatorNormal;
  gKeyMode[2] = mode::allpegiatorRandom;
  gKeyMode[3] = mode::allpegiatorUpDown;
}

void potsSetup()
{
  uint8_t ccs[4];
  for(int i = 0 ; i <= 4 ; ++i)
    ccs[i] = i+MIN_CC;
  gPots.setup(ccs);
}

void setup() {
  Timer1.initialize(8000);
  Timer1.attachInterrupt(updateNS1);
  Wire.begin();
  potsSetup();
  modesSetup();
  gDAC.setGain(2);
  randomSeed(0);
  
  pinMode( TRIGGER_PIN, OUTPUT );
  digitalWrite( TRIGGER_PIN, LOW );
}

void updateNS1()
{
  mClockTrig();
  gKeyMode();
  gGlidePin();
  gHoldPin();
  gMidi.ifMidiDo();
  
  if ( ! gNotes.update() ) return;
  
  gNotes.allpegiator();
  outputNotes();
  gNotes.utdated();
}

void loop() {
  gPots.write();
}
