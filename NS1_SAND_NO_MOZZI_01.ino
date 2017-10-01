#include <SPI.h>
#include <DAC_MCP49xx.h>
#include <TimerOne.h>

#include <bomidi2.h>
#include <ccdigipots.h>
#include <bodigitalgate.h>
#include <bofilters.h>
#include <boarraycontainer.h>
// ------------------- FIXED CONSTANTS ---------------------------
// -- !!! DO NOT CHANGE IF YOU DONT KNOW WHAT YOU ARE DOING !!! --
// ---------------------------------------------------------------

// Most significant bit and least significant bit at pitch:
// ([0-127] << 7) + [0-127] = [0-16383]
#define MAX_PITCH_MIDI_VALUE 16383
#define MAX_DAC_KEY_MIDI_MAP_VAL 4095

#define NS1_DAC_SS 4
// MCP4922 is 12 bit DAC. 2^12 = 4096
#define DAC_MAX_VALUE 4096
// DAC_SEMI_TONE_VALUE
#define DAC_SEMI_TONE_VALUE 68

// ------------------- PROJECT CONSTANTS ---------------------------
// -------------------- can be changed -----------------------------
// -----------------------------------------------------------------

#define MIN_NOTE 36
#define MAX_NOTE MIN_NOTE+61

#define MIN_CC 34
#define MAX_CC 37

#define NOTES_BUFFER 127
#define PITCH_RANGE 2
#define TRIGGER_PIN 5
#define KNOB_1_PIN A1
#define KNOB_2_PIN A2

#define CLICK_TRIG 6

uint16_t midiToDacVal(uint8_t midiVal)
{
  uint16_t dacVal = map(midiVal, MIN_NOTE, MAX_NOTE - 1, 0, MAX_DAC_KEY_MIDI_MAP_VAL);
  dacVal += (2 & midiVal) ? 1 : 0; //correlate dac semitones based on analyse.
  return dacVal;
}

// -----------------------------------------------------------------
// --------------------- IMPL --------------------------------------
// -----------------------------------------------------------------

class ToneHandler
{

    const int16_t ANALOG_HALF_BEND;

    uint16_t mCurrentTone;
    uint16_t mNextTone;
    bool mNoteOverlap;

    size_t mNoteIndex;
    //notes beging pressed down.
    array_container<uint8_t, NOTES_BUFFER> mNotes;
    //indicates that midi has changed the tone.
    bool mMIDIDirty;
    //pitch value for dac.
    int16_t mBend;

    void removeMidiNote(uint8_t note);
    void setOverlap(uint8_t noteIndex);
    void setOverlap();

  public:
    ToneHandler(uint8_t noteBuffer, uint8_t pitchRange);

    bool update();
    void utdated();
    bool gateOn();
    void addNote(uint8_t midiNote);
    void removeNote(uint8_t midiNote);
    
    //Todo: this two needs to be merge and . Same func call, 
    void allpegiator();
    uint16_t currentTone();
    
    void addPitch(uint16_t pitch); // pitch bend is +/- ANALOG_HALF_BEND semitones
};

//----------------------------------
//----- ToneHandler ------------
//----------------------------------

class Mode
{
    uint8_t mMode;

    uint8_t fetchMode()
    {
      return analogRead(KNOB_2_PIN) >> 8;
    }
  public:

    const static uint8_t NORMAL = 0;
    
    const static uint8_t ALLPEG_START = 1;
    const static uint8_t ALLPEG_NORMAL = 1;
    const static uint8_t ALLPEG_UPPDOWN = 2;
    const static uint8_t ALLPEG_RANDOM = 3;
    const static uint8_t ALLPEG_END = 3;

    Mode() : mMode(fetchMode()) {}

    void operator()()
    {
      mMode = fetchMode();
    }

    bool allpegiator()
    {
      return mMode >= ALLPEG_START && mMode <= ALLPEG_END;
    }

    uint8_t get()
    {
      return mMode;
    }
};

void noteon(uint8_t note, uint8_t velocity);
void noteoff(uint8_t note, uint8_t velocity);
void pitch(uint8_t lsb, uint8_t msb);
void changedMod(uint8_t cc, uint8_t value);
void changedCC(uint8_t cc, uint8_t value);
void outputNotes();

DAC_MCP49xx dac(DAC_MCP49xx::MCP4922, NS1_DAC_SS, -1);

Pots gPots;
ToneHandler gNotes(NOTES_BUFFER, PITCH_RANGE);

BoMidi <
BoMidiFilter<1, MIDITYPE::NOTEON, noteon, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::NOTEOFF, noteoff, keyBetween<MIN_NOTE,MAX_NOTE> > ,
BoMidiFilter<1, MIDITYPE::PB, pitch > ,
BoMidiFilter<1, MIDITYPE::CC, changedCC, controlBetween<MIN_CC, MAX_CC> >,
BoMidiFilter<1, MIDITYPE::CC, changedMod, controlBetween<1,1> >
> gMidi;

Mode mMode;
DigitalGate mClockTrig(CLICK_TRIG);

void ToneHandler::removeMidiNote(uint8_t note)
{
  size_t index = mNotes.index( note );
  if ( index != mNotes.index_end() )
  {
    mNotes.remove_at(index);
    mMIDIDirty = true;
  }
}

void ToneHandler::setOverlap(uint8_t noteIndex)
{
  //Setting mCurrentTone = MAX_DAC_KEY_MIDI_MAP_VAL+1;
  //halts the slide if mNotes has one note.
  if (mNotes.size())
  {
    //Serial.print("mNotes[noteIndex]: "); //Serial.println(mNotes[noteIndex]);
    mNextTone = midiToDacVal( mNotes[noteIndex] );
  }

  if (mNotes.size() && mCurrentTone < MAX_DAC_KEY_MIDI_MAP_VAL + 1)
  {
    mNoteOverlap = mCurrentTone != mNextTone;
  }
  else
  {
    mCurrentTone = MAX_DAC_KEY_MIDI_MAP_VAL + 1;
    mNoteOverlap = false;
  }
}

void ToneHandler::setOverlap()
{
  mNoteIndex = mNotes.size() - 1;
  setOverlap(mNoteIndex);
}

ToneHandler::ToneHandler(uint8_t noteBuffer, uint8_t pitchRange) :
  ANALOG_HALF_BEND( ( (uint16_t) pitchRange ) * DAC_SEMI_TONE_VALUE ) ,
  mMIDIDirty(false) ,
  mBend(0) ,
  mCurrentTone(0) ,
  mNoteOverlap(false) ,
  mNoteIndex(0)
{}

void ToneHandler::allpegiator()
{
  if ( ! ( mClockTrig.changed() && mClockTrig.high() ) ) return;

  if ( mNotes.size() > 1 )
  {
    switch (mMode.get())
    {
      case Mode::ALLPEG_RANDOM :
        mNoteIndex = random(mNotes.size());
        break;
      case Mode::ALLPEG_UPPDOWN :
        static bool up = true;
        if (up) up = ++mNoteIndex < mNotes.size() - 1;
        else up = --mNoteIndex == 0;
        mNoteIndex = (mNoteIndex) % mNotes.size();
        break;
      default:
        mNoteIndex = (mNoteIndex + 1) % mNotes.size();
        break;
    }
  }
  else
  {
    mNoteIndex = 0;
  }

  setOverlap(mNoteIndex);
  mMIDIDirty = true;
}


bool ToneHandler::update()
{
  mMode();
  if (mMode.allpegiator())
    return mClockTrig.changed() || mNoteOverlap;
  else
    return mMIDIDirty || mNoteOverlap;
}

void ToneHandler::utdated()
{
  mMIDIDirty = false;
}

bool ToneHandler::gateOn()
{
  if (mMode.allpegiator())
    return ! mNotes.empty() && mClockTrig.high();
  else
    return ! mNotes.empty();
}

void ToneHandler::addNote(uint8_t midiNote)
{
  // remove note if it is already being played
  removeMidiNote(midiNote);
  mNotes.push_back(midiNote);
  mMIDIDirty = true;
  setOverlap();
}

void ToneHandler::removeNote(uint8_t midiNote)
{
  removeMidiNote( midiNote );
  setOverlap();
}

uint16_t ToneHandler::currentTone()
{
  if(mNoteOverlap)
    keyGlide2(mNextTone, mCurrentTone, analogRead(KNOB_1_PIN));
  else
    mCurrentTone = mNextTone;
  return (uint16_t) mCurrentTone + mBend;
}

// pitch bend is +/- ANALOG_HALF_BEND semitones
void ToneHandler::addPitch(uint16_t pitch)
{
  // allow for a slight amount of slack in the middle
  if ( abs(pitch - 64) < 2 ) pitch = 64;
  mBend = map(pitch, 0, MAX_PITCH_MIDI_VALUE, -ANALOG_HALF_BEND, ANALOG_HALF_BEND) ;
  if ( ! mNotes.empty() ) mMIDIDirty = true;
}

void noteon(uint8_t note, uint8_t velocity)
{
  gNotes.addNote(note);
}
void noteoff(uint8_t note, uint8_t velocity)
{
  gNotes.removeNote(note);
}
void pitch(uint8_t lsb, uint8_t msb)
{
  uint16_t fullValue = (((uint16_t)msb) << 7) + lsb;
  gNotes.addPitch(fullValue);
}

void changedMod(uint8_t cc, uint8_t value)
{
  if (value <= 3) dac.outputB(0);
  else dac.outputB(value * 32);
}

void changedCC(uint8_t cc, uint8_t value)
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
  dac.outputA( tone );
}

bool gOldTrig = false;

void updateNS1()
{
  mClockTrig();
  gMidi.ifMidiDo();
  if ( ! gNotes.update() ) return;
  if ( mMode.allpegiator() )
    gNotes.allpegiator();
  outputNotes();
  gNotes.utdated();
}

void potsSetup()
{
  uint8_t ccs[4];
  for(int i = 0 ; i <= 4 ; ++i)
    ccs[i] = i+MIN_CC;
  gPots.setup(ccs);
}

void setup() {
  //Serial.begin(9600);

  Timer1.initialize(8000);
  Timer1.attachInterrupt(updateNS1);

  pinMode( TRIGGER_PIN, OUTPUT );
  pinMode( KNOB_1_PIN, INPUT );
  pinMode( KNOB_2_PIN, INPUT );
  pinMode( CLICK_TRIG, INPUT );

  digitalWrite( TRIGGER_PIN, LOW );
  digitalWrite( CLICK_TRIG, LOW );

  Wire.begin();

  potsSetup();
  
  dac.setGain(2);

  randomSeed(0);

}



void loop() {
  gPots.write();
}
