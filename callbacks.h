#ifndef _SAND_CALLBACKS_H_
#define _SAND_CALLBACKS_H_

namespace midi
{
  void noteon(uint8_t note, uint8_t velocity);
  void noteoff(uint8_t note, uint8_t velocity);
  void pitch(uint8_t lsb, uint8_t msb);
  void changedMod(uint8_t cc, uint8_t value);
  void changedCC(uint8_t cc, uint8_t value);
};

namespace sync {
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

void outputNotes();

#endif
