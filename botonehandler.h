
// ------------------- FIXED CONSTANTS ---------------------------
// -- !!! DO NOT CHANGE IF YOU DONT KNOW WHAT YOU ARE DOING !!! --
// ---------------------------------------------------------------

// Most significant bit and least significant bit at pitch:
// ([0-127] << 7) + [0-127] = [0-16383]
#define MAX_PITCH_MIDI_VALUE 16383
#define MAX_DAC_KEY 4095

// MCP4922 is 12 bit DAC. 2^12 = 4096
#define DAC_MAX_VALUE 4096
// DAC_SEMI_TONE
#define DAC_SEMI_TONE 68

//FIRST_NOTE_INDEX indicates that new tone is pressed.
//By that no slide of 'keyOverlap'
#define FIRST_NOTE_INDEX 255





#ifndef _TONE_HANDLER_H_
#define _TONE_HANDLER_H_

#include <bofilters.h>

#ifdef MOCK_ARDUINO
#include <arduinomock.h>
#else
#include <Arduino.h>
#endif


#include <boarraycontainer.h>

enum KeyMode {
  NORMAL,
  ALLPEG_UP,
  ALLPEG_UPDOWN,
  ALLPEG_RANDOM
};

enum TrigState
{
  IS_HIGH,
  IS_LOW
};

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
class ToneHandler
{

    const int16_t ANALOG_HALF_BEND;
    
    //indicates that midi has changed the tone.
    bool mMIDIDirty;
    
    //pitch value for dac.
    int16_t mBend;

    int16_t mCurrentTone;
    bool mNoteOverlap;
    size_t mNoteIndex;
    
    KeyMode mKeyMode;
    
    bool mAllpegiatorOn;
  
    TrigState mTrigState;
    
    bool mTrigChanged;

    uint8_t mSlideFactor;
  

    int16_t mNextTone;

    bool mHold;

    //notes beging pressed down.
    array_container<uint8_t, NOTESBUFFER> mNotes;

    void removeMidiNote(uint8_t note);
    void setOverlap(uint8_t noteIndex);
    void setOverlap();

  public:
  
    ToneHandler(uint8_t pitchRange);

    void utdated();
    bool gateOn();
    void addNote(uint8_t midiNote);
    void removeNote(uint8_t midiNote);
    
    //Todo: this two needs to be merge and . Same func call, 
    bool allpegiator();
    bool normal();
    
    void allpegiatorOn() { mAllpegiatorOn = true; }
    void allpegiatorOff() { mAllpegiatorOn = false; }
    uint16_t currentTone();

    void trig( TrigState state ) { mTrigState = state; }
    void trig( bool changed) { mTrigChanged = changed; }

    void mode(KeyMode keyMode) { mKeyMode = keyMode; }
    
    void addPitch(uint16_t pitch); // pitch bend is +/- ANALOG_HALF_BEND semitones

    void slide(uint8_t factor) {
      factor <<= 1;
      mSlideFactor = ~factor; // 0-127 => 255-0
    }

    void hold(bool state) {
      mHold = state;
      if(!mHold)
        mNotes.clear(); 
    }
};

//template 
#include "botonehandler.cpp"

#endif
