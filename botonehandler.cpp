#ifndef _TONE_HANDLER_CPP_
#define _TONE_HANDLER_CPP_

#include "botonehandler.h"

#include <bodebug.h>

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::removeMidiNote(uint8_t note)
{
  size_t index = mNotes.index( note );
  if ( index != mNotes.index_end() )
  {
    mNotes.remove_at(index);
  }
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::setOverlap(uint8_t noteIndex)
{

  DEBUG_2(SETOVERLAP,"setOverlap: ",noteIndex);
  
  if(mNoteIndex == noteIndex)
  {
    DEBUG(SETOVERLAP,"is already set.");
    return;
  }
  else if(noteIndex > MAXNOTE)
  {
    DEBUG(SETOVERLAP,"empty note was playing");
    //MAXNOTE < mNoteIndex indicates first time played.
    mNoteIndex = noteIndex;
    mNoteOverlap = false;
    return; 
  }


  DEBUG(SETOVERLAP,"Get next tone");
  mNoteIndex = noteIndex;
  mNextTone = midikeyToDac<MINNOTE,MAXNOTE,DAC_SEMI_TONE,MAX_DAC_KEY>( mNotes[noteIndex] );


  //if: First time played new note. Have no slide.
  if(mNoteIndex > MAXNOTE)
  {
    DEBUG(SETOVERLAP,"First time played");
    mNoteOverlap = false;
  }
  else
  {
    DEBUG(SETOVERLAP,"set note overlap");
    mNoteOverlap = mCurrentTone != mNextTone;
  }
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::setOverlap()
{
  if(!mNotes.size()) setOverlap(FIRST_NOTE_INDEX);
  else setOverlap(mNotes.size()-1);
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::ToneHandler(uint8_t pitchRange) :
  ANALOG_HALF_BEND( ( (uint16_t) pitchRange ) * DAC_SEMI_TONE ) ,
  mMIDIDirty(false) ,
  mBend(0) ,
  mCurrentTone(0) ,
  mNoteOverlap(false) ,
  mNoteIndex(FIRST_NOTE_INDEX) ,
  mKeyMode(NORMAL) ,
  mAllpegiatorOn(false) ,
  mTrigState(IS_LOW) ,
  mTrigChanged(false) ,

  mSlideFactor(0) ,
  mNextTone(0) ,
  mHold(false)
{}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
bool ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::normal()
{
  if( mAllpegiatorOn )
    return true;
  
  if (mMIDIDirty) //new key.
  {
    setOverlap();
    return true;
  }
  else if(mNoteOverlap) //multiple notes pressed
  {
    return true;
  }
  return true;
}
/*
if (mAllpegiatorOn)
    return mTrigChanged || mNoteOverlap;
  else
    return mMIDIDirty || mNoteOverlap;
*/
template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
bool ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::allpegiator()
{
  if( ! mAllpegiatorOn ) 
  {
    DEBUG(ALLPEGIATOR,"allpeg: is on");
    return false; //not on
  }
  else if( !mNotes.size() )
  {
    if(mMIDIDirty) setOverlap(FIRST_NOTE_INDEX);
    DEBUG(ALLPEGIATOR,"allpeg: notes not pressed.");
    return false;
  }
  else if ( !(mTrigState == IS_HIGH && mTrigChanged) )
  {
    DEBUG(ALLPEGIATOR,"allpeg: not triggered.");
    return false;
  }

  mMIDIDirty = true;
  
  size_t noteIndex = mNoteIndex;
  switch (mKeyMode)
  {
    case ALLPEG_RANDOM :
      noteIndex = random(mNotes.size());
      break;
    case ALLPEG_UPDOWN :
      static bool up = true;
      if (up) up = noteIndex+1 < mNotes.size();
      else up = noteIndex == 0;
      (up) ? ++noteIndex : --noteIndex;
      break;
    default:
      noteIndex = (noteIndex + 1) % mNotes.size();
      break;
  }
  
  DEBUG_2(ALLPEGIATOR,"allpeg noteIndex: ",noteIndex);
  setOverlap(noteIndex);
  
  return true;
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::utdated()
{
  mMIDIDirty = false;
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
bool ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::gateOn()
{
  if (mAllpegiatorOn)
    return ! mNotes.empty() && mTrigState == IS_HIGH;
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
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
void ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::removeNote(uint8_t midiNote)
{
  removeMidiNote( midiNote );
  mMIDIDirty = true;
}

template<uint8_t NOTESBUFFER, uint8_t MINNOTE, uint8_t MAXNOTE>
uint16_t ToneHandler<NOTESBUFFER,MINNOTE,MAXNOTE>::currentTone()
{
  if(mNoteOverlap)
  {
    mNoteOverlap = mCurrentTone != mNextTone;
    toneSlide(mNextTone, mCurrentTone, mSlideFactor);
    DEBUG_2(CURRENT_TONE,"toneSlide: ",mNoteOverlap);
  }
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
