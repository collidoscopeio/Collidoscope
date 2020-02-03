/*

 Copyright (C) 2015  Fiore Martin
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
#pragma once


#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/Color.h"
#include "cinder/PolyLine.h"
#include "cinder/Rand.h"


#include <vector>
#include <map>

#include "Chunk.h"
#include "DrawInfo.h"
#include "Config.h"


#ifdef USE_PARTICLES
#include "ParticleController.h"
#endif 


class DrawInfo;
typedef int SynthID;

/**
 * Collidoscope's graphical wave
 *
 */
class Wave : private cinder::Noncopyable
{
  friend class ParticleController;

  /**
  * A Cursor is the white thingy that loops through the selection when Collidoscope is played.
  */
  struct Cursor
  {
    int initialPosition;
    int pos;
    double creationTime;
    size_t durationInChunks;
  };


public:

  Wave(cinder::Color selectionColor);

  /** Resetting a wave makes it shrink until it disappears. Each time a new sample is recorded, the wave is reset.  */
  void reset(const DrawInfo& di);

  /** sets top and bottom values for the chunk.
   * \a bottom and \a top are in audio coordinates [-1.0, 1.0]
   */
  void setChunk(size_t index, float bottom, float top);

  const Chunk& getChunk(size_t index) const;

  void setSelectionStart(size_t start);

  void setSelectionSize(size_t size);

  size_t getSelectionStart(void) const { return mSelectionStart; }

  size_t getSelectionSize() const;

  size_t getSelectionEnd(void) const { return mSelectionEnd; }

  /** Places the cursor on the wave. Every cursor is associated to a synth voice of the audio engine.
   *  The synth id identifies uniquely the cursor in the internal map of the wave.
   *  If the cursor doesn't exist it is created */
  void addCursor(SynthID id, const DrawInfo& di, size_t durationInChunks);

  void removeCursors(SynthID id) { mCursors[id].clear(); }

  /** The particle spread parameter affects the size of the cloud of particles
     *  The cloud is the visual counterpart of the grain duration coefficient in sound.
     */
  void setParticleRadiusCoeff(float spread) { mParticleRadiusCoeff = spread; }

  float getParticleRadiusCoeff() const  { return mParticleRadiusCoeff; }

  void update(const DrawInfo& di);

  /** Sets the transparency of this wave. \a alpha ranges from 0 to 1 */
  void setselectionAlpha(float alpha) { mSelectionAlpha = alpha; }

  void draw(const DrawInfo& di);

private:

#ifdef USE_PARTICLES
  ParticleController mParticleController{};
#endif 

  bool hasCursorAtChunk(int chunkIdx) const;

  /* Maps id of the synth to cursor. There is one cursor for each Synth being played: notes + loop */
  std::array<std::vector<Cursor>, Config::MAX_VOICES + 1> mCursors;

  std::vector<Chunk> mChunks;

  bool mSelectionExists = false;
  size_t mSelectionStart = 0;
  size_t mSelectionEnd = 0;

  cinder::Color mColorUnselected{ 0.5f, 0.5f, 0.5f };
  cinder::Color mColorSelected;
  cinder::ColorA mColorSelectionBar;

  float mSelectionAlpha = Config::MAX_ALPHA; // [0.5f, 1.0f]
  float mParticleRadiusCoeff = 1.0f; // [1.0f, 8.0f]

  // cinder gl batch for batch drawing 
  ci::gl::BatchRef mChunkBatch;

};

