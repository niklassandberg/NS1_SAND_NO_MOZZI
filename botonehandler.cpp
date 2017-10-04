#ifndef _TONE_HANDLER_CPP_
#define _TONE_HANDLER_CPP_

#include "botonehandler.h"

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::removeMidiNote(uint8_t note)
{
  size_t index = mNotes.index( note );
  if ( index != mNotes.index_end() )
  {
    mNotes.remove_at(index);
    mMIDIDirty = true;
  }
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::setOverlap(uint8_t noteIndex)
{
  //Setting mCurrentTone = MAX_DAC_KEY_MIDI_MAP_VAL+1;
  //halts the slide if mNotes has one note.
  if (mNotes.size())
  {
    //TODO: cannot put const in template arguments. Why?!
    mNextTone = midikeyToDac<36,97,DAC_SEMI_TONE,MAX_DAC_KEY>( mNotes[noteIndex] );
  }

  if (mNotes.size() && ! (mCurrentTone > MAX_DAC_KEY) )
  {
    mNoteOverlap = mCurrentTone != mNextTone;
  }
  else
  {
    mCurrentTone = MAX_DAC_KEY + 1;
    mNoteOverlap = false;
  }
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::setOverlap()
{
  mNoteIndex = mNotes.size() - 1;
  setOverlap(mNoteIndex);
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::ToneHandler(uint8_t pitchRange) :
  ANALOG_HALF_BEND( ( (uint16_t) pitchRange ) * DAC_SEMI_TONE ) ,
  mMIDIDirty(false) ,
  mBend(0) ,
  mCurrentTone(0) ,
  mNoteOverlap(false) ,
  mNoteIndex(0),
  mKeyMode(NORMAL),
  mAllpegiatorOn(false),
  mGateState(IS_LOW),
  mGateChanged(false)
{}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::allpegiator()
{
  if( ! mAllpegiatorOn ) return;
  else if ( mGateState == IS_LOW || ! mGateChanged ) return;

  if ( mNotes.size() > 1 )
  {
    switch (mKeyMode)
    {
      case ALLPEG_RANDOM :
        mNoteIndex = random(mNotes.size());
        break;
      case ALLPEG_UPDOWN :
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


template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
bool ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::update()
{
  if (mAllpegiatorOn)
    return mGateChanged || mNoteOverlap;
  else
    return mMIDIDirty || mNoteOverlap;
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::utdated()
{
  mMIDIDirty = false;
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
bool ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::gateOn()
{
  //Serial.print("gateOn:"); Serial.println(mNotes.empty());
  if (mAllpegiatorOn)
    return ! mNotes.empty() && mGateState == IS_HIGH;
  else
    return ! mNotes.empty();
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::addNote(uint8_t midiNote)
{
  // remove note if it is already being played
  if(!mHold) removeMidiNote(midiNote);
  mNotes.push_back(midiNote);
  mMIDIDirty = true;
  setOverlap();
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::removeNote(uint8_t midiNote)
{
  //if(mHold) return;
  removeMidiNote( midiNote );
  setOverlap();
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
uint16_t ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::currentTone()
{
  if(mNoteOverlap)
    keyGlide2(mNextTone, mCurrentTone, mGlideFactor);
  else
    mCurrentTone = mNextTone;
  return (uint16_t) mCurrentTone + mBend;
}

// pitch bend is +/- ANALOG_HALF_BEND semitones
template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::addPitch(uint16_t pitch)
{
  // allow for a slight amount of slack in the middle
  if ( abs(pitch - 64) < 2 ) pitch = 64;
  mBend = map(pitch, 0, MAX_PITCH_MIDI_VALUE, -ANALOG_HALF_BEND, ANALOG_HALF_BEND) ;
  if ( ! mNotes.empty() ) mMIDIDirty = true;
}

#endif
