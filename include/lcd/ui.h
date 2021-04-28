//
// ui.h
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

#ifndef _ui_h
#define _ui_h

#include <circle/types.h>

#include "lcd/lcd.h"
#include "lcd/barchars.h"

/**
constexpr u8 BarSpacing = 2;
constexpr u8 ChannelCount = 9;

void DrawChannelLevelsCharacter(CLCD& LCD, CMIDIMonitor& MIDIMonitor, u8 nFirstRow, u8 nRows, u8 nBarXOffset, u8 nBarSpacing, bool bDrawBarBases)
{
	u8 nLCDColumns, nLCDRows;
	LCD.GetDimensions(nLCDColumns, nLCDRows);

	float ChannelLevels[16];
	MIDIMonitor.GetChannelLevels(ChannelLevels);

	char LineBuf[nRows][nLCDColumns];
	const u8 nBarHeight = nRows * 8;

	// Initialize with ASCII spaces
	memset(LineBuf, ' ', sizeof(LineBuf));

	// For each channel
	for (u8 i = 0; i < nChannels; ++i)
	{
		const u8 nCharIndex = i + i * nBarSpacing + nBarXOffset;
		assert(nCharIndex < 20);

		u8 nLevelPixels = ChannelLevels[i] * nBarHeight;
		if (bDrawBarBases && nLevelPixels == 0)
			nLevelPixels = 1;

		const u8 nFullRows  = nLevelPixels / 8;
		const u8 nRemainder = nLevelPixels % 8;

		for (u8 j = 0; j < nFullRows; ++j)
			LineBuf[nRows - j - 1][nCharIndex] = BarChars[8];

		for (u8 j = nFullRows; j < nRows; ++j)
			LineBuf[nRows - j - 1][nCharIndex] = BarChars[0];

		if (nRemainder)
			LineBuf[nRows - nFullRows - 1][nCharIndex] = BarChars[nRemainder];
	}

	for (u8 nRow = 0; nRow < nRows; ++nRow)
		LCD.Print(LineBuf[nRow], 0, nRow, false);

	// for (u8 nRow = 0; nRow < nRows; ++nRow)
	// {
	// 	WriteCommand(0x80 | m_RowOffsets[nFirstRow + nRow]);
	// 	for (u8 nColumn = 0; nColumn < m_nColumns; ++nColumn)
	// 		WriteData(LineBuf[nRow][nColumn]);
	// }
}

**/

#include <circle/types.h>

#include "midimonitor.h"

class CUserInterface
{
public:
	enum class TState
	{
		None,
		DisplayingMessage,
		DisplayingSpinnerMessage,
		DisplayingImage,
		EnteringPowerSavingMode,
		InPowerSavingMode
	};

	CUserInterface();

	void Update(CLCD& LCD, unsigned int nTicks);

	void ShowSystemMessage(const char* pMessage, bool bSpinner = false);
	// void ClearSpinnerMessage();
	void DisplayImage(TImage Image);
	// void EnterPowerSavingMode();
	// void ExitPowerSavingMode();

	// void OnSC55DisplayText(const char* pMessage);
	// void OnSC55DisplayDots(const u8* pData);

	TState GetState() const { return m_State; }

	static void DrawChannelLevels(CLCD& LCD, CMIDIMonitor& MIDIMonitor, u8 nBarHeight, unsigned int nTicks, u8 nChannels);

private:
	void DrawSystemState(CLCD& LCD) const;

	static void DrawChannelLevelsGraphical(CLCD& LCD, u8 nBarXOffset, u8 nBarYOffset, u8 nBarWidth, u8 nBarHeight, const float* pChannelLevels, const float* pPeakLevels, u8 nChannels, bool bDrawBarBases);

	// CLCDInterface(CLCD& LCD)
	// 	: m_pLCD(&LCD)
	// {
	// }


//protected:
	//void UpdateSystem(unsigned int nTicks);

	// N characters plus null terminator
	static constexpr size_t SystemMessageTextBufferSize = 20 + 1;
	static constexpr size_t SC55TextBufferSize = 32 + 1;

	// 64 bytes; each byte representing 5 pixels (see p78 of SC-55 manual)
	static constexpr size_t SC55PixelBufferSize = 64;

	static constexpr unsigned SystemMessageDisplayTimeMillis = 3000;
	static constexpr unsigned SystemMessageSpinnerTimeMillis = 32;
	static constexpr unsigned SC55DisplayTimeMillis = 3000;

	static constexpr float BarFalloff  = 1.0f / 16.0f;
	static constexpr float PeakFalloff = 1.0f / 64.0f;

	// // System state
	TState m_State;
	unsigned m_nStateTime;
	size_t m_nCurrentSpinnerChar;
	TImage m_CurrentImage;
	char m_SystemMessageTextBuffer[SystemMessageTextBufferSize];

	// // SC-55 state
	// bool m_bSC55DisplayingText;
	// bool m_bSC55DisplayingDots;
	// unsigned m_nSC55DisplayTextTime;
	// unsigned m_nSC55DisplayDotsTime;
	// char m_SC55TextBuffer[SC55TextBufferSize];
	// u8 m_SC55PixelBuffer[SC55PixelBufferSize];

	//CLCD* m_pLCD;
};

#endif
