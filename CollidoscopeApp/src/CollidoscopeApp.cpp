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


#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Exception.h"
#include <stdexcept>


#include "Config.h"
#include "Wave.h"
#include "DrawInfo.h"
#include "Log.h"
#include "AudioEngine.h"
#include "Oscilloscope.h"
#include "Messages.h"
#include "MIDI.h"

using namespace ci;
using namespace ci::app;

using namespace std;


class CollidoscopeApp : public App
{
public:

  void setup() override;
  void setupGraphics();

  /** Receives MIDI command messages from MIDI thread */
  void readMidiMessages();

  void keyDown(KeyEvent event) override;
  void update() override;
  void draw() override;
  void resize() override;

  Config mConfig;
  collidoscope::MIDI mMIDI;
  AudioEngine mAudioEngine;
  size_t mSamplesPerChunk;

  array< shared_ptr< Wave >, NUM_WAVES > mWaves;
  array< shared_ptr< DrawInfo >, NUM_WAVES > mDrawInfos;
  array< shared_ptr< Oscilloscope >, NUM_WAVES > mOscilloscopes;
  // vector to read the WAVE_* messages as a new wave gets recorded 
  vector< RecordWaveMsg > mWaveRecordingMessages;

  //buffer to read the TRIGGER_* messages as the pgranulars play
  vector< CursorTriggerMsg > mCursorTriggerMessages;
};


void CollidoscopeApp::setup()
{
  static_assert(NUM_WAVES == 1 || NUM_WAVES == 2, "Either one or two waves");

  hideCursor();
  /* setup is logged: setup steps and errors */

  /*try {
      mConfig.loadFromFile( "./collidoscope_config.xml" );
  }
  catch ( const Exception &e ){
      logError( string("Exception loading config from file:") + e.what() );
  }*/

  mAudioEngine.setup(mConfig);

  mSamplesPerChunk = mAudioEngine.getSampleRate() * Config::CHUNK_LEN_SECONDS;

  setupGraphics();

  mMIDI.setup(mConfig);
  
}

void CollidoscopeApp::setupGraphics()
{
  for (size_t waveIdx = 0; waveIdx < NUM_WAVES; waveIdx++)
  {
    mDrawInfos[waveIdx] = make_shared< DrawInfo >(waveIdx);
    mWaves[waveIdx] = make_shared< Wave >(mConfig.getWaveSelectionColor(waveIdx));
    mOscilloscopes[waveIdx] = make_shared< Oscilloscope >(mAudioEngine.getAudioOutputBufferRef(waveIdx).getNumFrames() / Config::OSCILLOSCOPE_POINTS_STRIDE);
  }
}

void CollidoscopeApp::keyDown(KeyEvent event)
{
  char c = event.getChar();

  const size_t waveIdx = 0;

  switch (c) 
  {
  case 'r':
  {
    mAudioEngine.record(waveIdx);
    break;
  }

  case 'w': 
  {
    mWaves[waveIdx]->setSelectionSize(mWaves[waveIdx]->getSelectionSize() + 1);

    size_t numSelectionChunks = mWaves[waveIdx]->getSelectionSize();
    // how many samples in one selection ?
    size_t selectionSize = numSelectionChunks * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS);

    mAudioEngine.setSelectionSize(waveIdx, selectionSize);
    break;
  };

  case 's': 
  {
    mWaves[waveIdx]->setSelectionSize(mWaves[waveIdx]->getSelectionSize() - 1);

    size_t selectionSize = mWaves[waveIdx]->getSelectionSize() * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS);
    mAudioEngine.setSelectionSize(waveIdx, selectionSize);
    break;
  };

  case 'd': 
  {

    size_t selectionStart = mWaves[waveIdx]->getSelectionStart();
    mWaves[waveIdx]->setSelectionStart(selectionStart + 1);

    selectionStart = mWaves[waveIdx]->getSelectionStart();
    mAudioEngine.setSelectionStart(waveIdx, selectionStart * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS));
    break;
  };

  case 'a': 
  {
    size_t selectionStart = mWaves[waveIdx]->getSelectionStart();

    if (selectionStart == 0)
      return;

    mWaves[waveIdx]->setSelectionStart(selectionStart - 1);

    selectionStart = mWaves[waveIdx]->getSelectionStart();

    mAudioEngine.setSelectionStart(waveIdx, selectionStart * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS));
    break;
  };

  case 'f':
  {
    setFullScreen(!isFullScreen());
    break;
  }

  case ' ': 
  {
    static bool isOn = false;
    isOn = !isOn;
    if (isOn) 
      mAudioEngine.loopOn(waveIdx);
    else 
      mAudioEngine.loopOff(waveIdx);
    break;
  };

  case '9': 
  {
    int c = mWaves[waveIdx]->getParticleRadiusCoeff();
    if (c == 1)
      return;
    else
      c -= 1;

    mAudioEngine.setGrainDurationCoeff(waveIdx, c);
    mWaves[waveIdx]->setParticleRadiusCoeff(float(c));
    break;
  }; 

  case '0': 
  {
    int c = mWaves[waveIdx]->getParticleRadiusCoeff();
    
    if (c == int(Config::MAX_DURATION_COEFF))
      return;
    else
      c += 1;

    mAudioEngine.setGrainDurationCoeff(waveIdx, c);
    mWaves[waveIdx]->setParticleRadiusCoeff(float(c));

    break;
  };
  }

}

void CollidoscopeApp::update()
{
  // check incoming MIDI messages  
  readMidiMessages();

  // check new wave chunks from recorder buffer 
  for (size_t waveIdx = 0; waveIdx < NUM_WAVES; waveIdx++)
  {
    mAudioEngine.getWaveRecordingMessages(waveIdx, mWaveRecordingMessages);

    for (size_t msgIndex = 0; msgIndex < mWaveRecordingMessages.size(); msgIndex++)
    {
      const RecordWaveMsg& msg = mWaveRecordingMessages[msgIndex];

      if (msg.cmd == Command::WAVE_CHUNK)
      {
        mWaves[waveIdx]->setChunk(msg.index, msg.arg1, msg.arg2);
      }
      else if (msg.cmd == Command::WAVE_START)
      {
        mWaves[waveIdx]->reset(*mDrawInfos[waveIdx]);
      }

    }
  }

  // check if new cursors have been triggered 
  for (size_t i = 0; i < NUM_WAVES; i++)
  {

    mAudioEngine.getCursorTriggers(i, mCursorTriggerMessages);

    for (CursorTriggerMsg& trigger : mCursorTriggerMessages)
    {
      const int nodeID = trigger.synthID;

      switch (trigger.type)
      {

      case CursorTriggerMsg::Type::NEW_TRIGGER:
      {
        const size_t durationInChunks = trigger.durationInSamples / mSamplesPerChunk;
        mWaves[i]->addCursor(nodeID, *mDrawInfos[i], durationInChunks);
        break;
      };

      case CursorTriggerMsg::Type::TRIGGER_END:
      {
        mWaves[i]->removeCursors(nodeID);
        break;
      };

      }

    }
  }

  // update cursors 
  for (size_t i = 0; i < NUM_WAVES; i++)
  {
    mWaves[i]->update(*mDrawInfos[i]);
  }

  // update oscilloscope 

  for (size_t i = 0; i < NUM_WAVES; i++)
  {
    const audio::Buffer &audioOutBuffer = mAudioEngine.getAudioOutputBufferRef(i);

    for (size_t pointIdx = 0; pointIdx < mOscilloscopes[i]->getNumPoints(); pointIdx++)
    {
      mOscilloscopes[i]->setPoint(pointIdx, audioOutBuffer.getData()[pointIdx], *mDrawInfos[i]);
    }
  }

}

void CollidoscopeApp::draw()
{
  gl::clear(Color(0, 0, 0));

  // First Wave
  mOscilloscopes[0]->draw();
  mWaves[0]->draw(*mDrawInfos[0]);

  // Second Wave
  if (NUM_WAVES == 2)
  {
    /* for the upper wave flip the x over the center of the screen which is
     the composition of rotate on the y-axis and translate by -screenwidth*/
    gl::pushModelMatrix();
    gl::rotate(float(M_PI), ci::vec3(0, 1, 0));
    gl::translate(float(-getWindowWidth()), 0.0f);
    mOscilloscopes[1]->draw();
    mWaves[1]->draw(*mDrawInfos[1]);
    gl::popModelMatrix();
  }
}

void CollidoscopeApp::resize()
{
  App::resize();

  for (int i = 0; i < NUM_WAVES; i++)
  {
    // reset the drawing information with the new windows size and same shrink factor  
    mDrawInfos[i]->reset(getWindow()->getBounds());

    /* reset the oscilloscope points to zero */
    for (int j = 0; j < mOscilloscopes[i]->getNumPoints(); j++)
    {
      mOscilloscopes[i]->setPoint(j, 0.0f, *mDrawInfos[i]);
    }
  }
}



void CollidoscopeApp::readMidiMessages()
{
  // check new midi messages 
  static std::vector<collidoscope::MIDIMessage> midiMessages{};
  mMIDI.getMidiMessages(midiMessages);

  for (auto &m : midiMessages) 
  {

    const size_t waveIdx = mConfig.getWaveForMIDIChannel(0);
    if (waveIdx >= NUM_WAVES)
      continue;

    if (m.getVoice() == collidoscope::MIDIMessage::Voice::eNoteOn) 
    {
      int midiNote = m.getData_1();
      unsigned int velocity = m.getData_2();

      if (velocity == 0) 
      {
        mAudioEngine.noteOff(waveIdx, midiNote);
      }
      else 
      {
        mAudioEngine.noteOn(waveIdx, midiNote);
      }
    }
    else if (m.getVoice() == collidoscope::MIDIMessage::Voice::eNoteOff) 
    {
      int midiNote = m.getData_1();
      mAudioEngine.noteOff(waveIdx, midiNote);
    }
    else if (m.getVoice() == collidoscope::MIDIMessage::Voice::eControlChange) 
    {
      switch (m.getData_1()) //controller number 
 
      { 
        case 1: // selection position
        {
          const size_t midiVal = m.getData_2();
          size_t selectionPos = ci::lmap<size_t>(midiVal, 0, 127, 0, Config::NUM_CHUNKS-1);
	  
	      const size_t selectionSizeBeforeStartUpdate = mWaves[waveIdx]->getSelectionSize();
	      mWaves[waveIdx]->setSelectionStart(selectionPos);

	      mAudioEngine.setSelectionStart(waveIdx, selectionPos * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS));

	      const size_t newSelectionSize = mWaves[waveIdx]->getSelectionSize();
	      if (selectionSizeBeforeStartUpdate != newSelectionSize) 
	      {
            mAudioEngine.setSelectionSize(waveIdx, newSelectionSize * (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS));
          }
          break;
        }
        case 2: // selection size 
        { 
          const size_t midiVal = m.getData_2();
          size_t numSelectionChunks = ci::lmap<size_t>(midiVal, 0, 127, 1, mConfig.getMaxSelectionNumChunks());

          mWaves[waveIdx]->setSelectionSize(numSelectionChunks);

          // how many samples in one selection ?
          size_t selectionSize = mWaves[waveIdx]->getSelectionSize() * 
                                (Config::WAVE_LEN_SECONDS * mAudioEngine.getSampleRate() / Config::NUM_CHUNKS);
          mAudioEngine.setSelectionSize(waveIdx, selectionSize);

          break;
        };

        case 3: // duration  
        { 
          const float midiVal = m.getData_2(); // 0-127
          const float coeff = ci::lmap<float>(midiVal, 0.0f, 127.0f, 1.0f, Config::MAX_DURATION_COEFF);
          mAudioEngine.setGrainDurationCoeff(waveIdx, coeff);
          mWaves[waveIdx]->setParticleRadiusCoeff(float(coeff));
          break;
        };

        case 4: // filter  
        { 
          const double midiVal = m.getData_2(); // 0-127
          const double minCutoff = mConfig.getMinFilterCutoffFreq();
          const double maxCutoff = mConfig.getMaxFilterCutoffFreq();
          const double cutoff = pow(maxCutoff / 200., midiVal / 127.0) * minCutoff;
          mAudioEngine.setFilterCutoff(waveIdx, cutoff);
          const float alpha = ci::lmap<double>(midiVal, 0.0f, 127.0f, Config::MIN_ALPHA, Config::MAX_ALPHA);
          mWaves[waveIdx]->setselectionAlpha(alpha);
          break;
        };

        case 5: // loop on off 
        { 
          static bool isOn = false;
          unsigned char midiVal = m.getData_2();

          if (midiVal > 0)
          {
            isOn = !isOn;
            isOn ? mAudioEngine.loopOn(waveIdx) : mAudioEngine.loopOff(waveIdx);
          }
            
          break;
        };

        case 6: // trigger record
        {
          unsigned char midiVal = m.getData_2();

          if (midiVal > 0)
            mAudioEngine.record(waveIdx);
          break;
        };

      }
    }
  }

  midiMessages.clear();
}



CINDER_APP(CollidoscopeApp, RendererGl, [](App::Settings *settings) 
{
  const std::vector< string > args = settings->getCommandLineArgs();

  int width = 0;
  int height = 0;

  if (args.size() == 3)
  {
    width = std::stoi(args[1]);
    height = std::stoi(args[2]);
  }
  else
  {
    console() << "Error: invalid arguments" << std::endl;
    console() << "Usage: ./CollidoscopeApp window_width window_height" << std::endl;
    console() << "For example: ./CollidoscopeApp 1024 768 " << std::endl;

    width = 1024;
    height = 768;
    //settings->setShouldQuit(true);
  }

  settings->setFullScreen(true);
  settings->setWindowSize(width, height);
  settings->setMultiTouchEnabled(false);
  settings->disableFrameRate();
})
