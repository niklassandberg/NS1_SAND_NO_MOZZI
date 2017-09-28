#include <SPI.h>
#include <DAC_MCP49xx.h>
#include <TimerOne.h>
#include <BO_NS1_DIGIPOTS.h>
#include <BOMIDI2.h>
#include <utils/array_container_size.h>

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

// -----------------------------------------------------------------
// --------------------- IMPL --------------------------------------
// -----------------------------------------------------------------

class ToneHandler
{
    const int16_t ANALOG_HALF_BEND; 

    uint16_t mCurrentTone;
    uint16_t mNextTone;
    bool mNoteOverlap;

    array_container<uint8_t,NOTES_BUFFER> mNotes; //notes beging pressed down.
    bool mMIDIDirty; //indicates that midi has changed the tone.
    int16_t mBend; //pitch value for dac.

    uint16_t midiToDacVal(uint8_t midiVal)
    {
      uint16_t dacVal = map(midiVal, MIN_NOTE, MAX_NOTE - 1, 0, MAX_DAC_KEY_MIDI_MAP_VAL);
      dacVal += (2 & midiVal) ? 1 : 0; //correlate dac semitones based on analyse.
      return dacVal;
    }

    void removeMidiNote(uint8_t note)
    {
      size_t index = mNotes.index( note );
      if ( index != mNotes.index_end() )
      {
        mNotes.remove_at(index);
        mMIDIDirty = true;
      }
    }

    void setOverlap()
    {
      //Setting mCurrentTone = MAX_DAC_KEY_MIDI_MAP_VAL+1;
      //halts the slide if mNotes has one note.
      if(mNotes.size())
      {
        mNextTone = midiToDacVal( mNotes.peek_back() );
      }
      
      if (mNotes.size() && mCurrentTone < MAX_DAC_KEY_MIDI_MAP_VAL+1)
      {
        mNextTone = midiToDacVal( mNotes.peek_back() );
        mNoteOverlap = mCurrentTone != mNextTone;
      }
      else 
      {
        mCurrentTone = MAX_DAC_KEY_MIDI_MAP_VAL+1;
        mNoteOverlap = false;
      }
    }

  public:
    ToneHandler(uint8_t noteBuffer, uint8_t pitchRange) :
      ANALOG_HALF_BEND( ( (uint16_t) pitchRange ) * DAC_SEMI_TONE_VALUE ) ,
      mMIDIDirty(false) ,
      mBend(0) ,
      mCurrentTone(0) ,
      mNoteOverlap(false)
    {}

    bool update()
    {
      return mMIDIDirty || mNoteOverlap;
    }

    void utdated()
    {
      mMIDIDirty = false;
    }

    bool gateOn()
    {
      return ! mNotes.empty();
    }

    void addNote(uint8_t midiNote)
    {
      // remove note if it is already being played
      removeMidiNote(midiNote);
      // add new note with calculated dacVal
      mNotes.push_back(midiNote);
      setOverlap();
      mMIDIDirty = true;
    }

    void removeNote(uint8_t midiNote)
    {
      removeMidiNote( midiNote );
      setOverlap();
    }

    // simple LP filter taken from https://www.embeddedrelated.com/showarticle/779.php
    // input is x. The output is y
    uint16_t glideFilter( uint16_t x, uint16_t y )
    {
      uint16_t alpha = analogRead(KNOB_1_PIN) >> 3;
      if (alpha == 0) return x;
      uint16_t ret;
      //if else: optimized, no unsigned int saves space...
      if (x > y) ret = y + (x - y) / alpha;
      else ret = y - (y - x) / alpha;
      // if: No long slides float on mCurrentTone uses to much space.
      if (ret == y) ret += (x > y) ? 1 : -1;
      return ret;
    }

    uint16_t currentTone()
    {
      if ( mNoteOverlap )
      {
        //TODO: make glideFilter functor or something so chifting can be made between alpegiator etc.
        mCurrentTone = glideFilter(mNextTone, mCurrentTone);
      }
      else
      {
        mCurrentTone = mNextTone;
      }
      return (uint16_t) mCurrentTone + mBend;
    }

    // pitch bend is +/- ANALOG_HALF_BEND semitones
    void addPitch(uint16_t pitch)
    {
      // allow for a slight amount of slack in the middle
      if ( abs(pitch - 64) < 2 ) pitch = 64;
      mBend = map(pitch, 0, MAX_PITCH_MIDI_VALUE, -ANALOG_HALF_BEND, ANALOG_HALF_BEND) ;
      if ( ! mNotes.empty() ) mMIDIDirty = true;
    }
};
  
class Pots
{
  class Pot {
    
    uint8_t mCC = 0;
    uint8_t mValue = 0;
    bool mCCReady = false;
  
  public:

    void cc(uint8_t cc)
    {
      mCC = cc;
    }
  
    void readIfCC(uint8_t cc, uint8_t value)
    {
      mCCReady = true;
      mValue = value << 1;
    }
  
    void writeIfReady(uint8_t potIndex)
    {
      if( mCCReady )
      {
        ns1digipots::write(potIndex, mValue);
        mCCReady = false;
      }
    }
  };
  
  Pot mPots[4];

public:
  
  Pots()
  {
    uint8_t x = MIN_CC;
    for(size_t i = 0 ; i < 4 ; ++i)
      mPots[i].cc(x++);
  }
  
  void write()
  {
    //it is necessary to move the i2c routines out of the callback. 
    //probably due to some interrupt handling!
    for( int i = 0; i < 4; ++i )
    {
      mPots[i].writeIfReady(i);
    }
  }

  void read(uint8_t cc, uint8_t value)
  {
    for( int i = 0; i < 4; ++i )
    {
      mPots[i].readIfCC(cc,value);
    }
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

BoMidi<
  BoMidiFilter<1, MIDITYPE::NOTEON, noteon, keyBetween<MIN_NOTE,MAX_NOTE> > ,
  BoMidiFilter<1, MIDITYPE::NOTEOFF, noteoff, keyBetween<MIN_NOTE,MAX_NOTE> > ,
  BoMidiFilter<1, MIDITYPE::PB, pitch> ,
  BoMidiFilter<1, MIDITYPE::CC, changedCC, controlBetween<MIN_CC, MAX_CC> >,
  BoMidiFilter<1, MIDITYPE::CC, changedMod, controlBetween<1,1> >
> gMidi;

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
  gPots.read(cc,value);
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

void updateNS1()
{
  gMidi.ifMidiDo();
  if ( ! gNotes.update() ) return;
  outputNotes();
  gNotes.utdated();
}

void setup() {
  
  Timer1.initialize(8000);          //check MIDI packets each XXX ms
  Timer1.attachInterrupt(updateNS1); // blinkLED to run every 0.15 seconds

  pinMode( TRIGGER_PIN, OUTPUT );
  pinMode( KNOB_1_PIN, INPUT );

  digitalWrite( TRIGGER_PIN, LOW);
  
  Wire.begin();
  dac.setGain(2);
}



void loop() {
  gPots.write();
}
