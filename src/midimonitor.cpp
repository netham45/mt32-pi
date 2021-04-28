//
// midimonitor.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2021 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#include <circle/timer.h>

#include "midimonitor.h"

CMIDIMonitor::CMIDIMonitor()
	: m_PeakLevels{0.0f},
	  m_PeakTimes{0}
{
	AllNotesOff();
	ResetControllers(false);
}

void CMIDIMonitor::OnShortMessage(u32 nMessage)
{
	const u8 nStatus  = nMessage & 0xF0;
	const u8 nChannel = nMessage & 0x0F;
	const u8 nData1   = (nMessage >> 8) & 0xFF;
	const u8 nData2   = (nMessage >> 16) & 0xFF;

	TChannelState& State = m_State[nChannel];
	unsigned int nTicks = CTimer::GetClockTicks();

	switch (nStatus)
	{
		// Note off
		case 0x80:
			State.Notes[nData1].nNoteOffTime = nTicks;
			break;

		// Note on
		case 0x90:
			if (nData2)
			{
				State.Notes[nData1].nNoteOnTime = nTicks;
				State.Notes[nData1].nNoteOffTime = 0;
				State.Notes[nData1].nVelocity = nData2;
			}
			else
				State.Notes[nData1].nNoteOffTime = nTicks;
			break;

		// Control change
		case 0xB0:
			ProcessCC(nChannel, nData1, nData2);
			break;

		// System Reset
		case 0xFF:
			AllNotesOff();
			ResetControllers(false);

		default:
			break;
	}
}

constexpr float EaseFunction(float nInput)
{
	return -((nInput -1) * (nInput - 1)) + 1;
}

void CMIDIMonitor::GetChannelLevels(unsigned int nTicks, float* pOutLevels, float* pOutPeaks)
{
	for (size_t nChannel = 0; nChannel < ChannelCount; ++nChannel)
	{
		// TODO: handle remappable drum channels
		const bool bIsPercussionChannel = nChannel == 9;
		float nChannelVolume = 0.0f;

		for (size_t nNote = 0; nNote < NoteCount; ++nNote)
		{
			const TNoteState& NoteState = m_State[nChannel].Notes[nNote];
			const float nEnvelope = bIsPercussionChannel ? ComputePercussionEnvelope(nTicks, NoteState) : ComputeEnvelope(nTicks, NoteState);
			const float nNoteVolume = nEnvelope * (NoteState.nVelocity / 127.0f) * (m_State[nChannel].nVolume / 127.0f) * (m_State[nChannel].nExpression / 127.0f);
			nChannelVolume = Utility::Max(nChannelVolume, nNoteVolume);
		}

		nChannelVolume = Utility::Clamp(nChannelVolume, 0.0f, 1.0f);

		float nPeakLevel = m_PeakLevels[nChannel];
		const float nPeakUpdatedMillis = Utility::TicksToMillis(nTicks - m_PeakTimes[nChannel]);

		if (nPeakUpdatedMillis >= PeakHoldTimeMillis)
		{
			const float nPeakFallMillis = Utility::Max(nPeakUpdatedMillis - PeakHoldTimeMillis, 0.0f);
			nPeakLevel -= nPeakFallMillis / PeakFalloffTimeMillis;
			nPeakLevel = Utility::Clamp(nPeakLevel, 0.0f, 1.0f);
		}

		if (nChannelVolume >= nPeakLevel)
		{
			nPeakLevel = nChannelVolume;
			m_PeakLevels[nChannel] = nChannelVolume;
			m_PeakTimes[nChannel] = nTicks;
		}

		pOutLevels[nChannel] = nChannelVolume;
		pOutPeaks[nChannel] = nPeakLevel;
	}
}

void CMIDIMonitor::AllNotesOff()
{
	for (auto& Channel : m_State)
	{
		for (auto& Note : Channel.Notes)
		{
			Note.nNoteOnTime = 0;
			Note.nNoteOffTime = 0;
			Note.nVelocity = 0;
		}
	}
}

void CMIDIMonitor::ResetControllers(bool bIsResetAllControllers)
{
	for (auto& Channel : m_State)
	{
		Channel.nExpression = 127;

		// The MIDI specification says that certain controllers should not be reset
		// in response to a Reset All Controllers message
		if (!bIsResetAllControllers)
		{
			Channel.nVolume = 100;
			Channel.nPan = 64;
		}
	}
}

void CMIDIMonitor::ProcessCC(u8 nChannel, u8 nCC, u8 nValue)
{
	TChannelState& State = m_State[nChannel];

	switch (nCC)
	{
		// Channel volume
		case 0x07:
			State.nVolume = nValue;
			break;

		// Pan
		case 0x0A:
			State.nPan = nValue;
			break;

		// Expression
		case 0x0B:
			State.nExpression = nValue;
			break;

		// According to the MIDI spec, the following Channel Mode messages
		// all function as All Notes Off messages
		case 0x78:	// All Sound Off
		case 0x7B:	// All Notes Off
		case 0x7C:	// Omni Off
		case 0x7D:	// Omni On
		case 0x7E:	// Mono On
		case 0x7F:	// Mono Off
			AllNotesOff();
			break;

		// Reset All Controllers
		case 0x79:
			ResetControllers(true);
			break;

		default:
			break;
	}
}

float CMIDIMonitor::ComputeEnvelope(unsigned int nTicks, const TNoteState& NoteState) const
{
	if (NoteState.nNoteOnTime == 0)
		return 0.0f;

	nTicks = Utility::Max(nTicks, NoteState.nNoteOnTime);

	if (NoteState.nNoteOffTime == 0)
	{
		// Attack phase
		return 1.0f;
	}
	else
	{
		// Decay/release phase
		const float nNoteOffDurationMillis = Utility::Min(static_cast<float>(Utility::TicksToMillis(nTicks - NoteState.nNoteOffTime)), DecayReleaseTimeMillis);
		return EaseFunction(Utility::Max(1.0f - nNoteOffDurationMillis / DecayReleaseTimeMillis, 0.0f));
	}

	// const float nNoteOnDurationMillis = (nTicks - NoteState.nNoteOnTime) / 1000.0f;

	// if (nNoteOnDurationMillis < AttackTimeMillis)
	// {
	// 	if (NoteState.nNoteOffTime == 0)
	// 	{
	// 		// Attack phase
	// 		assert(nNoteOnDurationMillis / AttackTimeMillis <= 1.0f);
	// 		return nNoteOnDurationMillis / AttackTimeMillis;
	// 	}
	// 	else
	// 	{
	// 		// Decay/release phase
	// 		const float nVolume = nNoteOnDurationMillis / AttackTimeMillis;
	// 		const float nGateDurationMillis = (NoteState.nNoteOffTime - NoteState.nNoteOnTime) / 1000.0f;
	// 		return Utility::Max((1.0f - nGateDurationMillis / DecayReleaseTimeMillis) * nVolume, 0.0f);
	// 	}
	// }
	// else if (nNoteOnDurationMillis < AttackTimeMillis + DecayTimeMillis)
	// {
	// 	const float nDecayDurationMillis = nNoteOnDurationMillis - AttackTimeMillis;
	// 	const float nVolume = 1.0f - (nDecayDurationMillis / DecayTimeMillis) * (1.0f - SustainLevel);
	// 	if (NoteState.nNoteOffTime == 0)
	// 	{
	// 		// Decay phase
	// 		assert(nVolume >= SustainLevel);
	// 		return nVolume;
	// 	}
	// 	else
	// 	{
	// 		// Decay/release phase
	// 		const float nGateDurationMillis = (NoteState.nNoteOffTime - NoteState.nNoteOnTime) / 1000.0f;
	// 		return Utility::Max((1.0f - nGateDurationMillis / DecayReleaseTimeMillis) * nVolume, 0.0f);
	// 	}
	// }
	// else
	// {
	// 	if (NoteState.nNoteOffTime == 0)
	// 		return SustainLevel;
	// 	else
	// 	{
	// 		assert(nTicks >= NoteState.nNoteOffTime);
	// 		const float nNoteOffDurationMillis = (nTicks - NoteState.nNoteOffTime) / 1000.0f;
	// 		return Utility::Max((1.0f - nNoteOffDurationMillis / ReleaseTimeMillis) * SustainLevel, 0.0f);
	// 	}
	// }
}

float CMIDIMonitor::ComputePercussionEnvelope(unsigned int nTicks, const TNoteState& NoteState) const
{
	if (NoteState.nNoteOnTime == 0)
		return 0.0f;

	nTicks = Utility::Max(nTicks, NoteState.nNoteOnTime);

	// Decay/release phase
	const float nNoteOnDurationMillis = Utility::Min(static_cast<float>(Utility::TicksToMillis(nTicks - NoteState.nNoteOnTime)), DecayReleaseTimeMillis);
	return EaseFunction(Utility::Max(1.0f - nNoteOnDurationMillis / DecayReleaseTimeMillis, 0.0f));
}
