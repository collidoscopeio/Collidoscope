/*

 Copyright (C) 2002 James McCartney.
 Copyright (C) 2016  Queen Mary University of London
 Author: Fiore Martin, based on Supercollider's (http://supercollider.github.io) TGrains code and Ross Bencina's "Implementing Real-Time Granular Synthesis"

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

#include <array>
#include <random>

#include "Envelopes.h"


namespace collidoscope {

  using std::size_t;

  /**
   * The very core of the Collidoscope audio engine: the granular synthesizer.
   * Based on SuperCollider's TGrains and Ross Bencina's "Implementing Real-Time Granular Synthesis"
   *
   * It implements Collidoscope's selection-based approach to granular synthesis.
   * A grain is basically a selection of a recorded sample of audio.
   * Grains are played in a loop: they are re-triggered each time they reach the end of the selection.
   * However, if the duration coefficient is greater than one, a new grain is re-triggered before the previous one is done,
   * the grains start to overlap with each other and create the typical eerie sound of grnular synthesis.
   * Also every time a new grain is triggered, it is offset of a few samples from the initial position to make the timbre more interesting.
   *
   *
   * PGranular uses a linear ASR envelope with 10 milliseconds attack and 50 milliseconds release.
   *
   * Note that PGranular is header based and only depends on std library and on "EnvASR.h" (also header based).
   * This means you can embedd it in two your project just by copying these two files over.
   *
   *
   */
  template <typename T>
  class PGranular
  {

  public:

    enum class TriggerType
    {
      NewGrainCreated,
      BecameIdle // when the synth becomes idle(no sound)
    };

    struct RandomOffsetGenerator
    {

      RandomOffsetGenerator(size_t max) :
        engine(rd()),
        distr{ 0, max }
      {}

      size_t operator()() 
      {
        return distr(engine);
      }

      std::random_device rd{}; // obtain a random number from hardware
      std::mt19937 engine;
      std::uniform_int_distribution<std::size_t> distr;
    };

    static constexpr size_t kMaxGrains = 32;
    static constexpr size_t kMinGrainsDuration = 640;
    static constexpr T kDefaultAttack = T(0.01);
    static constexpr T kDefaultRelease = T(0.05);

    static inline T interpolateLin(double xn, double xn_1, double decimal)
    {
      /* weighted sum interpolation */
      return static_cast<T> ((1 - decimal) * xn + decimal * xn_1);
    }

    /**
     * A single grain of the granular synthesis
     */
    struct PGrain
    {
      double phase;    // read pointer to mBuffer of this grain 
      double rate;     // rate of the grain. e.g. rate = 2 the grain will play twice as fast
      bool alive;      // whether this grain is alive. Not alive means it has been processed and can be replaced by another grain
      size_t age;      // age of this grain in samples 
      size_t duration; // duration of this grain in samples. minimum = 4

      double b1;       // hann envelope from Ross Bencina's "Implementing real time Granular Synthesis"
      double y1;
      double y2;
    };



    /**
     * Constructor.
     *
     * \param buffer a pointer to an array of T that contains the original sample that will be granulized
     * \param bufferLen length of buffer in samples
     * \rand function of type size_t ()(void) that is called back each time a new grain is generated. The returned value is used
     * to offset the starting sample of the grain. This adds more colour to the sound especially with small selections.
     * \triggerCallback function of type void ()(TriggerType, int) that is called back each time a new grain is generated.
     * \ID id of this PGrain. Passed to the triggerCallback function as second parameter to identify this PGranular as the caller.
     */
    PGranular(const T* buffer, size_t bufferLen, size_t sampleRate, std::function<void(TriggerType, int, size_t)> triggerCallback, int ID):
      mBuffer(buffer),
      mBufferLen(bufferLen),
      mRandOffset(sampleRate / 100),
      mTriggerCallback(triggerCallback),
      mEnvASR(T(0.01), T(1.0), T(0.05), sampleRate),
      mTrimRamp(sampleRate, T(1.0), T(0.25118864315096)),
      mID(ID)
    {
      /* init the grains */
      for (size_t grainIdx = 0; grainIdx < kMaxGrains; grainIdx++) 
      {
        mGrains[grainIdx].phase = 0;
        mGrains[grainIdx].rate = 1;
        mGrains[grainIdx].alive = false;
        mGrains[grainIdx].age = 0;
        mGrains[grainIdx].duration = 1;
      }
    }

    PGranular(const PGranular& copy) = default;

    ~PGranular() = default;

    /** Sets multiplier of duration of grains in seconds */
    void setGrainsDurationCoeff(float coeff)
    {
      mGrainsDurationCoeff = coeff;

      mGrainsDuration = std::lround(mTriggerRate * coeff);

      if (mGrainsDuration < kMinGrainsDuration)
        mGrainsDuration = kMinGrainsDuration;
    }

    /** Sets rate of grains. e.g rate = 2 means one octave higer */
    void setGrainsRate(double rate)
    {
      mGrainsRate = rate;
    }

    /** sets the selection start in samples */
    void setSelectionStart(size_t start)
    {
      mGrainsStart = start;
    }

    /** Sets the selection size ( and therefore the trigger rate) in samples */
    void setSelectionSize(size_t size)
    {

      if (size < kMinGrainsDuration)
        size = kMinGrainsDuration;

      mTriggerRate = size;

      mGrainsDuration = std::lround(size * mGrainsDurationCoeff);

    }

    void useBellEnvelope(bool useBell)
    {
        mUseBellEnvForGrain = useBell;
    }

    /** Sets the trim of the grains with respect to the level of the recorded sample
    *  trim is in amp value and defaule value is 0.25118864315096 (-12dB) */
    void setTrim(T trim)
    {
      if (trim != mTrimRamp.getTarget())
        mTrimRamp.setTarget(trim);
    }

    /* Sets attack of the overal envelope */
    void setEnvAttackTime(T attackTime)
    {
      mEnvASR.setAttackTime(attackTime);
    }

    /* Sets release of the overal envelope */
    void setEnvReleaseTime(T releaseTime)
    {
      mEnvASR.setReleaseTime(releaseTime);
    }

    /** Starts the synthesis engine */
    void noteOn(double rate)
    {
      if (mEnvASR.getState() != EnvASR<T>::State::eSustain) 
      {
        // note on sets triggering to the min value 
        if (mTriggerRate < kMinGrainsDuration) 
        {
          // mTriggerRate = kMinGrainsDuration;
        }

        setGrainsRate(rate);
        mEnvASR.setState(EnvASR<T>::State::eAttack);
      }
    }

    /** Stops the synthesis engine */
    void noteOff()
    {
      if (mEnvASR.getState() != EnvASR<T>::State::eIdle) {
        mEnvASR.setState(EnvASR<T>::State::eRelease);
      }
    }

    /* Stops the engine immediately, without releasing the envelope */
    void shutdown()
    {
      if (mEnvASR.getState() != EnvASR<T>::State::eIdle) 
      {
        mEnvASR.setState(EnvASR<T>::State::eShutDown);
      }
    }
    
    /** Whether the synthesis engine is active or not. After noteOff is called the synth stays active until the envelope decays to 0 */
    bool isIdle() const
    {
      return mEnvASR.getState() == EnvASR<T>::State::eIdle;
    }

    bool isShuttingDown() const
    {
      return mEnvASR.getState() == EnvASR<T>::State::eShutDown;
    }

    /**
     * Runs the granular engine and stores the output in \a audioOut
     *
     * \param pointer to an array of T. This will be filled with the output of PGranular. It needs to be at least \a numSamples long
     * \param tempBuffer a temporary buffer used to store the envelope value. It needs to be at least \a numSamples long
     * \param numSamples number of samples to be processed
     */
    void process(T* audioOut, T* tempBuffer, size_t numSamples)
    {

      if (isIdle())
        return;

      // num samples worth of sound ( due to envelope possibly finishing )
      size_t envSamples = 0;
      bool becameIdle = false;

      // process the envelope first and store it in the tempBuffer 
      for (size_t i = 0; i < numSamples; i++) 
      {
        tempBuffer[i] = mEnvASR.tick() * mTrimRamp.tick();
        envSamples++;

        if (isIdle()) 
        {
          // means that the envelope has stopped 
          becameIdle = true;
          break;
        }
      }

      // does the actual grains processing 
      processGrains(audioOut, tempBuffer, envSamples);

      // becomes idle if the envelope goes to idle state 
      if (becameIdle) 
      {
        mTriggerCallback(TriggerType::BecameIdle, mID, 0);
        reset();
      }
    }

  private:

    void processGrains(T* audioOut, T* envelopeValues, size_t numSamples)
    {

      /* process all existing alive grains */
      for (size_t grainIdx = 0; grainIdx < mNumAliveGrains; ) 
      {
        synthesizeGrain(mGrains[grainIdx], audioOut, envelopeValues, numSamples);

        if (!mGrains[grainIdx].alive) {
          // this grain is dead so copy the last of the active grains here 
          // so as to keep all active grains at the beginning of the array 
          // don't increment grainIdx so the last active grain is processed next cycle
          // if this grain is the last active grain then mNumAliveGrains is decremented 
          // and grainIdx = mNumAliveGrains so the loop stops 
          copyGrain(mNumAliveGrains - 1, grainIdx);
          mNumAliveGrains--;
        }
        else 
        {
          // go to next grain 
          grainIdx++;
        }
      }

      if (mTriggerRate == 0) 
      {
        return;
      }

      size_t randOffset = mRandOffset();
      bool newGrainWasTriggered = false;

      // trigger new grain and synthesize them as well 
      while (mTrigger < numSamples) 
      {

        // if there is room to accommodate new grains 
        if (mNumAliveGrains < kMaxGrains) 
        {
          // get next grain will be placed at the end of the alive ones 
          size_t grainIdx = mNumAliveGrains;
          mNumAliveGrains++;

          // initialize and synthesise the grain 
          PGrain &grain = mGrains[grainIdx];

          double phase = mGrainsStart + double(randOffset);
          if (phase >= mBufferLen)
            phase -= mBufferLen;

          grain.phase = phase;
          grain.rate = mGrainsRate;
          grain.alive = true;
          grain.age = 0;
          grain.duration = mGrainsDuration;

          const double w = 3.14159265358979323846 / mGrainsDuration;
          grain.b1 = 2.0 * std::cos(w);
          grain.y1 = std::sin(w);
          grain.y2 = 0.0;

          synthesizeGrain(grain, audioOut + mTrigger, envelopeValues + mTrigger, numSamples - mTrigger);

          if (grain.alive == false) 
          {
            mNumAliveGrains--;
          }

          newGrainWasTriggered = true;
        }

        // update trigger even if no new grain was started 
        mTrigger += mTriggerRate;
      }

      // prepare trigger for next cycle: init mTrigger with the reminder of the samples from this cycle 
      mTrigger -= numSamples;

      if (newGrainWasTriggered) 
      {
        mTriggerCallback(TriggerType::NewGrainCreated, mID, mGrainsDuration);
      }
    }

    // synthesize a single grain 
    // audioOut = pointer to audio block to fill 
    // numSamples = number of samples to process for this block
    void synthesizeGrain(PGrain &grain, T* audioOut, T* envelopeValues, size_t numSamples)
    {

      // copy all grain data into local variable for faster processing
      const auto rate = grain.rate;
      auto phase = grain.phase;
      auto age = grain.age;
      auto duration = grain.duration;


      auto b1 = grain.b1;
      auto y1 = grain.y1;
      auto y2 = grain.y2;

      // only process minimum between samples of this block and time left to leave for this grain 
      auto numSamplesToOut = std::min(numSamples, duration - age);

      for (size_t sampleIdx = 0; sampleIdx < numSamplesToOut; sampleIdx++) 
      {

        const size_t readIndex = (size_t)phase;
        const size_t nextReadIndex = (readIndex == mBufferLen - 1) ? 0 : readIndex + 1; // wrap on the read buffer if needed 

        const double decimal = phase - readIndex;

        T out = interpolateLin(mBuffer[readIndex], mBuffer[nextReadIndex], decimal);

        // calculate raised cosine bell envelope 
        auto y0 = b1 * y1 - y2;
        y2 = y1;
        y1 = y0;

        // apply envelope
        if(mUseBellEnvForGrain)
            out *= T(y0);

        audioOut[sampleIdx] += out * envelopeValues[sampleIdx];

        // increment age one sample 
        age++;
        // increment the phase according to the rate of this grain 
        phase += rate;

        if (phase >= mBufferLen) 
        {   // wrap the phase if needed 
          phase -= mBufferLen;
        }
      }

      if (age == duration) 
      {
        // if it processed all the samples left to leave ( numSamplesToOut = duration-age)
        // then the grain is finished 
        grain.alive = false;
      }
      else 
      {
        grain.phase = phase;
        grain.age = age;
        grain.y1 = y1;
        grain.y2 = y2;
      }
    }

    void copyGrain(size_t from, size_t to)
    {
      mGrains[to] = mGrains[from];
    }

    void reset()
    {
      mTrigger = 0;
      for (size_t i = 0; i < mNumAliveGrains; i++) 
      {
        mGrains[i].alive = false;
      }

      mNumAliveGrains = 0;
    }

    const int mID;

    // pointer to (mono) buffer, where the underlying sample is recorder 
    const T* mBuffer;
    // length of mBuffer in samples 
    const size_t mBufferLen;

    // offset in the buffer where the grains start. a.k.a. selection start 
    size_t mGrainsStart{ 0 };

    // grain duration in samples 
    float mGrainsDurationCoeff{ 1.0f };
    // duration of grains is selection size * duration coeff
    size_t mGrainsDuration{ kMinGrainsDuration };
    // rate of grain, affects pitch 
    double mGrainsRate{ 1.0f };
    // trim for the grain amplitude 
    T mTrim{ T(0.25118864315096) };

    size_t mTrigger{ 0 };  // next onset
    size_t mTriggerRate{ 0 };   // inter onset. Start silent

    // the array of grains 
    std::array<PGrain, kMaxGrains> mGrains;
    // number of alive grains 
    size_t mNumAliveGrains{ 0 };

    RandomOffsetGenerator mRandOffset;
    std::function<void(TriggerType, int, size_t)> mTriggerCallback;

    bool mUseBellEnvForGrain{ true };
    EnvASR<T> mEnvASR;
    EnvLin<T> mTrimRamp;
  };




} // namespace collidoscope


