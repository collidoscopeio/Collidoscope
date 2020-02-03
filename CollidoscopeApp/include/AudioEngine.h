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

#pragma once

#include <array>

#include "cinder/audio/Context.h"
#include "cinder/audio/ChannelRouterNode.h"
#include "cinder/audio/MonitorNode.h"
#include "cinder/audio/FilterNode.h"
#include "BufferToWaveRecorderNode.h"
#include "PGranularNode.h"
#include "RingBufferPack.h"

#include "Messages.h"
#include "Config.h"


/**
 * Audio engine of the application. It uses the Cinder library to process audio in input and output. 
 * The audio engine manages both waves. All methods have a waveIndx parameter to address a specific wave.
 */ 
class AudioEngine
{
public:

    AudioEngine() = default;
    AudioEngine( const AudioEngine &copy ) = delete;
    AudioEngine & operator=(const AudioEngine &copy) = delete;

    void setup( const Config& Config );

    size_t getSampleRate();

    void record( size_t index );

    void loopOn( size_t waveIdx );

    void loopOff( size_t waveIdx );

    void noteOn( size_t waveIdx, int note );

    void noteOff( size_t waveIdx, int note );

    void setSelectionSize( size_t waveIdx, size_t size );

    void setSelectionStart( size_t waveIdx, size_t start );

    void setGrainDurationCoeff( size_t waveIdx, float coeff );

    void setFilterCutoff( size_t waveIdx, double cutoff );

    void getCursorTriggers( size_t waveIdx, std::vector<CursorTriggerMsg>& cursorTriggers );

    void getWaveRecordingMessages(size_t waveIdx, std::vector<RecordWaveMsg>& waveRecordingMessages);

    /**
     * Returns a const reference to the audio output buffer. 
     * This is the buffer that is sent off to the audio interface at each audio cycle. 
     * It is used in the graphic thread to draw the oscilloscope.
     */
    const ci::audio::Buffer& getAudioOutputBufferRef( size_t waveIdx ) const;


private:

    // nodes for mic input 
    std::array<ci::audio::ChannelRouterNodeRef, NUM_WAVES > mInputRouterNodes;
    // nodes for recording audio input into buffer. Also sends chunks information through ring buffer
    std::array<BufferToWaveRecorderNodeRef, NUM_WAVES > mBufferRecorderNodes;
    // pgranulars wrapped in a Cinder::Node 
    std::array<PGranularNodeRef, NUM_WAVES > mPGranularNodes;


    std::array<ci::audio::ChannelRouterNodeRef, NUM_WAVES > mOutputRouterNodes;
    // nodes to get the audio buffer scoped in the oscilloscope 
    std::array<ci::audio::MonitorNodeRef, NUM_WAVES > mOutputMonitorNodes;
    // nodes for lowpass filtering
    std::array<cinder::audio::FilterLowPassNodeRef, NUM_WAVES> mLowPassFilterNodes;

    std::array<RingBuf<CursorTriggerMsg>, NUM_WAVES > mCursorTriggerRingBufs;
    std::array<RingBuf<RecordWaveMsg>, NUM_WAVES > mWaveRecordingRingBufs;
    std::array<RingBuf<NoteMsg>, NUM_WAVES > mNoteRingBufs;
    

};
