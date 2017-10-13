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
#include <bodebug.h>

#include "botonehandler.h"
#include "callbacks.h"


//------------------------------------------------------------------------
//------------------------------ DEFINES ---------------------------------
//------------------------------------------------------------------------

#define NS1_DAC_SS 4 //DO NOT TOUCH!!

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

//------------------------------------------------------------------------
//------------------------------ GLOBALS ---------------------------------
//------------------------------------------------------------------------


Mode<4> gKeyMode(KNOB_2_PIN,mode::singleKey);

Pots gPots;
ToneHandler<NOTES_BUFFER, MIN_NOTE, MAX_NOTE> gNotes(PITCH_RANGE);

AnalogPin gSlidePin(KNOB_1_PIN, pin::slideFactor);
TogglePin gHoldPin(BUTTON_1_PIN,pin::keysHold,100);

DigitalGate mClockTrig(CLICK_TRIG, sync::changed, sync::notChanged);

BoMidi <
BoMidiFilter<1, MIDITYPE::NOTEON, midi::noteon, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::NOTEOFF, midi::noteoff, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::PB, midi::pitch > ,
BoMidiFilter<1, MIDITYPE::CC, midi::changedCC, controlBetween<MIN_CC, MAX_CC> >,
//BoMidiFilter<1, MIDITYPE::CC, midi::changedMod, controlIs<1,1> >,
BoMidiFilter<1, MIDITYPE::CC, midi::changedMod, controlBetween<1,1> >
> gMidi;

DAC_MCP49xx gDAC(DAC_MCP49xx::MCP4922, NS1_DAC_SS, -1);

//------------------------------------------------------------------------
//------------------------ CAllBACK DEFINITIONS --------------------------
//------------------------------------------------------------------------

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

void sync::changed(bool high)
{
  if(high) gNotes.trig(IS_HIGH);
  else gNotes.trig(IS_LOW);
  gNotes.trig(true);
}

void sync::notChanged()
{
  gNotes.trig(false);
}

void pin::slideFactor(uint16_t factor)
{
  gNotes.slide(factor>>3); //0-127
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
  uint16_t fullValue = (((uint16_t)msb) << 7) | lsb;
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

void setup()
{
  Timer1.initialize(8000);
  Timer1.attachInterrupt(updateNS1);
  Wire.begin();
  potsSetup();
  modesSetup();
  gDAC.setGain(2);
  randomSeed(0);
  
  pinMode( TRIGGER_PIN, OUTPUT );
  digitalWrite( TRIGGER_PIN, LOW );

  DEBUG_START(9600);
}

void outputNotes()
{
  //TODO: this makes 
  bool gateOn = gNotes.gateOn();
  digitalWrite( TRIGGER_PIN, (gateOn) ? HIGH : LOW );
  //if ( ! gateOn ) return;
  uint16_t tone = gNotes.currentTone();
  if (tone > DAC_MAX_VALUE) tone = DAC_MAX_VALUE;
  gDAC.outputA( tone );
}

void updateNS1()
{
  //Exec update.
  mClockTrig();
  gKeyMode();
  gSlidePin();
  gHoldPin();
  gMidi.whileMidiDo();

  if ( ! gNotes.allpegiator() && ! gNotes.normal() ) return;
  outputNotes();
  gNotes.utdated();
}

void loop() {
  gPots.write();
}
