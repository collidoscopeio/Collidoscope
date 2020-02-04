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

#include <vector>
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/app/App.h"

enum class PARTICLE_ENGINE_TYPE : int 
{
  TRANSFORM_FEEDBACK_PARTICLES = 0,
  CPU_RENDERED_PARTICLES = 1, 
};

template<PARTICLE_ENGINE_TYPE T>
class ParticleController;

template<>
class ParticleController<PARTICLE_ENGINE_TYPE::CPU_RENDERED_PARTICLES>
{
  static const int kMaxParticles = 8000;
  static constexpr float kMinDumping = 0.45f;
  static constexpr float kMaxDumping = 1.0f;

public:
  ParticleController()
  {

    using namespace ci;

    const vec2 cloudCentre{ ci::app::getWindowCenter() };

    mParticles.assign(kMaxParticles, {});

    for (int i = 0; i < mParticles.size(); ++i)
    {	// assign starting values to particles.

      auto &p = mParticles.at(i);
      p.pos = cloudCentre;
      p.cloudCentre = cloudCentre;
      p.dir = ci::randVec2() * Rand::randFloat(1.0f, 5.0f);
      p.damping = Rand::randFloat(kMinDumping, kMaxDumping);
      p.lifeSpan = Rand::randFloat(0.0f, 100.0f);
    }

    mParticleVbo = gl::Vbo::create(GL_ARRAY_BUFFER, mParticles, GL_DYNAMIC_DRAW);

    geom::BufferLayout particleLayout {};
    particleLayout.append(geom::Attrib::POSITION, 2, sizeof(vec2), 0);

    auto mesh = gl::VboMesh::create(mParticles.size(), GL_POINTS, { { particleLayout, mParticleVbo } });

#if defined( CINDER_GL_ES )
    auto glsl = gl::GlslProg::create( gl::GlslProg::Format()
      .vertex( CI_GLSL( 100,
        uniform mat4 ciModelViewProjection;
        attribute vec4 ciPosition;

        void main(void) 
	{
          gl_Position = ciModelViewProjection * ciPosition;
          gl_PointSize = 1.0;
        }
      ))
      .fragment( CI_GLSL( 100,
        precision highp float;

        void main(void) 
	{
          gl_FragColor = vec4(1);
        }
      ))
    );
#else
    auto glsl = gl::GlslProg::create(gl::GlslProg::Format()
      .vertex(CI_GLSL(150,
        uniform mat4 ciModelViewProjection;
        in vec4 ciPosition;

        void main(void) 
        {
          gl_Position = ciModelViewProjection * ciPosition;
          gl_PointSize = 1.0;
        }
      ))
      .fragment(CI_GLSL(150,
        out vec4 oColor;

        void main(void) 
	{
          oColor = vec4(1.0f);
        }
      ))
    );
#endif
    mParticleBatch = gl::Batch::create(mesh, glsl);

  }


  void updateParticles(const ci::vec2& cloudCentre, float radius, float damping)
  {

    using namespace ci;

    for (Particle& p : mParticles)
    {
      p.pos += p.dir;

      
      if (p.damping > damping ||                                     // -> filters out some particle to make the cloud less dense when the sound is low pass filtered 
         ci::length(p.pos - p.cloudCentre) > radius - p.lifeSpan)    // -> reset particle to cloud centre if it's gone beyond the cloud radius 
      {
        p.cloudCentre = cloudCentre;
        p.pos = cloudCentre;
      }
    }

    // Copy particle data onto the GPU.
    // Map the GPU memory and write over it.
    void *gpuMem = mParticleVbo->mapReplace();
    memcpy(gpuMem, mParticles.data(), mParticles.size() * sizeof(vec2));
    mParticleVbo->unmap();
  }

  void draw()
  {
    mParticleBatch->draw();
  }

private:

  struct Particle
  {
    ci::vec2 pos;
    ci::vec2 dir;
    ci::vec2 cloudCentre;
    float lifeSpan;
    float damping;
  };

  std::vector<Particle> mParticles;

  ci::gl::VboRef mParticleVbo; 
  ci::gl::BatchRef mParticleBatch;

};


/**
 * The ParticleController creates/updates/draws and destroys particles
 */

#ifdef CINDER_GL_HAS_TRANSFORM_FEEDBACK
template<>
class ParticleController<PARTICLE_ENGINE_TYPE::TRANSFORM_FEEDBACK_PARTICLES>
{
  static const int kMaxParticles = 8000;
  static constexpr float kMinDumping = 0.45f;
  static constexpr float kMaxDumping = 1.0f;
  static constexpr size_t kNumParticleBuffers = 2;

public:
  /**
   * Every time addParticles is run, up to kMaxParticleAdd are added at once
   */

  ParticleController() 
  {
    using namespace ci;

    struct Particle
    {
      ci::vec2 pos;
      ci::vec2 dir;
      ci::vec2 cloudCentre;
      float lifeSpan;
      float damping;
    };

    std::vector<Particle> particles;
    const vec2 cloudCentre{ ci::app::getWindowCenter() };

    particles.assign(kMaxParticles, {});

    for (int i = 0; i < particles.size(); ++i)
    {	// assign starting values to particles.

      auto &p = particles.at(i);
      p.pos = cloudCentre;
      p.cloudCentre = cloudCentre;
      p.dir = ci::randVec2() * Rand::randFloat(1.0f, 5.0f);
      p.damping = Rand::randFloat(kMinDumping, kMaxDumping);
      p.lifeSpan = Rand::randFloat(0.0f, 100.0f);
    }

    // Create particle buffers on GPU and copy data into the first buffer.
    // Mark as static since we only write from the CPU once.
    mParticleBuffer[mSourceIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW);
    mParticleBuffer[mDestinationIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_STATIC_DRAW);

    for (int i = 0; i < kNumParticleBuffers; ++i)
    {
      // Describe the particle layout for OpenGL.
      mAttributes[i] = gl::Vao::create();
      gl::ScopedVao vao(mAttributes[i]);

      // Define attributes as offsets into the bound particle buffer
      gl::ScopedBuffer buffer(mParticleBuffer[i]);
      gl::enableVertexAttribArray(0);
      gl::enableVertexAttribArray(1);
      gl::enableVertexAttribArray(2);
      gl::enableVertexAttribArray(3);
      gl::enableVertexAttribArray(4);
      gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pos));
      gl::vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, dir));
      gl::vertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, cloudCentre));
      gl::vertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, lifeSpan));
      gl::vertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, damping));
    }

    mRenderProg = gl::GlslProg::create(gl::GlslProg::Format()
      .vertex(ci::app::loadAsset("draw_gl.vert"))
      .fragment(ci::app::loadAsset("draw_gl.frag"))
    );

    mUpdateProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(ci::app::loadAsset("particleUpdate_gl.frag"))
      .feedbackFormat(GL_INTERLEAVED_ATTRIBS)
      .feedbackVaryings({ "position", "direction", "cloudCentre", "lifeSpan", "damping" })
      .attribLocation("iPosition", 0)
      .attribLocation("iDirection", 1)
      .attribLocation("iCloudCentre", 2)
      .attribLocation("iLifeSpan", 3)
      .attribLocation("iDamping", 4));
  }

  void updateParticles(const ci::vec2& cloudCentre, float radius, float damping)
  {
    using namespace ci;

    gl::ScopedGlslProg prog(mUpdateProg);
    gl::ScopedState rasterizer(GL_RASTERIZER_DISCARD, true);	// turn off fragment stage

    mUpdateProg->uniform("uCloudCentre", cloudCentre);
    mUpdateProg->uniform("uRadius", radius);
    mUpdateProg->uniform("uDamping", damping);


    // Bind the source data (Attributes refer to specific buffers).
    gl::ScopedVao source(mAttributes[mSourceIndex]);
    // Bind destination as buffer base.
    gl::bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mParticleBuffer[mDestinationIndex]);
    gl::beginTransformFeedback(GL_POINTS);

    // Draw source into destination, performing our vertex transformations.
    gl::drawArrays(GL_POINTS, 0, kMaxParticles);

    gl::endTransformFeedback();

    // Swap source and destination for next loop
    std::swap(mSourceIndex, mDestinationIndex);
  }

  void draw()
  {
    using namespace ci;

    gl::ScopedGlslProg render(mRenderProg);
    gl::ScopedVao vao(mAttributes[mSourceIndex]);
    gl::context()->setDefaultShaderVars();
    gl::drawArrays(GL_POINTS, 0, kMaxParticles);
  }

private:

  ci::vec2 mCloudCentre;

  ci::gl::GlslProgRef mUpdateProg;
  ci::gl::GlslProgRef mRenderProg;

  // Descriptions of particle data layout.
  ci::gl::VaoRef		mAttributes[kNumParticleBuffers];
  // Buffers holding raw particle data on GPU.
  ci::gl::VboRef		mParticleBuffer[kNumParticleBuffers];

  // Current source and destination buffers for transform feedback.
  // Source and destination are swapped each frame after update.
  size_t mSourceIndex = 0;
  size_t mDestinationIndex = 1;
};

#endif

