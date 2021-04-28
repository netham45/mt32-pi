//
// synthlcd.cpp
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
#include <circle/util.h>

#include <cstdio>

#include "lcd/ui.h"
#include "utility.h"

constexpr u8 BarSpacing = 2;
constexpr u8 SpinnerChars[] = {'_', '_', '_', '-', '\'', '\'', '^', '^', '`', '`', '-', '_', '_', '_'};

CUserInterface::CUserInterface()
	: m_State(TState::None),
	  m_nStateTime(0),
	  m_nCurrentSpinnerChar(0),
	  m_CurrentImage(TImage::None),
	  m_SystemMessageTextBuffer{'\0'}
{
}

void CUserInterface::Update(CLCD& LCD, unsigned int nTicks)
{
	const unsigned int nDeltaTicks = (nTicks - m_nStateTime);

	// System message timeout
	if (m_State == TState::DisplayingMessage && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		m_State = TState::None;
		m_nStateTime = nTicks;
	}

	// Spinner update
	else if (m_State == TState::DisplayingSpinnerMessage && nDeltaTicks >= Utility::MillisToTicks(SystemMessageSpinnerTimeMillis))
	{
		m_nCurrentSpinnerChar = (m_nCurrentSpinnerChar + 1) % sizeof(SpinnerChars);
		m_SystemMessageTextBuffer[SystemMessageTextBufferSize - 2] = SpinnerChars[m_nCurrentSpinnerChar];
		m_nStateTime = nTicks;
	}

	// Image display update
	else if (m_State == TState::DisplayingImage && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		m_State = TState::None;
		m_nStateTime = nTicks;
	}

	// Power saving
	else if (m_State == TState::EnteringPowerSavingMode && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		LCD.SetBacklightEnabled(false);
		m_State = TState::InPowerSavingMode;
		m_nStateTime = nTicks;
	}

	if (m_State != TState::None)
		DrawSystemState(LCD);
}

void CUserInterface::ShowSystemMessage(const char* pMessage, bool bSpinner)
{
	const unsigned nTicks = CTimer::GetClockTicks();

	if (bSpinner)
	{
		constexpr int nMaxMessageLen = SystemMessageTextBufferSize - 3;
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "%-*.*s %c", nMaxMessageLen, nMaxMessageLen, pMessage, SpinnerChars[0]);
		m_State = TState::DisplayingSpinnerMessage;
		m_nCurrentSpinnerChar = 0;
	}
	else
	{
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), pMessage);
		m_State = TState::DisplayingMessage;
	}

	m_nStateTime = nTicks;
}

void CUserInterface::DisplayImage(TImage Image)
{
	const unsigned nTicks = CTimer::GetClockTicks();
	m_CurrentImage = Image;
	m_State = TState::DisplayingImage;
	m_nStateTime = nTicks;
}

void CUserInterface::DrawChannelLevels(CLCD& LCD, CMIDIMonitor& MIDIMonitor, u8 nBarHeight, unsigned int nTicks, u8 nChannels)
{
	float ChannelLevels[16];
	float PeakLevels[16];
	MIDIMonitor.GetChannelLevels(nTicks, ChannelLevels, PeakLevels);

	u8 nWidth, nHeight;
	LCD.GetDimensions(nWidth, nHeight);

	if (LCD.GetType() == CLCD::TType::Graphical)
	{
		const u8 nBarWidth = (nWidth - (nChannels * BarSpacing)) / nChannels;
		DrawChannelLevelsGraphical(LCD, 2, 0, nBarWidth, nBarHeight, ChannelLevels, PeakLevels, nChannels, true);
	}
	else
	{
		// TODO: Character
	}
}

void CUserInterface::DrawChannelLevelsGraphical(CLCD& LCD, u8 nBarXOffset, u8 nBarYOffset, u8 nBarWidth, u8 nBarHeight, const float* pChannelLevels, const float* pPeakLevels, u8 nChannels, bool bDrawBarBases)
{
	assert(pChannelLevels != nullptr);
	const u8 nBarMaxY = nBarHeight - 1;

	for (u8 nChannel = 0; nChannel < nChannels; ++nChannel)
	{
		const u8 nLevelPixels = pChannelLevels[nChannel] * nBarMaxY;
		const u8 nX1 = nBarXOffset + nChannel * (nBarWidth + BarSpacing);
		const u8 nX2 = nX1 + nBarWidth - 1;

		if (nLevelPixels > 0 || bDrawBarBases)
		{
			const u8 nY1 = nBarYOffset + (nBarMaxY - nLevelPixels);
			const u8 nY2 = nY1 + nLevelPixels;
			LCD.DrawFilledRect(nX1, nY1, nX2, nY2);
		}

		if (pPeakLevels)
		{
			const u8 nPeakLevelPixels = pPeakLevels[nChannel] * nBarMaxY;
			if (nPeakLevelPixels)
			{
				// TODO: DrawLine
				const u8 nY = nBarYOffset + (nBarMaxY - nPeakLevelPixels);
				LCD.DrawFilledRect(nX1, nY, nX2, nY);
			}
		}
	}
}

void CUserInterface::DrawSystemState(CLCD& LCD) const
{
	u8 nWidth, nHeight;
	LCD.GetDimensions(nWidth, nHeight);

	if (LCD.GetType() == CLCD::TType::Graphical)
	{
		if (m_State == TState::DisplayingImage)
			LCD.DrawImage(m_CurrentImage);
		else
		{
			const u8 nMessageRow = nHeight == 32 ? 0 : 1;
			LCD.Print(m_SystemMessageTextBuffer, 0, nMessageRow, true);
		}
	}
	else
	{
		if (m_State != TState::DisplayingImage)
		{
			if (nHeight == 2)
			{
				LCD.Print(m_SystemMessageTextBuffer, 0, 0, true);
				LCD.Print("", 0, 1, true);
			}
			else if (nHeight == 4)
			{
				// Clear top line
				LCD.Print("", 0, 0, true);
				LCD.Print(m_SystemMessageTextBuffer, 0, 1, true);
				LCD.Print("", 0, 2, true);
				LCD.Print("", 0, 3, true);
			}
		}
	}
}

/**

CSynthLCD::CSynthLCD()
	: m_SystemState(TSystemState::None),
	  m_nSystemStateTime(0),
	  m_nCurrentSpinnerChar(0),
	  m_CurrentImage(TImage::None),
	  m_SystemMessageTextBuffer{'\0'},

	//   m_MT32State(TMT32State::DisplayingPartStates),
	//   m_nMT32StateTime(0),
	//  m_MT32TextBuffer{'\0'},
	//  m_nPreviousMasterVolume(0),

	  m_bSC55DisplayingText(false),
	  m_bSC55DisplayingDots(false),
	  m_nSC55DisplayTextTime(0),
	  m_nSC55DisplayDotsTime(0),
	  m_SC55TextBuffer{'\0'},
	  m_SC55PixelBuffer{0},

	  m_ChannelVelocities{0},
	  m_ChannelLevels{0},
	  m_ChannelPeakLevels{0},
	  m_ChannelPeakTimes{0}
{
}

void CSynthLCD::OnSystemMessage(const char* pMessage, bool bSpinner)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();

	if (bSpinner)
	{
		constexpr int nMaxMessageLen = SystemMessageTextBufferSize - 3;
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "%-*.*s %c", nMaxMessageLen, nMaxMessageLen, pMessage, SpinnerChars[0]);
		m_SystemState = TSystemState::DisplayingSpinnerMessage;
		m_nCurrentSpinnerChar = 0;
	}
	else
	{
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), pMessage);
		m_SystemState = TSystemState::DisplayingMessage;
	}

	m_nSystemStateTime = nTicks;
}

void CSynthLCD::ClearSpinnerMessage()
{
	m_SystemState = TSystemState::None;
	m_nCurrentSpinnerChar = 0;
}

void CSynthLCD::OnDisplayImage(TImage Image)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();
	m_CurrentImage = Image;
	m_SystemState = TSystemState::DisplayingImage;
	m_nSystemStateTime = nTicks;
}

void CSynthLCD::EnterPowerSavingMode()
{
	snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "Power saving mode");
	m_SystemState = TSystemState::EnteringPowerSavingMode;
	m_nSystemStateTime = CTimer::Get()->GetTicks();
}

void CSynthLCD::ExitPowerSavingMode()
{
	SetBacklightEnabled(true);
	m_SystemState = TSystemState::None;
}

void CSynthLCD::OnSC55DisplayText(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(m_SC55TextBuffer, sizeof(m_SC55TextBuffer), pMessage);

	m_bSC55DisplayingText = true;
	m_nSC55DisplayTextTime = ticks;
}

void CSynthLCD::OnSC55DisplayDots(const u8* pData)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	memcpy(m_SC55PixelBuffer, pData, sizeof(m_SC55PixelBuffer));

	m_bSC55DisplayingDots = true;
	m_nSC55DisplayDotsTime = ticks;
}

void CSynthLCD::Update(CSoundFontSynth& Synth)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();
	UpdateSystem(nTicks);

	// Displaying text timeout
	if (m_bSC55DisplayingText && (nTicks - m_nSC55DisplayTextTime) > MSEC2HZ(SC55DisplayTimeMillis))
		m_bSC55DisplayingText = false;

	// Displaying text timeout
	if (m_bSC55DisplayingDots && (nTicks - m_nSC55DisplayDotsTime) > MSEC2HZ(SC55DisplayTimeMillis))
		m_bSC55DisplayingDots = false;
}

void CSynthLCD::UpdateSystem(unsigned int nTicks)
{
	// System message timeout
	if (m_SystemState == TSystemState::DisplayingMessage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = nTicks;
	}

	// Spinner update
	else if (m_SystemState == TSystemState::DisplayingSpinnerMessage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageSpinnerTimeMillis))
	{
		m_nCurrentSpinnerChar = (m_nCurrentSpinnerChar + 1) % sizeof(SpinnerChars);
		m_SystemMessageTextBuffer[SystemMessageTextBufferSize - 2] = SpinnerChars[m_nCurrentSpinnerChar];
		m_nSystemStateTime = nTicks;
	}

	// Image display update
	else if (m_SystemState == TSystemState::DisplayingImage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = nTicks;
	}

	// Power saving
	else if (m_SystemState == TSystemState::EnteringPowerSavingMode && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		SetBacklightEnabled(false);
		m_SystemState = TSystemState::InPowerSavingMode;
		m_nSystemStateTime = nTicks;
	}
}

**/
