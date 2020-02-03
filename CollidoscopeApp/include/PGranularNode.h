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

#include "cinder/Cinder.h"
#include "cinder/audio/Node.h"
#include "cinder/audio/dsp/RingBuffer.h"
#include "boost/optional.hpp"
#include "Messages.h"
#include "RingBufferPack.h"

#include <memory>

#include "PGranular.h"
#include "Config.h"

typedef std::shared_ptr<class PGranularNode> PGranularNodeRef;

/*
A node in the Cinder audio graph that holds PGranulars for loop and keyboard playing  
*/
class PGranularNode : public ci::audio::Node
{
public:
    static const int kNoMidiNote = -50;

    explicit PGranularNode( ci::audio::Buffer *grainBuffer, RingBuf<CursorTriggerMsg>& triggerRingBuf, RingBuf<NoteMsg>& mNoteRingBuf );

    /** Set selection size in samples */
    void setSelectionSize( size_t size )
    {
        mSelectionSize.store( size );
    }

    /** Set selection start in samples */
    void setSelectionStart( size_t start )
    {
        mSelectionStart.store( start );
    }

    void setGrainsDurationCoeff( float coeff )
    {
        mGrainDurationCoeff.store( coeff );
    }

protected:
    void initialize() override;

    void process( ci::audio::Buffer *buffer ) override;

private:
    
    void cursorCallback(collidoscope::PGranular<float>::TriggerType triggerType, int ID, size_t grainDuration);

    // creates or re-start a PGranular and sets the pitch according to the MIDI note passed as argument
    void handleNoteMsg( const NoteMsg &msg );

    // pointers to PGranular objects 
    using PGranularType = collidoscope::PGranular<float>;
    std::unique_ptr<PGranularType> mPGranularLoop;
    std::array<std::unique_ptr<PGranularType>, Config::MAX_VOICES> mPGranularNotes;

    // maps midi notes to pgranulars. When a noteOff is received makes sure the right PGranular is turned off
    std::array<int, Config::MAX_VOICES> mMidiNotes;

    // buffer containing the recorded audio, to pass to PGranular in initialize()
    ci::audio::Buffer *mGrainBuffer;

    ci::audio::BufferRef mTempBuffer;

    RingBuf<CursorTriggerMsg>& mTriggerRingBuf;
    RingBuf<NoteMsg>& mNoteRingBuf;

    std::atomic<size_t> mSelectionSize { 0 };
    
    std::atomic<size_t> mSelectionStart { 0 };
    
    std::atomic<float> mGrainDurationCoeff { 1.0f };

};

