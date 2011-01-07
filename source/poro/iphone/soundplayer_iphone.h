/***************************************************************************
 *
 * Copyright (c) 2010 Petri Purho, Dennis Belfrage
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ***************************************************************************/

#ifndef SOUNDPLAYERIPHONE_H
#define SOUNDPLAYERIPHONE_H

#include "../poro_types.h"
#include "isoundplayer.h"
#include <CoreAudio/CoreAudioTypes.h>


namespace poro {

	class SoundPlayerIPhone : public ISoundPlayer
	{
	public:
		virtual bool Init();
		
		virtual ISound* LoadSound( const types::string& filename );
		
		virtual void Play( ISound* sound, float volume );
		virtual void Play( ISound* sound, float volume, bool loop );
		virtual void Stop( ISound* sound );
	private:
		UInt32 GetSoundSequenceId();		
		UInt32 mSoundSequenceId;
	};

}

#endif