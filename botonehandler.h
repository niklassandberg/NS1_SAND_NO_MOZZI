#ifndef _TONE_HANDLER_H_
#define _TONE_HANDLER_H_

#include <Arduino.h>
#include <boarraycontainer.h>

#include <bofilters.h>

#define NOTES_BUFFER 127


// ------------------- FIXED CONSTANTS ---------------------------
// -- !!! DO NOT CHANGE IF YOU DONT KNOW WHAT YOU ARE DOING !!! --
// ---------------------------------------------------------------

// Most significant bit and least significant bit at pitch:
// ([0-127] << 7) + [0-127] = [0-16383]
#define MAX_PITCH_MIDI_VALUE 16383
#define MAX_DAC_KEY_MIDI_MAP_VAL 4095

// MCP4922 is 12 bit DAC. 2^12 = 4096
#define DAC_MAX_VALUE 4096
// DAC_SEMI_TONE_VALUE
#define DAC_SEMI_TONE_VALUE 68

enum KeyMode {
  NORMAL,
  ALLPEG_UP,
  ALLPEG_UPDOWN,
  ALLPEG_RANDOM
};

enum GateState
{
  IS_HIGH,
  IS_LOW
};

class ToneHandler
{
  bool mAllpegiatorOn;
  KeyMode mKeyMode;

  bool mGateChanged;
  GateState mGateState;

    uint16_t mGlideFactor;
  
    const int16_t ANALOG_HALF_BEND;
    const uint8_t MINNOTE;
    const uint8_t MAXNOTE;

    uint16_t mCurrentTone;
    uint16_t mNextTone;
    bool mNoteOverlap;

    bool mHold;

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

uint16_t midiToDacVal(uint8_t midiVal)
{
  uint16_t dacVal = map(midiVal, MINNOTE, MAXNOTE - 1, 0, MAX_DAC_KEY_MIDI_MAP_VAL);
  dacVal += (2 & midiVal) ? 1 : 0; //correlate dac semitones based on analyse.
  return dacVal;
}

  public:
  
    ToneHandler(uint8_t noteBuffer, uint8_t pitchRange, uint8_t minNote, uint8_t maxNote);

    bool update();
    void utdated();
    bool gateOn();
    void addNote(uint8_t midiNote);
    void removeNote(uint8_t midiNote);
    
    //Todo: this two needs to be merge and . Same func call, 
    void allpegiator();
    void allpegiatorOn() { mAllpegiatorOn = true; }
    void allpegiatorOff() { mAllpegiatorOn = false; }
    uint16_t currentTone();

    void gate( GateState state ) { mGateState = state; }
    void gate( bool changed) { mGateChanged = changed; }

    void mode(KeyMode keyMode) { mKeyMode = keyMode; }
    
    void addPitch(uint16_t pitch); // pitch bend is +/- ANALOG_HALF_BEND semitones

    void glide(uint16_t factor) { mGlideFactor = factor; }

    void hold(bool state) {
      mHold = state;
      if(!mHold)
        mNotes.clear(); 
    }
};

#endif