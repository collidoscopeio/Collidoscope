/*

 Copyright (C) 2016  Queen Mary University of London
 Author: Fiore Martin

 This file is part of Collidoscope.

 Collidoscope is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PGranularNode.h"

#include "cinder/audio/Context.h"

PGranularNode::PGranularNode(ci::audio::Buffer *grainBuffer, RingBuf<CursorTriggerMsg>& triggerRingBuf, RingBuf<NoteMsg>& noteRingBuf):
  Node(Format().channels(1)),
  mGrainBuffer(grainBuffer),
  mTriggerRingBuf(triggerRingBuf),
  mNoteRingBuf(noteRingBuf)
{
  for (int i = 0; i < Config::MAX_VOICES; i++)
  {
    mMidiNotes[i] = kNoMidiNote;
  }
}


void PGranularNode::initialize()
{
  mTempBuffer = std::make_shared< ci::audio::Buffer >(getFramesPerBlock());

  auto callback = [this](collidoscope::PGranular<float>::TriggerType triggerType, int ID, size_t grainDuration)
  {
    cursorCallback(triggerType, ID, grainDuration);
  };

  /* create the PGranular object for notes */
  for (size_t i = 0; i < Config::MAX_VOICES; i++)
  {
    mPGranularNotes[i].reset(new PGranularType(mGrainBuffer->getData(), mGrainBuffer->getNumFrames(), getSampleRate(), callback, i));
  }

  /* create the PGranular object for looping */
  mPGranularLoop.reset(new PGranularType(mGrainBuffer->getData(), mGrainBuffer->getNumFrames(), getSampleRate(), callback, Config::MAX_VOICES));
}

void PGranularNode::process(ci::audio::Buffer *buffer)
{
  
  const size_t selectionSize = mSelectionSize.load();
  mPGranularLoop->setSelectionSize(selectionSize);
  for (size_t i = 0; i < Config::MAX_VOICES; i++)
  {
    mPGranularNotes[i]->setSelectionSize(selectionSize);
  }

  const size_t selectionStart = mSelectionStart.load();
  mPGranularLoop->setSelectionStart(selectionStart);
  for (size_t i = 0; i < Config::MAX_VOICES; i++)
  {
      mPGranularNotes[i]->setSelectionStart(selectionStart);
  }

  const float grainDurationCoeff = mGrainDurationCoeff.load();
  mPGranularLoop->setGrainsDurationCoeff(grainDurationCoeff);
  for (size_t i = 0; i < Config::MAX_VOICES; i++)
  {
      mPGranularNotes[i]->setGrainsDurationCoeff(grainDurationCoeff);
  }

  // check messages to start/stop notes or loop 
  size_t availableRead = mNoteRingBuf.getAvailableRead();
  CI_ASSERT(availableRead <= Config::NOTE_RINGBUF_LEN);

  NoteMsg notesArray[Config::NOTE_RINGBUF_LEN];
  mNoteRingBuf.read(notesArray, availableRead);

  for (size_t i = 0; i < availableRead; i++)
  {
    handleNoteMsg(notesArray[i]);
  }

  // process loop if not idle 
  if (!mPGranularLoop->isIdle()) 
  {
    /* buffer is one channel only so I can use getData */
    mPGranularLoop->process(buffer->getData(), mTempBuffer->getData(), buffer->getSize());
  }

  // process notes if not idle 
  for (size_t i = 0; i < Config::MAX_VOICES; i++)
  {
    if (mPGranularNotes[i]->isIdle())
      continue;

    mPGranularNotes[i]->process(buffer->getData(), mTempBuffer->getData(), buffer->getSize());

    if (mPGranularNotes[i]->isIdle())
    {
      // this note became idle so update mMidiNotes
      mMidiNotes[i] = kNoMidiNote;
    }

  }
}

void PGranularNode::cursorCallback(collidoscope::PGranular<float>::TriggerType triggerType, int ID, size_t grainDuration)
{
  switch (triggerType)
  {
    case PGranularType::TriggerType::NewGrainCreated:
    {
      CursorTriggerMsg msg{ CursorTriggerMsg::Type::NEW_TRIGGER, ID, grainDuration }; // put ID 
      mTriggerRingBuf.write(&msg, 1);
      break;
    };

    case PGranularType::TriggerType::BecameIdle:
    {
      CursorTriggerMsg msg{ CursorTriggerMsg::Type::TRIGGER_END, ID, 0 }; // put ID 
      mTriggerRingBuf.write(&msg, 1);
      break;
    }
  }
}

void PGranularNode::handleNoteMsg(const NoteMsg &msg)
{
  switch (msg.cmd) 
  {
  case Command::NOTE_ON: 
  {
    bool synthFound = false;

    for (int i = 0; i < Config::MAX_VOICES; i++)
    {
      // note was already on, so re-attack
      if (mMidiNotes[i] == msg.midiNote)
      {
        mPGranularNotes[i]->noteOn(msg.rate);
        synthFound = true;
        break;
      }
    }

    if (!synthFound) 
    {
      // then look for a free voice 
      for (int i = 0; i < Config::MAX_VOICES; i++)
      {
        if (mMidiNotes[i] == kNoMidiNote) 
        {
          mPGranularNotes[i]->noteOn(msg.rate);
          mMidiNotes[i] = msg.midiNote;
          synthFound = true;
          break;
        }
      }
    }

    break;
  };

  case Command::NOTE_OFF: 
  {
    for (int i = 0; i < Config::MAX_VOICES; i++)
    {
      if (!mPGranularNotes[i]->isIdle() && mMidiNotes[i] == msg.midiNote) 
      {
        mPGranularNotes[i]->noteOff();
        break;
      }
    }
    break;
  };

  case Command::LOOP_ON: 
  {
    mPGranularLoop->noteOn(1.0);
    break;
  };

  case Command::LOOP_OFF: 
  {
    mPGranularLoop->noteOff();
    break;
  }

  default: CI_ASSERT(false);
  }
}
