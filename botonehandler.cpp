#include "botonehandler.h"

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

ToneHandler::ToneHandler(uint8_t noteBuffer, uint8_t pitchRange, uint8_t minNote, uint8_t maxNote) :
  ANALOG_HALF_BEND( ( (uint16_t) pitchRange ) * DAC_SEMI_TONE_VALUE ) ,
  mMIDIDirty(false) ,
  mBend(0) ,
  mCurrentTone(0) ,
  mNoteOverlap(false) ,
  mNoteIndex(0),
  mKeyMode(NORMAL),
  mAllpegiatorOn(false),
  mGateState(IS_LOW),
  mGateChanged(false),
  MINNOTE(minNote), MAXNOTE(maxNote)
{}

void ToneHandler::allpegiator()
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


bool ToneHandler::update()
{
  if (mAllpegiatorOn)
    return mGateChanged || mNoteOverlap;
  else
    return mMIDIDirty || mNoteOverlap;
}

void ToneHandler::utdated()
{
  mMIDIDirty = false;
}

bool ToneHandler::gateOn()
{
  //Serial.print("gateOn:"); Serial.println(mNotes.empty());
  if (mAllpegiatorOn)
    return ! mNotes.empty() && mGateState == IS_HIGH;
  else
    return ! mNotes.empty();
}

void ToneHandler::addNote(uint8_t midiNote)
{
  // remove note if it is already being played
  if(!mHold) removeMidiNote(midiNote);
  mNotes.push_back(midiNote);
  mMIDIDirty = true;
  setOverlap();
}

void ToneHandler::removeNote(uint8_t midiNote)
{
  //if(mHold) return;
  removeMidiNote( midiNote );
  setOverlap();
}

uint16_t ToneHandler::currentTone()
{
  if(mNoteOverlap)
    keyGlide2(mNextTone, mCurrentTone, mGlideFactor);
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
