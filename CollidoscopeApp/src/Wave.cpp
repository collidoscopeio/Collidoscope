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

#include "Wave.h"
#include "DrawInfo.h"
#include "cinder/CinderMath.h"


using namespace ci;

Wave::Wave(Color selectionColor) :
  mColorSelected(selectionColor),
  mColorSelectionBar(selectionColor, 0.5f)
{
  mChunks.reserve(Config::NUM_CHUNKS);

  for (size_t i = 0; i < Config::NUM_CHUNKS; i++) 
  {
    mChunks.emplace_back(i);
  }

  // init cinder batch drawing
  auto lambert = gl::ShaderDef().color();
  gl::GlslProgRef shader = gl::getStockShader(lambert);
  mChunkBatch = gl::Batch::create(geom::Rect(ci::Rectf(0, 0, Chunk::kWidth, 1)), shader);
}

void Wave::reset(const DrawInfo& di)
{
  for (size_t i = 0; i < Config::NUM_CHUNKS; i++)
  {
    mChunks[i].reset();
  }
}

void Wave::setChunk(size_t index, float bottom, float top)
{
  Chunk &c = mChunks[index];
  c.setTop(top);
  c.setBottom(bottom);
}

inline const Chunk & Wave::getChunk(size_t index) const
{
  return mChunks[index];
}

void Wave::setSelectionStart(size_t start)
{
  /* deselect the previous */
  mChunks[mSelectionStart].setAsSelectionStart(false);
  /* select the next */
  mChunks[start].setAsSelectionStart(true);

  mSelectionExists = true;

  size_t size = getSelectionSize();

  mSelectionStart = start;
  mSelectionEnd = start + size - 1;
  if (mSelectionEnd > Config::NUM_CHUNKS - 1)
    mSelectionEnd = Config::NUM_CHUNKS - 1;

}

void Wave::setSelectionSize(size_t size)
{
  if (size <= 0)
  {
    mSelectionExists = false;
    return;
  }

  size -= 1;

  // check boundaries: size cannot bring the selection end beyond the end of the wave 
  if (mSelectionStart + size >= Config::NUM_CHUNKS) 
  {
    size = Config::NUM_CHUNKS - mSelectionStart - 1;
  }

  /* deselect the previous */
  mChunks[mSelectionEnd].setAsSelectionEnd(false);

  mSelectionEnd = mSelectionStart + size;
  /* select the next */
  mChunks[mSelectionEnd].setAsSelectionEnd(true);

  mSelectionExists = true;

}

size_t Wave::getSelectionSize() const
{
  if (!mSelectionExists) 
    return 0;
  else 
    return 1 + mSelectionEnd - mSelectionStart;
}

void Wave::addCursor(SynthID id, const DrawInfo& di, size_t durationInChunks)
{
  Cursor newCursor{};
  newCursor.initialPosition = getSelectionStart();
  newCursor.pos = getSelectionStart();
  newCursor.creationTime =  ci::app::getElapsedSeconds();
  newCursor.durationInChunks = durationInChunks;

  mCursors[id].push_back(newCursor);
}

void Wave::update(const DrawInfo& di) 
{

  // update the cursor positions
  double now = ci::app::getElapsedSeconds();
  
  for (auto& voiceCursors : mCursors)
  {
    for (Cursor& cursor : voiceCursors)
    {
      if (!mSelectionExists)
      {
        cursor.pos = -1;
      }

      if (cursor.pos == -1)
        continue;

      double elapsed = now - cursor.creationTime;

      // A chunk of audio corresponds to a certain time lenght of audio, according to sample rate.
      // Use elapsed time to advance through chunks so that the cursor is animated. 
      // So it goes from start to end of the selection in the time span of the grain 
      cursor.pos = cursor.initialPosition + int(elapsed / Config::CHUNK_LEN_SECONDS);

    }

    voiceCursors.erase(std::remove_if(voiceCursors.begin(),
                                      voiceCursors.end(),
                                      [](const Wave::Cursor& c) { return c.pos - c.initialPosition >= c.durationInChunks || c.pos == -1; }),
                       voiceCursors.end());
  }



  // update chunks for animation 
  for (auto &chunk : mChunks)
  {
    chunk.update(di);
  }

#ifdef USE_PARTICLES

  const int randSChunkInSelection = Rand::randInt(0, getSelectionSize());
  const int centerChunkIndex = getSelectionStart() + randSChunkInSelection;
  float cloudCentreX = 1.0f + (centerChunkIndex * (2.0f + Chunk::kWidth)) + Chunk::kWidth / 2.0f;
  const float wavePixelLen = Config::NUM_CHUNKS * (2 + Chunk::kWidth);
  cloudCentreX *= float(di.getWindowWidth()) / wavePixelLen;
  
  const vec2 cloudCentre{ cloudCentreX, di.getWaveCenterY() };
  const float radius = [this]()
  {
    auto nonEmpty = std::find_if(mCursors.begin(), mCursors.end(), [](const std::vector<Cursor>& c) { return !c.empty(); });
    if (nonEmpty != mCursors.end())
      return ci::lmap<float>(mParticleRadiusCoeff, 1, Config::MAX_DURATION_COEFF, 0, (ci::app::getWindowHeight() / NUM_WAVES) * 0.5f);
    else
      return 0.0f; // all empty, no notes is playing 
  }();

  mParticleController.updateParticles(cloudCentre, radius, mSelectionAlpha);
#endif

}

void Wave::draw(const DrawInfo& di) 
{

  /* ########### draw the particles ########## */
#ifdef USE_PARTICLES
  mParticleController.draw();
#endif

  /* ########### draw the wave ########## */
  /* scale the wave to fit the window */
  gl::pushModelView();

  const float wavePixelLen = (Config::NUM_CHUNKS * (2 + Chunk::kWidth));
  /* scale the x-axis for the wave to fit the window precisely */
  gl::scale(((float)di.getWindowWidth()) / wavePixelLen, 1.0f);

  /* draw the chunks */
  if (!mSelectionExists)
  {
    /* no selection: all chunks the same color */
    gl::color(mColorUnselected);
    for (size_t i = 0; i < Config::NUM_CHUNKS; i++)
    {
      mChunks[i].draw(di, mChunkBatch);
    }
  }
  else
  {
    gl::enableAlphaBlending();

    cinder::ColorA colorForSelection{ mColorSelected, mSelectionAlpha };

    gl::color(mColorSelectionBar);
    mChunks[getSelectionStart()].drawBar(di, mChunkBatch);
    mChunks[getSelectionEnd()].drawBar(di, mChunkBatch);

    for (size_t chunkIdx = 0; chunkIdx < Config::NUM_CHUNKS; chunkIdx++)
    {
      
      if (hasCursorAtChunk(chunkIdx))
      {
        const Color cursorColor{1.0f, 1.0f, 1.0f};
        gl::color(cursorColor);
        mChunks[chunkIdx].draw(di, mChunkBatch);
      }
      else if (chunkIdx >= getSelectionStart() && chunkIdx <= getSelectionEnd())
      {
        gl::color(colorForSelection);
      }
      else
      {
        gl::color(mColorUnselected);
      }

      mChunks[chunkIdx].draw(di, mChunkBatch);
    }
      
    gl::disableAlphaBlending();
  }

  gl::popModelView();

}

bool Wave::hasCursorAtChunk(int chunkIdx) const
{
  for (const std::vector<Cursor>& voiceCursors : mCursors)
  {
    for (const Cursor& cursor : voiceCursors)
    {
      if (cursor.pos % Config::NUM_CHUNKS == chunkIdx) return true;
    }
  }

  return false;
}



