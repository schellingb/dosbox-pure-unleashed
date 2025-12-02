/*
 *  Copyright (C) 2020-2025 Bernhard Schelling
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Surface.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>
#include <ZL_Data.h>
#include <ZL_Input.h>
#include <ZL_Thread.h>
#include <ZL_Math3D.h>

#include <vector>

#include <libretro-common/include/libretro.h>
#include <include/cross.h>
#include "vfs_implementation.h"

#include <dosbox_pure_sta.h>
int DBPS_SaveSlotIndex;
std::string DBPS_BrowsePath;

enum EHotkeyFKeys
{
	HOTKEY_F_PAUSE       =  1,
	HOTKEY_F_SLOWMOTION  =  2,
	HOTKEY_F_FASTFORWARD =  3,
	HOTKEY_F_QUICKSAVE   =  5,
	HOTKEY_F_FULLSCREEN  =  7,
	HOTKEY_F_QUICKLOAD   =  9,
	HOTKEY_F_LOCKMOUSE   = 11,
	HOTKEY_F_TOGGLEOSD   = 12,
};
static unsigned short HotkeyMod;
static unsigned char ThrottleMode, LastAudioThrottleMode;
static bool ThrottlePaused, SpeedModHold, DisableSystemALT, UseMiddleMouseMenu, PointerLock, DrawStretched;
static bool DrawCoreShader, DoApplyInterfaceOptions, DoApplyGeometry, DoSave, DoLoad, AudioSkip, DefaultPointerLock, AudioMuted = false;
static char Scaling;
static int CRTFilter, AudioLatency;
static float FastRate = 5.0f, SlowRate = 0.3f;
static const float AudioVolumeStep = 0.1f;
static float AudioVolume = 1.0f;
static std::string PathSaves, PathSystem;
static ZL_Vector PointerLockPos;
enum { FAST_FPS_LIMIT = 200 };

static ZL_Surface srfCore, srfOSD;
static ZL_Color colOSDBG;
static ZL_Mutex mtxCoreOptions;
static ZL_Font fntOSD;
static ZL_Shader shdrCore, shdrOSD;

static const retro_key ZLKtoRETROKEY[] =
{
	RETROK_UNKNOWN, //ZLK_UNKNOWN = 0,
	RETROK_UNKNOWN, //1
	RETROK_UNKNOWN, //2
	RETROK_UNKNOWN, //3
	RETROK_a, //ZLK_A = 4,
	RETROK_b, //ZLK_B = 5,
	RETROK_c, //ZLK_C = 6,
	RETROK_d, //ZLK_D = 7,
	RETROK_e, //ZLK_E = 8,
	RETROK_f, //ZLK_F = 9,
	RETROK_g, //ZLK_G = 10,
	RETROK_h, //ZLK_H = 11,
	RETROK_i, //ZLK_I = 12,
	RETROK_j, //ZLK_J = 13,
	RETROK_k, //ZLK_K = 14,
	RETROK_l, //ZLK_L = 15,
	RETROK_m, //ZLK_M = 16,
	RETROK_n, //ZLK_N = 17,
	RETROK_o, //ZLK_O = 18,
	RETROK_p, //ZLK_P = 19,
	RETROK_q, //ZLK_Q = 20,
	RETROK_r, //ZLK_R = 21,
	RETROK_s, //ZLK_S = 22,
	RETROK_t, //ZLK_T = 23,
	RETROK_u, //ZLK_U = 24,
	RETROK_v, //ZLK_V = 25,
	RETROK_w, //ZLK_W = 26,
	RETROK_x, //ZLK_X = 27,
	RETROK_y, //ZLK_Y = 28,
	RETROK_z, //ZLK_Z = 29,
	RETROK_1, //ZLK_1 = 30,
	RETROK_2, //ZLK_2 = 31,
	RETROK_3, //ZLK_3 = 32,
	RETROK_4, //ZLK_4 = 33,
	RETROK_5, //ZLK_5 = 34,
	RETROK_6, //ZLK_6 = 35,
	RETROK_7, //ZLK_7 = 36,
	RETROK_8, //ZLK_8 = 37,
	RETROK_9, //ZLK_9 = 38,
	RETROK_0, //ZLK_0 = 39,
	RETROK_RETURN, //ZLK_RETURN = 40,
	RETROK_ESCAPE, //ZLK_ESCAPE = 41,
	RETROK_BACKSPACE, //ZLK_BACKSPACE = 42,
	RETROK_TAB, //ZLK_TAB = 43,
	RETROK_SPACE, //ZLK_SPACE = 44,
	RETROK_MINUS, //ZLK_MINUS = 45,
	RETROK_EQUALS, //ZLK_EQUALS = 46,
	RETROK_LEFTBRACKET, //ZLK_LEFTBRACKET = 47,
	RETROK_RIGHTBRACKET, //ZLK_RIGHTBRACKET = 48,
	RETROK_BACKSLASH, //ZLK_BACKSLASH = 49,
	RETROK_BACKSLASH, //ZLK_SHARP = 50,
	RETROK_SEMICOLON, //ZLK_SEMICOLON = 51,
	RETROK_QUOTE, //ZLK_APOSTROPHE = 52,
	RETROK_BACKQUOTE, //ZLK_GRAVE = 53,
	RETROK_COMMA, //ZLK_COMMA = 54,
	RETROK_PERIOD, //ZLK_PERIOD = 55,
	RETROK_SLASH, //ZLK_SLASH = 56,
	RETROK_CAPSLOCK, //ZLK_CAPSLOCK = 57,
	RETROK_F1, //ZLK_F1 = 58,
	RETROK_F2, //ZLK_F2 = 59,
	RETROK_F3, //ZLK_F3 = 60,
	RETROK_F4, //ZLK_F4 = 61,
	RETROK_F5, //ZLK_F5 = 62,
	RETROK_F6, //ZLK_F6 = 63,
	RETROK_F7, //ZLK_F7 = 64,
	RETROK_F8, //ZLK_F8 = 65,
	RETROK_F9, //ZLK_F9 = 66,
	RETROK_F10, //ZLK_F10 = 67,
	RETROK_F11, //ZLK_F11 = 68,
	RETROK_F12, //ZLK_F12 = 69,
	RETROK_PRINT, //ZLK_PRINTSCREEN = 70,
	RETROK_SCROLLOCK, //ZLK_SCROLLLOCK = 71,
	RETROK_PAUSE, //ZLK_PAUSE = 72,
	RETROK_INSERT, //ZLK_INSERT = 73,
	RETROK_HOME, //ZLK_HOME = 74,
	RETROK_PAGEUP, //ZLK_PAGEUP = 75,
	RETROK_DELETE, //ZLK_DELETE = 76,
	RETROK_END, //ZLK_END = 77,
	RETROK_PAGEDOWN, //ZLK_PAGEDOWN = 78,
	RETROK_RIGHT, //ZLK_RIGHT = 79,
	RETROK_LEFT, //ZLK_LEFT = 80,
	RETROK_DOWN, //ZLK_DOWN = 81,
	RETROK_UP, //ZLK_UP = 82,
	RETROK_NUMLOCK, //ZLK_NUMLOCKCLEAR = 83,
	RETROK_KP_DIVIDE, //ZLK_KP_DIVIDE = 84,
	RETROK_KP_MULTIPLY, //ZLK_KP_MULTIPLY = 85,
	RETROK_KP_MINUS, //ZLK_KP_MINUS = 86,
	RETROK_KP_PLUS, //ZLK_KP_PLUS = 87,
	RETROK_KP_ENTER, //ZLK_KP_ENTER = 88,
	RETROK_KP1, //ZLK_KP_1 = 89,
	RETROK_KP2, //ZLK_KP_2 = 90,
	RETROK_KP3, //ZLK_KP_3 = 91,
	RETROK_KP4, //ZLK_KP_4 = 92,
	RETROK_KP5, //ZLK_KP_5 = 93,
	RETROK_KP6, //ZLK_KP_6 = 94,
	RETROK_KP7, //ZLK_KP_7 = 95,
	RETROK_KP8, //ZLK_KP_8 = 96,
	RETROK_KP9, //ZLK_KP_9 = 97,
	RETROK_KP0, //ZLK_KP_0 = 98,
	RETROK_KP_PERIOD, //ZLK_KP_PERIOD = 99,
	RETROK_OEM_102, //ZLK_NONUSBACKSLASH = 100,
	RETROK_MENU, //ZLK_APPLICATION = 101,
	RETROK_POWER, //ZLK_POWER = 102,
	RETROK_KP_EQUALS, //ZLK_KP_EQUALS = 103,
	RETROK_F13, //ZLK_F13 = 104,
	RETROK_F14, //ZLK_F14 = 105,
	RETROK_F15, //ZLK_F15 = 106,
	RETROK_UNKNOWN, //ZLK_F16 = 107,
	RETROK_UNKNOWN, //ZLK_F17 = 108,
	RETROK_UNKNOWN, //ZLK_F18 = 109,
	RETROK_UNKNOWN, //ZLK_F19 = 110,
	RETROK_UNKNOWN, //ZLK_F20 = 111,
	RETROK_UNKNOWN, //ZLK_F21 = 112,
	RETROK_UNKNOWN, //ZLK_F22 = 113,
	RETROK_UNKNOWN, //ZLK_F23 = 114,
	RETROK_UNKNOWN, //ZLK_F24 = 115,
	RETROK_UNKNOWN, //ZLK_EXECUTE = 116,
	RETROK_HELP, //ZLK_HELP = 117,
	RETROK_MENU, //ZLK_MENU = 118,
	RETROK_UNKNOWN, //ZLK_SELECT = 119,
	RETROK_UNKNOWN, //ZLK_STOP = 120,
	RETROK_UNKNOWN, //ZLK_AGAIN = 121,   // redo
	RETROK_UNKNOWN, //ZLK_UNDO = 122,
	RETROK_UNKNOWN, //ZLK_CUT = 123,
	RETROK_UNKNOWN, //ZLK_COPY = 124,
	RETROK_UNKNOWN, //ZLK_PASTE = 125,
	RETROK_UNKNOWN, //ZLK_FIND = 126,
	RETROK_UNKNOWN, //ZLK_MUTE = 127,
	RETROK_UNKNOWN, //ZLK_VOLUMEUP = 128,
	RETROK_UNKNOWN, //ZLK_VOLUMEDOWN = 129,
	RETROK_UNKNOWN, //130
	RETROK_UNKNOWN, //131
	RETROK_UNKNOWN, //132
	RETROK_COMMA, //ZLK_KP_COMMA = 133,
	RETROK_KP_EQUALS, //ZLK_KP_EQUALSAS400 = 134,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL1 = 135,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL2 = 136,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL3 = 137, // Yen
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL4 = 138,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL5 = 139,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL6 = 140,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL7 = 141,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL8 = 142,
	RETROK_UNKNOWN, //ZLK_INTERNATIONAL9 = 143,
	RETROK_UNKNOWN, //ZLK_LANG1 = 144, // IME/Alphabet toggle
	RETROK_UNKNOWN, //ZLK_LANG2 = 145, // IME conversion
	RETROK_UNKNOWN, //ZLK_LANG3 = 146, // Katakana
	RETROK_UNKNOWN, //ZLK_LANG4 = 147, // Hiragana
	RETROK_UNKNOWN, //ZLK_LANG5 = 148, // Zenkaku/Hankaku
	RETROK_UNKNOWN, //ZLK_LANG6 = 149, // reserved
	RETROK_UNKNOWN, //ZLK_LANG7 = 150, // reserved
	RETROK_UNKNOWN, //ZLK_LANG8 = 151, // reserved
	RETROK_UNKNOWN, //ZLK_LANG9 = 152, // reserved
	RETROK_UNKNOWN, //ZLK_ALTERASE = 153, // Erase-Eaze
	RETROK_SYSREQ, //ZLK_SYSREQ = 154,
	RETROK_UNKNOWN, //ZLK_CANCEL = 155,
	RETROK_UNKNOWN, //ZLK_CLEAR = 156,
	RETROK_UNKNOWN, //ZLK_PRIOR = 157,
	RETROK_UNKNOWN, //ZLK_RETURN2 = 158,
	RETROK_UNKNOWN, //ZLK_SEPARATOR = 159,
	RETROK_UNKNOWN, //ZLK_OUT = 160,
	RETROK_UNKNOWN, //ZLK_OPER = 161,
	RETROK_UNKNOWN, //ZLK_CLEARAGAIN = 162,
	RETROK_UNKNOWN, //ZLK_CRSEL = 163,
	RETROK_UNKNOWN, //ZLK_EXSEL = 164,
	RETROK_UNKNOWN, //165
	RETROK_UNKNOWN, //166
	RETROK_UNKNOWN, //167
	RETROK_UNKNOWN, //168
	RETROK_UNKNOWN, //169
	RETROK_UNKNOWN, //170
	RETROK_UNKNOWN, //171
	RETROK_UNKNOWN, //172
	RETROK_UNKNOWN, //173
	RETROK_UNKNOWN, //174
	RETROK_UNKNOWN, //175
	RETROK_UNKNOWN, //ZLK_KP_00 = 176,
	RETROK_UNKNOWN, //ZLK_KP_000 = 177,
	RETROK_UNKNOWN, //ZLK_THOUSANDSSEPARATOR = 178,
	RETROK_UNKNOWN, //ZLK_DECIMALSEPARATOR = 179,
	RETROK_UNKNOWN, //ZLK_CURRENCYUNIT = 180,
	RETROK_UNKNOWN, //ZLK_CURRENCYSUBUNIT = 181,
	RETROK_UNKNOWN, //ZLK_KP_LEFTPAREN = 182,
	RETROK_UNKNOWN, //ZLK_KP_RIGHTPAREN = 183,
	RETROK_LEFTBRACE, //ZLK_KP_LEFTBRACE = 184,
	RETROK_RIGHTBRACE, //ZLK_KP_RIGHTBRACE = 185,
	RETROK_TAB, //ZLK_KP_TAB = 186,
	RETROK_BACKSPACE, //ZLK_KP_BACKSPACE = 187,
	RETROK_UNKNOWN, //ZLK_KP_A = 188,
	RETROK_UNKNOWN, //ZLK_KP_B = 189,
	RETROK_UNKNOWN, //ZLK_KP_C = 190,
	RETROK_UNKNOWN, //ZLK_KP_D = 191,
	RETROK_UNKNOWN, //ZLK_KP_E = 192,
	RETROK_UNKNOWN, //ZLK_KP_F = 193,
	RETROK_UNKNOWN, //ZLK_KP_XOR = 194,
	RETROK_UNKNOWN, //ZLK_KP_POWER = 195,
	RETROK_UNKNOWN, //ZLK_KP_PERCENT = 196,
	RETROK_UNKNOWN, //ZLK_KP_LESS = 197,
	RETROK_UNKNOWN, //ZLK_KP_GREATER = 198,
	RETROK_UNKNOWN, //ZLK_KP_AMPERSAND = 199,
	RETROK_UNKNOWN, //ZLK_KP_DBLAMPERSAND = 200,
	RETROK_UNKNOWN, //ZLK_KP_VERTICALBAR = 201,
	RETROK_UNKNOWN, //ZLK_KP_DBLVERTICALBAR = 202,
	RETROK_UNKNOWN, //ZLK_KP_COLON = 203,
	RETROK_UNKNOWN, //ZLK_KP_HASH = 204,
	RETROK_SPACE, //ZLK_KP_SPACE = 205,
	RETROK_UNKNOWN, //ZLK_KP_AT = 206,
	RETROK_EXCLAIM, //ZLK_KP_EXCLAM = 207,
	RETROK_UNKNOWN, //ZLK_KP_MEMSTORE = 208,
	RETROK_UNKNOWN, //ZLK_KP_MEMRECALL = 209,
	RETROK_CLEAR, //ZLK_KP_MEMCLEAR = 210,
	RETROK_UNKNOWN, //ZLK_KP_MEMADD = 211,
	RETROK_UNKNOWN, //ZLK_KP_MEMSUBTRACT = 212,
	RETROK_UNKNOWN, //ZLK_KP_MEMMULTIPLY = 213,
	RETROK_UNKNOWN, //ZLK_KP_MEMDIVIDE = 214,
	RETROK_UNKNOWN, //ZLK_KP_PLUSMINUS = 215,
	RETROK_CLEAR, //ZLK_KP_CLEAR = 216,
	RETROK_CLEAR, //ZLK_KP_CLEARENTRY = 217,
	RETROK_UNKNOWN, //ZLK_KP_BINARY = 218,
	RETROK_UNKNOWN, //ZLK_KP_OCTAL = 219,
	RETROK_UNKNOWN, //ZLK_KP_DECIMAL = 220,
	RETROK_UNKNOWN, //ZLK_KP_HEXADECIMAL = 221,
	RETROK_UNKNOWN, //222
	RETROK_UNKNOWN, //223
	RETROK_LCTRL, //ZLK_LCTRL = 224,
	RETROK_LSHIFT, //ZLK_LSHIFT = 225,
	RETROK_LALT, //ZLK_LALT = 226, // alt, option
	RETROK_LMETA, //ZLK_LGUI = 227, // windows, command (apple), meta
	RETROK_RCTRL, //ZLK_RCTRL = 228,
	RETROK_RSHIFT, //ZLK_RSHIFT = 229,
	RETROK_RALT, //ZLK_RALT = 230, // alt gr, option
	RETROK_RMETA, //ZLK_RGUI = 231, // windows, command (apple), meta
	RETROK_UNKNOWN, //232
	RETROK_UNKNOWN, //233
	RETROK_UNKNOWN, //234
	RETROK_UNKNOWN, //235
	RETROK_UNKNOWN, //236
	RETROK_UNKNOWN, //237
	RETROK_UNKNOWN, //238
	RETROK_UNKNOWN, //239
	RETROK_UNKNOWN, //240
	RETROK_UNKNOWN, //241
	RETROK_UNKNOWN, //242
	RETROK_UNKNOWN, //243
	RETROK_UNKNOWN, //244
	RETROK_UNKNOWN, //245
	RETROK_UNKNOWN, //246
	RETROK_UNKNOWN, //247
	RETROK_UNKNOWN, //248
	RETROK_UNKNOWN, //249
	RETROK_UNKNOWN, //250
	RETROK_UNKNOWN, //251
	RETROK_UNKNOWN, //252
	RETROK_UNKNOWN, //253
	RETROK_UNKNOWN, //254
	RETROK_UNKNOWN, //255
	RETROK_UNKNOWN, //256
	RETROK_MODE, //ZLK_MODE = 257,
	RETROK_UNKNOWN, //ZLK_AUDIONEXT = 258,
	RETROK_UNKNOWN, //ZLK_AUDIOPREV = 259,
	RETROK_UNKNOWN, //ZLK_AUDIOSTOP = 260,
	RETROK_UNKNOWN, //ZLK_AUDIOPLAY = 261,
	RETROK_UNKNOWN, //ZLK_AUDIOMUTE = 262,
	RETROK_UNKNOWN, //ZLK_MEDIASELECT = 263,
	RETROK_COMPOSE, //ZLK_WWW = 264,
	RETROK_COMPOSE, //ZLK_MAIL = 265,
	RETROK_UNKNOWN, //ZLK_CALCULATOR = 266,
	RETROK_UNKNOWN, //ZLK_COMPUTER = 267,
	RETROK_UNKNOWN, //ZLK_AC_SEARCH = 268,
	RETROK_UNKNOWN, //ZLK_AC_HOME = 269,
	RETROK_UNKNOWN, //ZLK_AC_BACK = 270,
	RETROK_UNKNOWN, //ZLK_AC_FORWARD = 271,
	RETROK_UNKNOWN, //ZLK_AC_STOP = 272,
	RETROK_UNKNOWN, //ZLK_AC_REFRESH = 273,
	RETROK_UNKNOWN, //ZLK_AC_BOOKMARKS = 274,
	RETROK_UNKNOWN, //ZLK_BRIGHTNESSDOWN = 275,
	RETROK_UNKNOWN, //ZLK_BRIGHTNESSUP = 276,
	RETROK_UNKNOWN, //ZLK_DISPLAYSWITCH = 277,
	RETROK_UNKNOWN, //ZLK_KBDILLUMTOGGLE = 278,
	RETROK_UNKNOWN, //ZLK_KBDILLUMDOWN = 279,
	RETROK_UNKNOWN, //ZLK_KBDILLUMUP = 280,
	RETROK_UNKNOWN, //ZLK_EJECT = 281,
	RETROK_UNKNOWN, //ZLK_SLEEP = 282,
	RETROK_UNKNOWN, //ZLK_LAST
};
static bool RETROKDown[RETROK_LAST];
static retro_system_av_info av { { 1024, 1024, 1024, 1024, 1 }, { 70.0f, 44100.0f } };
static retro_keyboard_event_t retro_keyboard_event_cb;
static scalar ui_last_audio_stretch = 1.0f;
static ZL_Rectf core_rec, osd_rec;
static ZL_TextBuffer txtOSD;
static ticks_t txtOSDTick, dirtySettingsTick;
struct SNotify { ZL_TextBuffer txt; unsigned duration; retro_log_level level; ticks_t ticks; float y; };
static std::vector<SNotify> vecNotify;
static std::vector<ZL_JoystickData*> vecJoys;

extern "C" { unsigned int SDL_GetTicks(void); }
extern "C" { int SDL_ShowCursor(int toggle); }
extern "C" { struct SDL_Window* SDL_GetMouseFocus(void); }
extern "C" { void* SDL_GL_GetProcAddress(const char *proc); } 
extern "C" { unsigned long SDL_GetThreadID(struct SDL_Thread* = NULL); }
#if defined(ZILLALOG)
static unsigned long MainThreadID = SDL_GetThreadID();
#endif

struct SJoyBind
{
	ZL_JoystickData* Joy;
	enum EFrom : unsigned char { FROM_NONE, FROM_AXIS, FROM_HAT, FROM_BALL, FROM_BUTTON } From;
	unsigned char Num;
	signed char Dir;
	short GetVal(bool forAnalog)
	{
		if (forAnalog)
			switch (From)
			{
				case FROM_AXIS:   return (short)ZL_Math::Clamp(Joy->axes[Num] * Dir, 0, 0x7fff);
				case FROM_HAT:    return ((Joy->hats[Num] & Dir) ? (short)0x7fff : (short)0);
				case FROM_BALL:   return (short)ZL_Math::Clamp((ZL_Math::Abs(Dir) == 1 ? Joy->balls[Num].dx : Joy->balls[Num].dy) * ZL_Math::Sign(Dir) * 10, 0, 0x7fff);
				case FROM_BUTTON: return (Joy->buttons[Num] ? (short)0x7fff : (short)0);
				default:break;
			}
		else
			switch (From)
			{
				case FROM_AXIS:   return (short)((Joy->axes[Num] * Dir) >= 12000);
				case FROM_HAT:    return (short)((Joy->hats[Num] & Dir) ? 1 : 0);
				case FROM_BALL:   return (short)(((ZL_Math::Abs(Dir) == 1 ? Joy->balls[Num].dx : Joy->balls[Num].dy) * ZL_Math::Sign(Dir) * 10) > 10);
				case FROM_BUTTON: return (short)(Joy->buttons[Num] ? 1 : 0);
				default:break;
			}
		return 0;
	}
	void Describe(std::string& outJoyName, std::string& outBind, const char* prefix, const char* bindname = NULL) const
	{
		outJoyName.assign(prefix).append(Joy ? Joy->name : "<Disconnected Controller>");
		if (bindname) { ZL_String::format((ZL_String&)outBind, "%s%s", prefix, bindname); return; }
		switch (From)
		{
			case FROM_AXIS:   ZL_String::format((ZL_String&)outBind, "%sAxis %d %stive",    prefix, (Num + 1), (Dir == 1 ? "Posi" : "Nega")); break;
			case FROM_HAT:    ZL_String::format((ZL_String&)outBind, "%sHat %d %s",         prefix, (Num + 1), (Dir == ZL_HAT_UP ? "Up" : Dir == ZL_HAT_RIGHT ? "Right" : Dir == ZL_HAT_DOWN ? "Down" : "Left")); break;
			case FROM_BALL:   ZL_String::format((ZL_String&)outBind, "%sBall %d %c %stive", prefix, (Num + 1), (ZL_Math::Abs(Dir) == 1 ? 'X' : 'Y'), (Dir > 0 ? "Posi" : "Nega")); break;
			case FROM_BUTTON: ZL_String::format((ZL_String&)outBind, "%sButton %d",         prefix, (Num + 1), (Dir == 1 ? "Posi" : "Nega")); break;
			default:break;
		}
	}
	ZL_String ToConfig() const
	{
		std::string joyName, bind;
		Describe(joyName, bind, "");
		return (((ZL_String&)joyName) += '|').append(((ZL_String&)bind).str_replace(" ", "|"));
	}
	static SJoyBind FromConfig(const ZL_String& cfg)
	{
		SJoyBind res = { NULL }; // zero bytes including padding
		std::vector<ZL_String> parts = cfg.split("|");
		int n = (parts.size() >= 3 ? (int)parts[2] : 0);
		if (n)
		{
			switch (parts[1].length() > 2 ? (parts[1].c_str()[3]|0x20) : '\0')
			{
				case 's': res.From = FROM_AXIS;   res.Dir = ((parts.size() >= 4 && (parts[3].c_str()[0]|0x20) == 'n') ? -1 : 1); break;
				case ' ': res.From = FROM_HAT;    { char c = (parts.size() >= 4 ? (parts[3].c_str()[0]|0x20) : '\0'); res.Dir = (c == 'u' ? ZL_HAT_UP : c == 'r' ? ZL_HAT_RIGHT : c == 'd' ? ZL_HAT_DOWN : ZL_HAT_LEFT); } break;
				case 'l': res.From = FROM_BALL;   res.Dir = ((parts.size() >= 4 && (parts[3].c_str()[0]|0x20) == 'x') ? 1 : 2) * ((parts.size() >= 5 && (parts[4].c_str()[0]|0x20) == 'n') ? -1 : 1); break;
				case 't': res.From = FROM_BUTTON; break;
				default: res.From = FROM_NONE; res.Dir = 0; n = 1; break;
			}
			res.Num = (unsigned char)(n - 1);
			for (ZL_JoystickData* j : vecJoys)
			{
				if (parts[0] != j->name) continue;
				res.Joy = j;
				if (res.Num >= (res.From == FROM_AXIS ? j->naxes : res.From == FROM_HAT ? j->nhats : res.From == FROM_BALL ? j->nballs : res.From == FROM_BUTTON ? j->nbuttons : 0))
					res = { NULL };
				break;
			}
		}
		return res;
	}
};

enum { BIND_PORT0 = 0, _BIND_PORTS = 4 };
enum EBindId { BIND_ID_UP, BIND_ID_DOWN, BIND_ID_LEFT, BIND_ID_RIGHT, BIND_ID_B, BIND_ID_A, BIND_ID_Y, BIND_ID_X, BIND_ID_SELECT, BIND_ID_START, BIND_ID_L, BIND_ID_R, BIND_ID_L2, BIND_ID_R2, BIND_ID_L3, BIND_ID_R3, BIND_ID_LSTICK_UP, BIND_ID_LSTICK_DOWN, BIND_ID_LSTICK_LEFT, BIND_ID_LSTICK_RIGHT, BIND_ID_RSTICK_UP, BIND_ID_RSTICK_DOWN, BIND_ID_RSTICK_LEFT, BIND_ID_RSTICK_RIGHT, _BIND_ID_COUNT, _BIND_ID_FIRST_AXIS = BIND_ID_LSTICK_UP };
static const char* BindName[] = { "up", "down", "left", "right", "b", "a", "y", "x", "select", "start", "l", "r", "l2", "r2", "l3", "r3", "lstickup", "lstickdown", "lstickleft", "lstickright", "rstickup", "rstickdown", "rstickleft", "rstickright" };
static SJoyBind JoyBinds[_BIND_PORTS][_BIND_ID_COUNT], *CaptureJoyBind;
static std::vector<unsigned char> CaptureJoyState;
static const char** JoyBindTemplateNames;

static SJoyBind JoyBindTemplateXInput[_BIND_ID_COUNT] = {
	{ NULL, SJoyBind::FROM_BUTTON,    0 }, { NULL, SJoyBind::FROM_BUTTON,    1 }, { NULL, SJoyBind::FROM_BUTTON,    2 }, { NULL, SJoyBind::FROM_BUTTON,    3 }, // DPAD
	{ NULL, SJoyBind::FROM_BUTTON,   10 }, { NULL, SJoyBind::FROM_BUTTON,   11 }, { NULL, SJoyBind::FROM_BUTTON,   12 }, { NULL, SJoyBind::FROM_BUTTON,   13 }, // BAYX
	{ NULL, SJoyBind::FROM_BUTTON,    5 }, { NULL, SJoyBind::FROM_BUTTON,    4 }, { NULL, SJoyBind::FROM_BUTTON,    8 }, { NULL, SJoyBind::FROM_BUTTON,    9 }, // SEL,START,L,R
	{ NULL, SJoyBind::FROM_AXIS,  4,  1 }, { NULL, SJoyBind::FROM_AXIS,  5,  1 }, { NULL, SJoyBind::FROM_BUTTON,    6 }, { NULL, SJoyBind::FROM_BUTTON,    7 }, // L2,R2,L3,R3
	{ NULL, SJoyBind::FROM_AXIS,  1, -1 }, { NULL, SJoyBind::FROM_AXIS,  1,  1 }, { NULL, SJoyBind::FROM_AXIS,  0, -1 }, { NULL, SJoyBind::FROM_AXIS,  0,  1 }, // LSTICK
	{ NULL, SJoyBind::FROM_AXIS,  3, -1 }, { NULL, SJoyBind::FROM_AXIS,  3,  1 }, { NULL, SJoyBind::FROM_AXIS,  2, -1 }, { NULL, SJoyBind::FROM_AXIS,  2,  1 }, // RSTICK
};
static const char* JoyBindTemplateXInputNames[] = {
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "A Button (Down)", "B Button (Right)", "X Button (Left)", "Y Button (Up)", // DPAD,BAYX
	"Select / View", "Start / Menu", "Left Bumper", "Right Bumper", "Left Trigger", "Right Trigger", "Left Stick Press", "Right Stick Press", // SEL,START,L,R,L2,R2;L3,R3
	"Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right", "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right", // LSTICK,RSTICK
};

static SJoyBind JoyBindTemplatePS4[_BIND_ID_COUNT] = {
	{ NULL, SJoyBind::FROM_HAT, 0, ZL_HAT_UP   }, { NULL, SJoyBind::FROM_HAT, 0, ZL_HAT_DOWN }, { NULL, SJoyBind::FROM_HAT, 0, ZL_HAT_LEFT }, { NULL, SJoyBind::FROM_HAT, 0, ZL_HAT_RIGHT }, // DPAD
	{ NULL, SJoyBind::FROM_BUTTON,           1 }, { NULL, SJoyBind::FROM_BUTTON,           2 }, { NULL, SJoyBind::FROM_BUTTON,           0 }, { NULL, SJoyBind::FROM_BUTTON,            3 }, // BAYX
	{ NULL, SJoyBind::FROM_BUTTON,           8 }, { NULL, SJoyBind::FROM_BUTTON,           9 }, { NULL, SJoyBind::FROM_BUTTON,           4 }, { NULL, SJoyBind::FROM_BUTTON,            5 }, // SEL,START,L,R
	{ NULL, SJoyBind::FROM_BUTTON,           6 }, { NULL, SJoyBind::FROM_BUTTON,           7 }, { NULL, SJoyBind::FROM_BUTTON,          10 }, { NULL, SJoyBind::FROM_BUTTON,           11 }, // L2,R2,L3,R3
	{ NULL, SJoyBind::FROM_AXIS,         2, -1 }, { NULL, SJoyBind::FROM_AXIS,         2,  1 }, { NULL, SJoyBind::FROM_AXIS,         3, -1 }, { NULL, SJoyBind::FROM_AXIS,          3,  1 }, // LSTICK
	{ NULL, SJoyBind::FROM_AXIS,         0, -1 }, { NULL, SJoyBind::FROM_AXIS,         0,  1 }, { NULL, SJoyBind::FROM_AXIS,         1, -1 }, { NULL, SJoyBind::FROM_AXIS,          1,  1 }, // RSTICK
};
static const char* JoyBindTemplatePS4Names[] = {
	"D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Cross Button (Down)", "Circle Button (Right)", "Square Button (Left)", "Triangle Button (Up)", // DPAD,BAYX
	"Select / Create", "Start / Options", "L1", "R1", "L2", "R2", "Left Stick Press", "Right Stick Press", // SEL,START,L,R,L2,R2;L3,R3
	"Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right", "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right", // LSTICK,RSTICK
};

static inline void DirtySettings() { dirtySettingsTick = ZLTICKS + 10000; }
static inline void SynchronizeSettings(bool force) { if (dirtySettingsTick && (force || ZLPASSED(dirtySettingsTick))) { ZL_Application::SettingsSynchronize(); dirtySettingsTick = 0; } }

static EBindId GetBindIdFromRetro(unsigned device, unsigned index, unsigned id, bool axispos = false)
{
	switch (device)
	{
		case RETRO_DEVICE_JOYPAD:
			switch (id)
			{
				case RETRO_DEVICE_ID_JOYPAD_UP:     return BIND_ID_UP;
				case RETRO_DEVICE_ID_JOYPAD_DOWN:   return BIND_ID_DOWN;
				case RETRO_DEVICE_ID_JOYPAD_LEFT:   return BIND_ID_LEFT;
				case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return BIND_ID_RIGHT;
				case RETRO_DEVICE_ID_JOYPAD_B:      return BIND_ID_B;
				case RETRO_DEVICE_ID_JOYPAD_A:      return BIND_ID_A;
				case RETRO_DEVICE_ID_JOYPAD_Y:      return BIND_ID_Y;
				case RETRO_DEVICE_ID_JOYPAD_X:      return BIND_ID_X;
				case RETRO_DEVICE_ID_JOYPAD_SELECT: return BIND_ID_SELECT;
				case RETRO_DEVICE_ID_JOYPAD_START:  return BIND_ID_START;
				case RETRO_DEVICE_ID_JOYPAD_L:      return BIND_ID_L;
				case RETRO_DEVICE_ID_JOYPAD_R:      return BIND_ID_R;
				case RETRO_DEVICE_ID_JOYPAD_L2:     return BIND_ID_L2;
				case RETRO_DEVICE_ID_JOYPAD_R2:     return BIND_ID_R2;
				case RETRO_DEVICE_ID_JOYPAD_L3:     return BIND_ID_L3;
				case RETRO_DEVICE_ID_JOYPAD_R3:     return BIND_ID_R3;
			}
			break;
		case RETRO_DEVICE_ANALOG:
			switch (index)
			{
				case RETRO_DEVICE_INDEX_ANALOG_LEFT:  return (id == RETRO_DEVICE_ID_ANALOG_X ? (axispos ? BIND_ID_LSTICK_RIGHT : BIND_ID_LSTICK_LEFT) : (axispos ? BIND_ID_LSTICK_DOWN : BIND_ID_LSTICK_UP));
				case RETRO_DEVICE_INDEX_ANALOG_RIGHT: return (id == RETRO_DEVICE_ID_ANALOG_X ? (axispos ? BIND_ID_RSTICK_RIGHT : BIND_ID_RSTICK_LEFT) : (axispos ? BIND_ID_RSTICK_DOWN : BIND_ID_RSTICK_UP));
				case RETRO_DEVICE_INDEX_ANALOG_BUTTON: return _BIND_ID_COUNT;
			}
			break;
	}
	ZL_ASSERT(false);
	return _BIND_ID_COUNT;
}

static void RefreshJoysticks()
{
	int oldcount = (int)vecJoys.size(), matches = 0;
	for (int devidx = 0; devidx < ZL_Joystick::NumJoysticks(); devidx++)
	{
		ZL_JoystickData* joy = ZL_Joystick::JoystickOpen(devidx);
		if (!joy) continue; // was just disconnected?
		int knownidx = -1;
		for (ZL_JoystickData*& it : vecJoys) { if (it == joy) { knownidx = (int)(&it - &vecJoys[0]); break; } }
		if (knownidx != -1) ZL_Joystick::JoystickClose(joy); // was already open, reduce refcount
		else { knownidx = (int)vecJoys.size(); vecJoys.push_back(joy); } // new joystick
		if (knownidx != matches) { joy = vecJoys[knownidx]; vecJoys[knownidx] = vecJoys[matches]; vecJoys[matches] = joy; } // swap into match pos
		matches++;
	}
	if (oldcount == (int)vecJoys.size() && matches == oldcount) return; // nothing changed

	for (int i = (int)vecJoys.size(); i-- != matches;) // disconnect old
	{
		ZL_JoystickData* oldjoy = vecJoys[i];
		for (int idx = 0; idx != _BIND_PORTS * _BIND_ID_COUNT; idx++)
		{
			int bindPort = (idx / _BIND_ID_COUNT), bindId = (idx % _BIND_ID_COUNT);
			if (JoyBinds[bindPort][bindId].Joy == oldjoy) { JoyBinds[bindPort][bindId].Joy = NULL; JoyBinds[bindPort][bindId].From = SJoyBind::FROM_NONE; }
		}
		ZL_Joystick::JoystickClose(oldjoy);
		vecJoys.resize(i);
	}

	ZL_JoystickData *joyXInput = NULL, *joyPS4 = NULL;
	for (ZL_JoystickData* joy : vecJoys)
	{
		if (!memcmp(joy->name, "XInput Controller #", 19) && (!joyXInput || joy->name[19] < joyXInput->name[19]))
			joyXInput = joy;
		else if (!strcmp(joy->name, "Wireless Controller") && !joyPS4 && joy->naxes == 6 && joy->nhats == 1 && joy->nballs == 0 && joy->nbuttons == 14)
			joyPS4 = joy;
	}

	SJoyBind* bindtemplate = NULL;
	if      (joyXInput) { for (SJoyBind& it : JoyBindTemplateXInput) { it.Joy = joyXInput; } bindtemplate = JoyBindTemplateXInput; JoyBindTemplateNames = JoyBindTemplateXInputNames; }
	else if (joyPS4)    { for (SJoyBind& it : JoyBindTemplatePS4)    { it.Joy = joyPS4;    } bindtemplate = JoyBindTemplatePS4;    JoyBindTemplateNames = JoyBindTemplatePS4Names;    }

	bool useCustomControllerBinding = (((*ZL_Application::SettingsGet("custom_controller_bindings").c_str())|0x20) == 't'); // 't'rue
	if (useCustomControllerBinding)
	{
		char key[100], have_primary = 0, have_secondary = 0;
		for (int idx = 0; idx != _BIND_PORTS * _BIND_ID_COUNT; idx++)
		{
			int bindPort = (idx / _BIND_ID_COUNT), bindId = (idx % _BIND_ID_COUNT);
			sprintf(key, "bind_port_%d_%s", (bindPort + 1), BindName[bindId]);
			if (!ZL_Application::SettingsHas(key)) continue;
			(bindPort ? have_secondary : have_primary) = 1;
			JoyBinds[bindPort][bindId] = SJoyBind::FromConfig(ZL_Application::SettingsGet(key));
		}
		if (have_primary && !have_secondary && bindtemplate && !memcmp(JoyBinds[0], bindtemplate, sizeof(JoyBinds[0])))
		{
			for (int bindId = 0; bindId != _BIND_ID_COUNT; bindId++) { sprintf(key, "bind_port_%d_%s", (0 + 1), BindName[bindId]); ZL_Application::SettingsDel(key); DirtySettings(); }
			have_primary = false;
		}
		if (!have_primary)
		{
			if (!have_secondary && ZL_Application::SettingsHas("custom_controller_bindings")) { ZL_Application::SettingsDel("custom_controller_bindings"); DirtySettings(); }
			useCustomControllerBinding = false; // apply template below if needed
		}
	}

	if (!useCustomControllerBinding && bindtemplate)
		memcpy(JoyBinds[0], bindtemplate, sizeof(JoyBinds[0]));
	else
		JoyBindTemplateNames = NULL;
}

static void SetCaptureJoyBind(const SJoyBind& setbind)
{
	if (!memcmp(CaptureJoyBind, &setbind, sizeof(SJoyBind))) { CaptureJoyBind = NULL; return; }
	int idx = (int)(CaptureJoyBind - JoyBinds[0]), bindPort = (idx / _BIND_ID_COUNT), bindId = (idx % _BIND_ID_COUNT);
	*CaptureJoyBind = setbind;
	CaptureJoyBind = NULL;
	JoyBindTemplateNames = NULL; // disassociate on change

	char key[100];
	sprintf(key, "bind_port_%d_%s", (bindPort + 1), BindName[bindId]);
	mtxCoreOptions.Lock();
	ZL_Application::SettingsSet(key, setbind.ToConfig());
	if (!ZL_Application::SettingsHas("custom_controller_bindings"))
	{
		ZL_Application::SettingsSet("custom_controller_bindings", "true");
		for (int idx = 0; idx != _BIND_PORTS * _BIND_ID_COUNT; idx++)
		{
			int bindPort = (idx / _BIND_ID_COUNT), bindId = (idx % _BIND_ID_COUNT);
			if (!JoyBinds[bindPort][bindId].Joy) continue;
			sprintf(key, "bind_port_%d_%s", (bindPort + 1), BindName[bindId]);
			ZL_Application::SettingsSet(key, JoyBinds[bindPort][bindId].ToConfig());
		}
	}
	DirtySettings();
	mtxCoreOptions.Unlock();
}

static void ApplyFPSLimit(unsigned char newThrottleMode = ThrottleMode, bool unpause = false)
{
	ThrottleMode = newThrottleMode;
	double rate = av.timing.fps;
	if (newThrottleMode == RETRO_THROTTLE_SLOW_MOTION) rate *= SlowRate;
	else if (newThrottleMode == RETRO_THROTTLE_FAST_FORWARD && FastRate && (rate *= FastRate) >= FAST_FPS_LIMIT) rate /= (int)FastRate;
	ZL_Application::SetFpsLimit((float)rate);
	if (unpause) ThrottlePaused = false;
	AudioSkip = true;
}

static std::string GetSavePath()
{
	const std::string& content_name = DBPS_GetContentName();
	return ((std::string(PathSaves) += '/').append(content_name.empty() ? "DOSBox-pure" : content_name.c_str()).append(".state") += (DBPS_SaveSlotIndex ? (char)('0' + DBPS_SaveSlotIndex) : '\0'));
}

static void RunSave()
{
	DoSave = DoLoad = false;
	enum { RZIP_VERSION = 1, RZIP_COMPRESSION_LEVEL = 6, RZIP_DEFAULT_CHUNK_SIZE = 131072 };
	size_t sz = retro_serialize_size(), maxdeflate = ZL_Compression::CompressMaxSize(RZIP_DEFAULT_CHUNK_SIZE), deflate_written;
	unsigned char *buf = (unsigned char*)malloc(16 + sz + 4 + maxdeflate), *mem = buf + 16, *chnk = mem + sz;
	if (!retro_serialize(mem, sz)) { free(buf); return; }

	// Prepend with RASTATE header
	memcpy(buf, "RASTATE\1MEM ", 12);
	buf[12] = (unsigned char)(sz & 0xFF); buf[13] = (unsigned char)((sz >> 8) & 0xFF); buf[14] = (unsigned char)((sz >> 16) & 0xFF); buf[15] = (unsigned char)((sz >> 24) & 0xFF);
	sz += 16;

	FILE* f = fopen_wrap(GetSavePath().c_str(), "wb");
	unsigned char rzip_header[] =
	{
		// > 'Magic numbers' - first 8 bytes
		35, 82, 90, 73, 80, 118, RZIP_VERSION, 35, // #RZIPv*#
		// > Uncompressed chunk size - next 4 bytes
		(RZIP_DEFAULT_CHUNK_SIZE & 0xFF), ((RZIP_DEFAULT_CHUNK_SIZE >> 8) & 0xFF), ((RZIP_DEFAULT_CHUNK_SIZE >> 16) & 0xFF), ((RZIP_DEFAULT_CHUNK_SIZE >> 24) & 0xFF),
		// > Total uncompressed data size - next 8 bytes
		(unsigned char)(sz & 0xFF), (unsigned char)((sz >> 8) & 0xFF), (unsigned char)((sz >> 16) & 0xFF), (unsigned char)((sz >> 24) & 0xFF), (unsigned char)(((Bit64u)sz >> 32) & 0xFF), (unsigned char)(((Bit64u)sz >> 40) & 0xFF), (unsigned char)(((Bit64u)sz >> 48) & 0xFF), (unsigned char)(((Bit64u)sz >> 56) & 0xFF),
	};
	if (!fwrite(rzip_header, sizeof(rzip_header), 1, f)) goto fail;

	for (size_t i = 0; i < sz; i += RZIP_DEFAULT_CHUNK_SIZE)
	{
		// Write compressed chunk to file
		if (!ZL_Compression::Compress(buf + i, ZL_Math::Min(sz - i, (size_t)RZIP_DEFAULT_CHUNK_SIZE), chnk + 4, &(deflate_written = maxdeflate), RZIP_COMPRESSION_LEVEL)) goto fail;
		chnk[0] = (deflate_written & 0xFF); chnk[1] = ((deflate_written >> 8) & 0xFF); chnk[2] = ((deflate_written >> 16) & 0xFF); chnk[3] = ((deflate_written >> 24) & 0xFF);
		if (!fwrite(chnk, 4 + deflate_written, 1, f)) goto fail;
	}
	if (0) { fail: vecNotify.push_back({ ZL_TextBuffer(fntOSD, "Error while saving state"), 5000, RETRO_LOG_ERROR, ZLTICKS, 0.0f }); }
	else vecNotify.push_back({ ZL_TextBuffer(fntOSD, "Saved State"), 1000, RETRO_LOG_WARN, ZLTICKS, 0.0f });
	fclose(f);
}

static void RunLoad()
{
	DoSave = DoLoad = false;
	unsigned char* buf = NULL, rzip_header[20], chunk_header[4], *mem;
	size_t sz;
	FILE* f = fopen_wrap(GetSavePath().c_str(), "rb");
	if (!f) goto fail;
	if (fread(rzip_header, sizeof(rzip_header), 1, f) && !memcmp(rzip_header, "#RZIPv\1#", 8))
	{
		// Get uncompressed chunk size - next 4 bytes
		size_t chunk_size = ((size_t)rzip_header[8]) | ((size_t)rzip_header[9] << 8) | ((size_t)rzip_header[10] << 16) | ((size_t)rzip_header[11] << 24), maxdeflate = ZL_Compression::CompressMaxSize(chunk_size), deflate_read, decomp_size;
		// Get total uncompressed data size - next 8 bytes
		sz = (size_t)(((Bit64u)rzip_header[12]) | ((Bit64u)rzip_header[13] << 8) | ((Bit64u)rzip_header[14] << 16) | ((Bit64u)rzip_header[15] << 24) | ((Bit64u)rzip_header[16] << 32) | ((Bit64u)rzip_header[17] << 40) | ((Bit64u)rzip_header[18] << 48) | ((Bit64u)rzip_header[19] << 56));
		buf = (unsigned char*)malloc(sz + maxdeflate);
		unsigned char* chnk = buf + sz;
		for (size_t i = 0; i != sz; i += chunk_size)
			if (!fread(chunk_header, sizeof(chunk_header), 1, f) || (deflate_read = ((size_t)chunk_header[0]) | ((size_t)chunk_header[1] << 8) | ((size_t)chunk_header[2] << 16) | ((size_t)chunk_header[3] << 24)) == 0
				|| !fread(chnk, deflate_read, 1, f) || !ZL_Compression::Decompress(chnk, deflate_read, buf + i, &(decomp_size = (chunk_size = ZL_Math::Min(sz - i, chunk_size)))) || decomp_size != chunk_size) goto fail;
	}
	else
	{
		fseek(f, 0, SEEK_END);
		sz = (size_t)ftell(f);
		buf = (unsigned char*)malloc(sz);
		fseek(f, 0, SEEK_SET);
		if (!fread(buf, sz, 1, f)) sz = 0;
	}
	mem = buf;
	if (sz > 8 && !memcmp(buf, "RASTATE\1", 8)) // Find mem block in RASTATE
		for (size_t i = 8, block_size; i + 8 < sz; i += 8 + (((block_size) + 7) & ~7)) // Align to 8-byte boundary
		{
			block_size = buf[i+4] | (buf[i+5] << 8) | (buf[i+6] << 16) | (buf[i+7] << 24);
			if (memcmp(buf + i, "MEM ", 4)) continue;
			mem = buf + i + 8;
			sz = block_size;
			break;
		}
	if (!retro_unserialize(mem, sz)) {} // will show error on its own
	else if (0) { fail: vecNotify.push_back({ ZL_TextBuffer(fntOSD, "Error while loading state"), 5000, RETRO_LOG_ERROR, ZLTICKS, 0.0f }); }
	else vecNotify.push_back({ ZL_TextBuffer(fntOSD, "Loaded State"), 1000, RETRO_LOG_INFO, ZLTICKS, 0.0f });
	if (f) fclose(f);
	free(buf);
}

static retro_proc_address_t RETRO_CALLCONV retro_hw_get_proc_address(const char *sym)
{
	return (retro_proc_address_t)SDL_GL_GetProcAddress(sym);
}

uintptr_t RETRO_CALLCONV retro_hw_get_current_framebuffer(void)
{
	extern unsigned ZL_Surface_GetGLFrameBuffer(ZL_Surface* srf);
	return ZL_Surface_GetGLFrameBuffer(&srfCore);
}

static bool RETRO_CALLCONV retro_environment_cb(unsigned cmd, void *data)
{
	ZL_ASSERT(MainThreadID == SDL_GetThreadID() || cmd == RETRO_ENVIRONMENT_GET_VFS_INTERFACE || cmd == RETRO_ENVIRONMENT_GET_VARIABLE || cmd == RETRO_ENVIRONMENT_SET_VARIABLE || cmd == RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY || cmd == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY);
	static bool variables_updated;
	switch (cmd)
	{
		case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
			return true;
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
			*((const char **)data) = PathSaves.c_str();
			return true;
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
			*((const char **)data) = PathSystem.c_str();
			return true;
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			return false;
		case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
			retro_keyboard_event_cb = ((struct retro_keyboard_callback*)data)->callback;
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
			//static retro_core_options_update_display_callback_t update_display_cb;
			//update_display_cb = ((struct retro_core_options_update_display_callback*)data)->callback;
			//return true;
			return false;
		case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
			//static retro_disk_control_ext_callback disk_control_cb;
			//disk_control_cb = *((struct retro_disk_control_ext_callback*)data);
			//return true;
			return false;
		case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
			return false;
		case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
			//((struct retro_perf_callback*)data)->get_time_usec = retro_perf_get_time_usec;
			return false;
		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
			*(unsigned*)data = 2;
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
			mtxCoreOptions.Lock();
			for (const retro_core_option_v2_definition *it = ((retro_core_options_v2*)data)->definitions; it->key; it++)
			{
				if (!ZL_Application::SettingsHas(it->key)) continue;
				ZL_String val = ZL_Application::SettingsGet(it->key);
				if (val != it->default_value) { for (const retro_core_option_value *v = it->values; v->value; v++) { if (val == v->value) goto keyok; } }
				static const char* allowlist[] = { "interface_fastrate", "interface_slowrate", "interface_audiolatency",
					"dosbox_pure_force60fps", "dosbox_pure_menu_transparency", "dosbox_pure_mouse_speed_factor", "dosbox_pure_mouse_speed_factor_x", "dosbox_pure_joystick_analog_deadzone",
					"dosbox_pure_cycles", "dosbox_pure_cycles_max", "dosbox_pure_cycles_scale", "dosbox_pure_cycle_limit" };
				for (const char* p : allowlist) { if (!strcmp(it->key, p)) goto keyok; }
				ZL_Application::SettingsDel(it->key);
				keyok:;
			}
			mtxCoreOptions.Unlock();
			return true;
		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			bool res = true;
			mtxCoreOptions.Lock();
			ZL_String val = ZL_Application::SettingsGet(((retro_variable*)data)->key);
			if (!val.empty())
			{
				static ZL_Json storage; // to keep the string pointers stable
				ZL_Json it = storage[((retro_variable*)data)->key];
				it.SetString(val.c_str());
				((retro_variable*)data)->value = it.GetString();
			}
			else res = false;
			mtxCoreOptions.Unlock();
			return res;
		}
		case RETRO_ENVIRONMENT_SET_VARIABLE:
		{
			mtxCoreOptions.Lock();
			if (!((retro_variable*)data)->value) ZL_Application::SettingsDel(((retro_variable*)data)->key);
			else ZL_Application::SettingsSet(((retro_variable*)data)->key, ((retro_variable*)data)->value);
			DirtySettings();
			variables_updated = true;
			if (((retro_variable*)data)->key[0] == 'i' || !strcmp(((retro_variable*)data)->key, "dosbox_pure_aspect_correction")) DoApplyInterfaceOptions = true;
			mtxCoreOptions.Unlock();
			return true;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			*((bool*)data) = variables_updated;
			variables_updated = false;
			return true;
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
			return false;
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
			return false;
		case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
			return false;
		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
			return false;
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
			return false;
		case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
		{
			retro_vfs_interface_info* vfs = (retro_vfs_interface_info*)data;
			static retro_vfs_interface vfs_iface =
			{
				NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				retro_vfs_mkdir_impl,
				retro_vfs_opendir_impl,
				retro_vfs_readdir_impl,
				retro_vfs_dirent_get_name_impl,
				retro_vfs_dirent_is_dir_impl,
				retro_vfs_closedir_impl
			};
			vfs->iface = &vfs_iface;
			return true;
		}
		case RETRO_ENVIRONMENT_GET_THROTTLE_STATE:
		{
			float corefps = (float)av.timing.fps, vsyncfps = ZL_Application::GetVsyncFps();
			if (vsyncfps && vsyncfps != corefps)
			{
				((retro_throttle_state*)data)->mode = RETRO_THROTTLE_VSYNC;
				((retro_throttle_state*)data)->rate = vsyncfps;
			}
			else
			{
				((retro_throttle_state*)data)->mode = ThrottleMode;
				((retro_throttle_state*)data)->rate = corefps * (ThrottleMode == RETRO_THROTTLE_SLOW_MOTION ? SlowRate : ThrottleMode == RETRO_THROTTLE_FAST_FORWARD ? FastRate : ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING ? 0.0f : 1.0f);
			}
			return true;
		}
		case RETRO_ENVIRONMENT_SET_GEOMETRY:
			ZL_ASSERT(av.geometry.max_width == ((retro_game_geometry*)data)->max_width && av.geometry.max_height == ((retro_game_geometry*)data)->max_height);
			av.geometry = *((retro_game_geometry*)data);
			DoApplyGeometry = true;
			return true;
		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
			av = *((retro_system_av_info*)data);
			ApplyFPSLimit();
			if (av.geometry.max_width > (unsigned)srfCore.GetWidth() ||av.geometry.max_height > (unsigned)srfCore.GetHeight())
			{
				srfCore = ZL_Surface(av.geometry.max_width, av.geometry.max_height);
				DoApplyInterfaceOptions = true;
			}
			else { srfCore.RenderToBegin(true, false); srfCore.RenderToEnd(); } // clear to black
			DoApplyGeometry = true;
			return true;
		case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
			#ifdef ZL_VIDEO_OPENGL_ES2
			#define DBP_RETRO_HW_CONTEXT RETRO_HW_CONTEXT_OPENGLES2
			#elif defined(ZL_VIDEO_OPENGL_CORE)
			#define DBP_RETRO_HW_CONTEXT RETRO_HW_CONTEXT_OPENGL_CORE
			#else
			#define DBP_RETRO_HW_CONTEXT RETRO_HW_CONTEXT_OPENGL
			#endif
			*(retro_hw_context_type*)data = DBP_RETRO_HW_CONTEXT;
			return true;
		case RETRO_ENVIRONMENT_SET_HW_RENDER:
			ZL_ASSERT(((retro_hw_render_callback*)data)->context_type == DBP_RETRO_HW_CONTEXT);
			((retro_hw_render_callback*)data)->get_proc_address = retro_hw_get_proc_address;
			((retro_hw_render_callback*)data)->get_current_framebuffer = retro_hw_get_current_framebuffer;
			((retro_hw_render_callback*)data)->context_reset();
			return true;
		case RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE:
			return true;
		case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
		{
			const retro_message_ext* msg = (const retro_message_ext*)data;
			if (msg->type == RETRO_MESSAGE_TYPE_STATUS)
				{ txtOSD.SetText(msg->msg, ZLWIDTH); txtOSDTick = ZLTICKS; }
			else if (msg->type == RETRO_MESSAGE_TYPE_NOTIFICATION)
				vecNotify.push_back({ ZL_TextBuffer(fntOSD, msg->msg), msg->duration, msg->level, ZLTICKS, 0.0f });
			else { ZL_ASSERT(0); }
			return true;
		}
		case RETRO_ENVIRONMENT_SHUTDOWN:
			ZL_Application::Quit();
			return true;
	}
	ZL_ASSERTMSG1(false, "[retro_environment_cb] UNSUPPORTED CMD: %d", cmd);
	return false;
}

void MIXER_CallBack(void* userdata, unsigned char* stream, int len);
static void ProcessAudioBuffer(short* buffer, unsigned int samples)
{
	MIXER_CallBack(NULL, (unsigned char*)buffer, samples * 4);

	if (AudioMuted)
		memset(buffer, 0, samples * 4);
	else
		for (size_t i = 0; i < samples * 2; ++i)
			buffer[i] = ZL_Math::Clamp<short>(buffer[i] * AudioVolume, -32768, 32767);
}

static bool AudioMix(short* buffer, unsigned int samples, bool need_mix)
{
	unsigned char tm = (LastAudioThrottleMode == RETRO_THROTTLE_FAST_FORWARD ? RETRO_THROTTLE_FAST_FORWARD : ThrottleMode);
	LastAudioThrottleMode = ThrottleMode;

	extern Bit32u DBP_MIXER_DoneSamplesCount();
	size_t have = DBP_MIXER_DoneSamplesCount(), want = samples;
	if (tm == RETRO_THROTTLE_FAST_FORWARD) want = (FastRate ? (size_t)(samples * FastRate) : (size_t)DBP_MIXER_DoneSamplesCount());
	if (tm == RETRO_THROTTLE_SLOW_MOTION) want = (size_t)(samples * SlowRate);
	for (unsigned int tick = 0; have < want;)
	{
		ZL_Thread::Sleep(0);
		have = DBP_MIXER_DoneSamplesCount();
		if (have >= want) break;
		if (!tick) tick = SDL_GetTicks();
		if (((int)SDL_GetTicks() - (int)tick) >= (AudioLatency)) { /*ZL_LOG("AUDIOMIX", "Not enough audio");*/ break; } // emulation lagging (or crashed)
	}

	if (have == 0)
	{
		//ZL_LOG("AUDIOMIX", "Have zero audio");
		memset(buffer, 0, samples * 4);
		return true;
	}

	const bool catchup = (have > want * (av.timing.fps < 50 ? 20 : 6)/4 && want == samples);
	static unsigned int catchups;
	if (catchup || catchups)
	{
		// When running with vsync we need to allow catching up for potentially much longer (until the emulation skips a frame)
		unsigned int skip_catchups = 30; float corefps = (float)av.timing.fps, vsyncfps = ZL_Application::GetVsyncFps();
		if (vsyncfps && vsyncfps > corefps) skip_catchups += (unsigned int)(1000.0f / ((vsyncfps - corefps) * AudioLatency));
		catchups += (catchup ? 2 : -1);
		//if (catchups > skip_catchups) want = samples * (((have - want) <= 800 ? 40800 : (40000 + have - want))) / 40000; // alternative speed up playback
		//if (catchups > skip_catchups*2) catchups = 20;
		if (catchups > skip_catchups || have > want * 3) AudioSkip = true;
		ZL_LOG("AUDIOMIX", "Catch-up - Have %d but want %d (catchups: %u)", (int)have, (int)want, catchups);
	}

	if (have < want || want != samples || AudioSkip || tm == RETRO_THROTTLE_FRAME_STEPPING)
	{
		enum { UI_MAX_SAMPLES = 4096*4 };
		static short stretchbuf[UI_MAX_SAMPLES * 2]; // stereo

		size_t use;
		if (tm == RETRO_THROTTLE_FAST_FORWARD || tm == RETRO_THROTTLE_SLOW_MOTION) use = ZL_Math::Min(have, want);
		else if (have >= want || AudioSkip || tm == RETRO_THROTTLE_FRAME_STEPPING)
		{
			// When lagging behind, don't speed audio up and just scrap it instead (one hiccup is better than prolonged audio speedup)
			if (AudioSkip) { ZL_LOG("AUDIOMIX", "Audio Skip!"); }
			AudioSkip = false;
			catchups = 0;
			use = ZL_Math::Min(have, (size_t)samples);
		}
		else use = ((have < 10) ? have : (have * 7 / 10)); // use 70% for stretching while ahead, keep 20% as additional buffer to not fall behind again

		if (use > UI_MAX_SAMPLES) use = UI_MAX_SAMPLES;
		const double audio_stretch = (tm == RETRO_THROTTLE_FRAME_STEPPING ? 1.0 : (double)use / samples); // don't stretch during frame stepping
		ZL_LOG("AUDIOMIX", "Stretch %d (of %d available total) into %d (factor %f)", (int)use, (int)have, (int)samples, (float)audio_stretch);
		for (size_t scrap, keep = want / 5; have >= samples && have > use + keep; have -= scrap)
		{
			scrap = ZL_Math::Min((size_t)(have - use - keep), (size_t)UI_MAX_SAMPLES);
			ProcessAudioBuffer(stretchbuf, (int)scrap);
			ZL_LOG("AUDIOMIX", "Scrapping %d (of %d available total)", (int)scrap, (int)have);
		}
		ProcessAudioBuffer(stretchbuf, (int)use);

		if (0)
		{
			for (size_t i = 0; i != samples*2; i++, buffer++) // repeat samples
			{
				buffer[0] = stretchbuf[i < (use*2) ? i : (use*2) - (i % (use*2))];
			}
		}
		else
		{
			for (size_t i = 0, jMax = use - 1; i != samples; i++, buffer += 2) // linear interpolation
			{
				const double src_pos_float = i * audio_stretch;
				const size_t j0 = (size_t)src_pos_float, j1 = j0 + (size_t)(j0 != jMax);
				const double frac = src_pos_float - j0;
				buffer[0] = (short)((1.0 - frac) * stretchbuf[j0 * 2 + 0] + frac * stretchbuf[j1 * 2 + 0]); // stretchbuf[j0 * 2 + 0] // simple nearest
				buffer[1] = (short)((1.0 - frac) * stretchbuf[j0 * 2 + 1] + frac * stretchbuf[j1 * 2 + 1]); // stretchbuf[j0 * 2 + 1] // simple nearest
			}
		}

		if (tm == RETRO_THROTTLE_FRAME_STEPPING && use < samples) { memset(buffer - (samples - use) * 2, 0, (samples - use) * 4); }
		ui_last_audio_stretch = (float)audio_stretch;
	}
	else
	{
		ProcessAudioBuffer(buffer, (int)want);
	}
	return true;
}

static void RETRO_CALLCONV retro_video_refresh_cb(const void *data, unsigned width, unsigned height, size_t pitch)
{
	if (!data) return; // skipped frame
	ZL_ASSERT(width <= (unsigned)srfCore.GetWidth() && height <= (unsigned)srfCore.GetHeight() && data == RETRO_HW_FRAME_BUFFER_VALID); // only accept HWContext drawing with NULL data
	float scaleX = (float)width / srfCore.GetWidth(), scaleY = (float)height / srfCore.GetHeight();
	if (scaleX != srfCore.GetScaleW() || scaleY != srfCore.GetScaleH())
	{
		srfCore.SetScaleTo((float)width, (float)height);
		DoApplyInterfaceOptions = true;
	}
}

void DBPS_SubmitOSDFrame(const void *data, unsigned width, unsigned height)
{
	ZL_ASSERT(width == DBPS_OSD_WIDTH && height == DBPS_OSD_HEIGHT);
	srfOSD.SetScaleTo((float)width, (float)height);
	srfOSD.SetPixels((const unsigned char*)data, 0, 0, width, height, 4);
	unsigned p0 = ((unsigned int*)data)[0], p1 = ((unsigned int*)data)[width/2], p2 = ((unsigned int*)data)[width-1], osdbg = ((p0 == p1 || p0 == p2) ? p0 : p1);
	colOSDBG = ZL_Color(s((osdbg>>16)&0xFF)/s(255), s((osdbg>>8)&0xFF)/s(255), s(osdbg&0xFF)/s(255), s((osdbg>>24)&0xFF)/s(255));
}

static void RETRO_CALLCONV retro_input_poll_cb(void)
{
	ZL_ASSERT(MainThreadID == SDL_GetThreadID());
}

static int16_t RETRO_CALLCONV retro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
	ZL_ASSERT(MainThreadID == SDL_GetThreadID());
	if (device == RETRO_DEVICE_KEYBOARD)
	{
		return (id < RETROK_LAST ? (int16_t)RETROKDown[id] : 0);
	}

	if (device == RETRO_DEVICE_MOUSE && (!CaptureJoyBind || id != RETRO_DEVICE_ID_MOUSE_RIGHT))
	{
		if (id == RETRO_DEVICE_ID_MOUSE_X)         return (int16_t)ZL_Input::MouseDelta().x;
		if (id == RETRO_DEVICE_ID_MOUSE_Y)         return (int16_t)-ZL_Input::MouseDelta().y;
		if (id == RETRO_DEVICE_ID_MOUSE_LEFT)      return (int16_t)ZL_Input::Held(ZL_BUTTON_LEFT);
		if (id == RETRO_DEVICE_ID_MOUSE_RIGHT)     return (int16_t)ZL_Input::Held(ZL_BUTTON_RIGHT);
		if (id == RETRO_DEVICE_ID_MOUSE_MIDDLE)    return (int16_t)(UseMiddleMouseMenu ? 0 : ZL_Input::Held(ZL_BUTTON_MIDDLE));
		if (id == RETRO_DEVICE_ID_MOUSE_WHEELUP)   return (int16_t)(ZL_Input::MouseWheel() > 0);
		if (id == RETRO_DEVICE_ID_MOUSE_WHEELDOWN) return (int16_t)(ZL_Input::MouseWheel() < 0);
	}

	if (device == RETRO_DEVICE_POINTER)
	{
		if (id == RETRO_DEVICE_ID_POINTER_X)
		{
			scalar x = ((PointerLock && !DBPS_IsShowingOSD()) ? PointerLockPos.x : (SDL_GetMouseFocus() ? ZL_Input::Pointer().x : ZLHALFW));
			if (x <= core_rec.left) return -0x7fff;
			if (x >= core_rec.right) return 0x7fff;
			return (int16_t)((x - core_rec.left) / core_rec.Width() * 65534.99f - 32767.495f);
		}
		if (id == RETRO_DEVICE_ID_POINTER_Y)
		{
			scalar y = ((PointerLock && !DBPS_IsShowingOSD()) ? PointerLockPos.y : (SDL_GetMouseFocus() ? ZL_Input::Pointer().y : ZLHALFH));
			if (y <= core_rec.low) return 0x7fff;
			if (y >= core_rec.high) return -0x7fff;
			return (int16_t)((y - core_rec.low) / core_rec.Height() * -65534.99f - 32767.495f);
		}
		if (id == RETRO_DEVICE_ID_POINTER_PRESSED) return (int16_t)ZL_Input::Held(ZL_BUTTON_LEFT);
	}

	#ifndef NDEBUG // debug only, for lightgun/screenmouse/screenpointer test code in retro_run
	if (device == RETRO_DEVICE_LIGHTGUN || device == (RETRO_DEVICE_MOUSE | 0x10000) || device == (RETRO_DEVICE_POINTER | 0x10000)) return 0;
	#endif

	if (id == RETRO_DEVICE_ID_JOYPAD_MASK && device == RETRO_DEVICE_JOYPAD) return 0; // only used to detect game focus in other frontends

	if (CaptureJoyBind)
	{
		if (device != RETRO_DEVICE_MOUSE) return 0;
		if (ZL_Input::Held(ZL_BUTTON_RIGHT)) { CaptureJoyBind = NULL; return 1; }
		static const signed char dirsAxis[] = { -1, 1 }, dirsHat[] = { ZL_HAT_UP, ZL_HAT_RIGHT, ZL_HAT_DOWN, ZL_HAT_LEFT }, dirsBall[] = { -2, -1, 1, 2 }, dirsButton[] = { 0 };
		static const signed char *dirs[] = { NULL, dirsAxis, dirsHat, dirsBall, dirsButton };
		static const unsigned char dirsCount[] = { 0, COUNT_OF(dirsAxis), COUNT_OF(dirsHat), COUNT_OF(dirsBall), COUNT_OF(dirsButton) };
		size_t num = 0, have = CaptureJoyState.size();
		SJoyBind tst = { NULL }; // zero bytes including padding
		for (ZL_JoystickData* j : vecJoys)
		{
			tst.Joy = j;
			const unsigned char nums[] = { 0, (unsigned char)j->naxes, (unsigned char)j->nhats, (unsigned char)j->nballs, (unsigned char)j->nbuttons };
			for (tst.From = SJoyBind::FROM_AXIS; tst.From <= SJoyBind::FROM_BUTTON; tst.From = (SJoyBind::EFrom)(tst.From + 1))
				for (tst.Num = nums[tst.From]; tst.Num--;)
					for (int d = dirsCount[tst.From]; d--; num++)
					{
						tst.Dir = dirs[tst.From][d];
						unsigned char v = (unsigned char)tst.GetVal(false), &w = (num >= have ? (CaptureJoyState.push_back(v),CaptureJoyState[num]) : CaptureJoyState[num]);
						if (v != (w & 1)) { if (w < 2 || v) w += 3; else { SetCaptureJoyBind(tst); return 1; } }
					}
		}
		return 0;
	}

	EBindId bid = GetBindIdFromRetro(device, index, id);
	if (bid == _BIND_ID_COUNT) return 0;

	if (bid < _BIND_ID_FIRST_AXIS)
	{
		SJoyBind& bnd = JoyBinds[port][bid];
		return (bnd.Joy ? (int)bnd.GetVal(true) : 0);
	}
	else
	{
		SJoyBind &bndneg = JoyBinds[port][bid], &bndpos = JoyBinds[port][bid+1];
		return 0 - (bndneg.Joy ? bndneg.GetVal(true) : 0) + (bndpos.Joy ? bndpos.GetVal(true) : 0);
	}
}

void DBPS_StartCaptureJoyBind(unsigned port, unsigned device, unsigned index, unsigned id, bool axispos)
{
	ZL_ASSERT(port < _BIND_PORTS);
	EBindId bid = GetBindIdFromRetro(device, index, id, axispos);
	if (bid == _BIND_ID_COUNT) return;
	CaptureJoyState.clear();
	CaptureJoyBind = &JoyBinds[port][bid];
}

bool DBPS_HaveJoy() { return !vecJoys.empty(); }

bool DBPS_GetJoyBind(unsigned port, unsigned device, unsigned index, unsigned id, bool axispos, std::string& outJoyName, std::string& outBind, const char* prefix)
{
	ZL_ASSERT(port < _BIND_PORTS);
	EBindId bid = GetBindIdFromRetro(device, index, id, axispos);
	if (bid == _BIND_ID_COUNT) return false;
	SJoyBind& bnd = JoyBinds[port][bid];
	if (!bnd.From) return false;
	bnd.Describe(outJoyName, outBind, prefix, (JoyBindTemplateNames ? JoyBindTemplateNames[bid] : NULL));
	return true;
}

void DBPS_GetMouse(short& mx, short& my, bool osd)
{
	extern ZL_Vector ZL_SdlQueryMousePos();
	const ZL_Vector p = ((PointerLock && !DBPS_IsShowingOSD()) ? PointerLockPos : (SDL_GetMouseFocus() ? ZL_SdlQueryMousePos() : ZLCENTER));
	const ZL_Rectf& rec = (osd ? osd_rec : core_rec);
	mx = ((p.x <= rec.left) ? (int16_t)-0x7fff : ((p.x >= rec.right) ? (int16_t)0x7fff : (int16_t)((p.x - rec.left) / rec.Width() * 65534.99f - 32767.495f)));
	my = ((p.y <= rec.low)  ? (int16_t)0x7fff : ((p.y >= rec.high)  ? (int16_t)-0x7fff : (int16_t)((p.y - rec.low) / rec.Height() * -65534.99f - 32767.495f)));
}

void DBPS_RequestSaveLoad(bool save, bool load)
{
	DoSave = save;
	DoLoad = load;
}

bool DBPS_HaveSaveSlot()
{
	if (DoSave) return true;
	FILE* f = fopen_wrap(GetSavePath().c_str(), "rb");
	if (f) fclose(f);
	return (f != NULL);
}

void DBPS_OnContentLoad(const char* name, const char* dir, size_t dirlen)
{
	extern void ZL_SdlSetTitle(const char* newtitle);
	ZL_SdlSetTitle(ZL_String("DOSBox Pure - ").append(name).c_str());
	if (dirlen)
	{
		ZL_String contentPath(dir, dirlen);
		Cross::NormalizePath(Cross::MakePathAbsolute(contentPath));
		if (ZL_Application::SettingsGet("interface_contentpath") != contentPath)
		{
			ZL_Application::SettingsSet("interface_contentpath", contentPath);
			DirtySettings();
		}
	}
	SynchronizeSettings(true);
	AudioSkip = true;
}

static bool OnKeyUseHotKey(ZL_KeyboardEvent& e)
{
	//static const char* zlknames[] = { "UNKNOWN", NULL, NULL, NULL, "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "RETURN", "ESCAPE", "BACKSPACE", "TAB", "SPACE", "MINUS", "EQUALS", "LEFTBRACKET", "RIGHTBRACKET", "BACKSLASH", "SHARP", "SEMICOLON", "APOSTROPHE", "GRAVE", "COMMA", "PERIOD", "SLASH", "CAPSLOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "PRINTSCREEN", "SCROLLLOCK", "PAUSE", "INSERT", "HOME", "PAGEUP", "DELETE", "END", "PAGEDOWN", "RIGHT", "LEFT", "DOWN", "UP", "NUMLOCKCLEAR", "KP_DIVIDE", "KP_MULTIPLY", "KP_MINUS", "KP_PLUS", "KP_ENTER", "KP_1", "KP_2", "KP_3", "KP_4", "KP_5", "KP_6", "KP_7", "KP_8", "KP_9", "KP_0", "KP_PERIOD", "NONUSBACKSLASH", "APPLICATION", "POWER", "KP_EQUALS", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "EXECUTE", "HELP", "MENU", "SELECT", "STOP", "AGAIN", "UNDO", "CUT", "COPY", "PASTE", "FIND", "MUTE", "VOLUMEUP", "VOLUMEDOWN", NULL, NULL, NULL, "KP_COMMA", "KP_EQUALSAS400", "INTERNATIONAL1", "INTERNATIONAL2", "INTERNATIONAL3", "INTERNATIONAL4", "INTERNATIONAL5", "INTERNATIONAL6", "INTERNATIONAL7", "INTERNATIONAL8", "INTERNATIONAL9", "LANG1", "LANG2", "LANG3", "LANG4", "LANG5", "LANG6", "LANG7", "LANG8", "LANG9", "ALTERASE", "SYSREQ", "CANCEL", "CLEAR", "PRIOR", "RETURN2", "SEPARATOR", "OUT", "OPER", "CLEARAGAIN", "CRSEL", "EXSEL", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "KP_00", "KP_000", "THOUSANDSSEPARATOR", "DECIMALSEPARATOR", "CURRENCYUNIT", "CURRENCYSUBUNIT", "KP_LEFTPAREN", "KP_RIGHTPAREN", "KP_LEFTBRACE", "KP_RIGHTBRACE", "KP_TAB", "KP_BACKSPACE", "KP_A", "KP_B", "KP_C", "KP_D", "KP_E", "KP_F", "KP_XOR", "KP_POWER", "KP_PERCENT", "KP_LESS", "KP_GREATER", "KP_AMPERSAND", "KP_DBLAMPERSAND", "KP_VERTICALBAR", "KP_DBLVERTICALBAR", "KP_COLON", "KP_HASH", "KP_SPACE", "KP_AT", "KP_EXCLAM", "KP_MEMSTORE", "KP_MEMRECALL", "KP_MEMCLEAR", "KP_MEMADD", "KP_MEMSUBTRACT", "KP_MEMMULTIPLY", "KP_MEMDIVIDE", "KP_PLUSMINUS", "KP_CLEAR", "KP_CLEARENTRY", "KP_BINARY", "KP_OCTAL", "KP_DECIMAL", "KP_HEXADECIMAL", NULL, NULL, "LCTRL", "LSHIFT", "LALT", "LGUI", "RCTRL", "RSHIFT", "RALT", "RGUI",  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "MODE", "AUDIONEXT", "AUDIOPREV", "AUDIOSTOP", "AUDIOPLAY", "AUDIOMUTE", "MEDIASELECT", "WWW", "MAIL", "CALCULATOR", "COMPUTER", "AC_SEARCH", "AC_HOME", "AC_BACK", "AC_FORWARD", "AC_STOP", "AC_REFRESH", "AC_BOOKMARKS", "BRIGHTNESSDOWN", "BRIGHTNESSUP", "DISPLAYSWITCH", "KBDILLUMTOGGLE", "KBDILLUMDOWN", "KBDILLUMUP", "EJECT", "SLEEP",  }; 
	//ZL_LOG("KEY", "DOWN: %d - %s", (int)e.is_down, zlknames[(int)e.key]);
	static bool pressedFs[12], pressedAltReturn, pressedApplication;
	if ((e.key == ZLK_RETURN || e.key == ZLK_F4) && (!DisableSystemALT || !DBPS_IsGameRunning()))
	{
		bool onlyAlt = ((e.mod & ZLKMOD_ALT) && !(e.mod & (ZLKMOD_CTRL|ZLKMOD_SHIFT|ZLKMOD_META)));
		if (e.key == ZLK_F4 && onlyAlt) { if (e.is_down) ZL_Application::Quit(); return true; }
		if (e.key == ZLK_RETURN)
		{
			if ((e.is_down && !onlyAlt) || pressedAltReturn == e.is_down) return false;
			pressedAltReturn = e.is_down;
			if (e.is_down) ZL_Display::ToggleFullscreen();
			return true;
		}
	}
	if (e.key == ZLK_APPLICATION) pressedApplication = e.is_down;


	unsigned short mod = ((e.mod & ZLKMOD_CTRL) ? ZLKMOD_CTRL : 0) | ((e.mod & ZLKMOD_SHIFT) ? ZLKMOD_SHIFT : 0) | ((e.mod & ZLKMOD_ALT) ? ZLKMOD_ALT : 0) | ((e.mod & ZLKMOD_META) ? ZLKMOD_META : 0) | (pressedApplication ? ZLKMOD_MODE : 0);

	if (mod == HotkeyMod)
	{
		if (e.key == ZLK_M && e.is_down && !e.is_repeat)
		{
			AudioMuted ^= true;
			vecNotify.push_back({ ZL_TextBuffer(fntOSD, (AudioMuted ? "Audio Muted" : "Audio Unmuted")), 500, RETRO_LOG_INFO, ZLTICKS, 0.0f });
			return true;
		}
		else if (e.key == ZLK_PAGEUP || e.key == ZLK_PAGEDOWN && e.is_down)
		{
			/* adjust audio volume by 10% */
			float delta = e.key == ZLK_PAGEUP ? AudioVolumeStep : -AudioVolumeStep;
			AudioVolume = ZL_Math::Clamp<float>(AudioVolume + delta, 0.0f, 1.0f);
			vecNotify.push_back({ ZL_TextBuffer(fntOSD, ZL_String::format("Audio Volume: %d%%", (int)std::round(AudioVolume * 100.0f))), 500, RETRO_LOG_INFO, ZLTICKS, 0.0f });
		}
	}

	const int f = (e.key - ZLK_F1);
	if (f < 0 || f > 11) return false;
	if ((e.is_down && ((mod & HotkeyMod) != HotkeyMod || (mod & ((ZLKMOD_CTRL|ZLKMOD_SHIFT|ZLKMOD_ALT|ZLKMOD_META|ZLKMOD_MODE)&~HotkeyMod)))) || pressedFs[f] == e.is_down) return false;
	pressedFs[f] = e.is_down;
	switch (f)
	{
		case (HOTKEY_F_QUICKSAVE-1):   if (e.is_down) RunSave(); return true;
		case (HOTKEY_F_QUICKLOAD-1):   if (e.is_down) RunLoad(); return true;
		case (HOTKEY_F_FULLSCREEN-1):  if (e.is_down) ZL_Display::ToggleFullscreen(); return true;
		case (HOTKEY_F_LOCKMOUSE-1):   if (e.is_down) { PointerLock ^= true; vecNotify.push_back({ ZL_TextBuffer(fntOSD, (PointerLock ? "Locked mouse pointer" : "Unlocked mouse pointer")), 500, RETRO_LOG_INFO, ZLTICKS, 0.0f }); } return true;
		case (HOTKEY_F_PAUSE-1):
			if (!e.is_down) return true;
			if (ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING) ApplyFPSLimit(RETRO_THROTTLE_NONE, true);
			else ApplyFPSLimit(RETRO_THROTTLE_FRAME_STEPPING);
			return true;
		case (HOTKEY_F_TOGGLEOSD-1):
			if (!e.is_down) return true;
			if (ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING) ThrottlePaused = false;
			else DBPS_ToggleOSD();
			return true;
		case (HOTKEY_F_SLOWMOTION-1):
		case (HOTKEY_F_FASTFORWARD-1):
		{
			unsigned char newMode = (f == (HOTKEY_F_SLOWMOTION-1) ? RETRO_THROTTLE_SLOW_MOTION : RETRO_THROTTLE_FAST_FORWARD);
			if (SpeedModHold)
			{
				if (e.is_down) ApplyFPSLimit(newMode);
				else if (ThrottleMode == newMode) ApplyFPSLimit(RETRO_THROTTLE_NONE, true);
			}
			else if (e.is_down)
			{
				ApplyFPSLimit((ThrottleMode == newMode ? RETRO_THROTTLE_NONE : newMode), true);
			}
			return true;
		}
	}
	return false;
}

static void OnKeyDown(ZL_KeyboardEvent& e)
{
	if (OnKeyUseHotKey(e)) return;
	unsigned rk = (unsigned)ZLKtoRETROKEY[e.key];
	if (e.is_repeat || rk == RETROK_UNKNOWN) return;
	RETROKDown[rk] = true;
	uint16_t mod =
		((e.mod & (ZLKMOD_SHIFT)) ? RETROKMOD_SHIFT : 0) |
		((e.mod & (ZLKMOD_CTRL)) ? RETROKMOD_CTRL : 0) |
		((e.mod & (ZLKMOD_ALT)) ? RETROKMOD_ALT : 0) |
		((e.mod & (ZLKMOD_META)) ? RETROKMOD_META : 0) |
		((e.mod & (ZLKMOD_NUM)) ? RETROKMOD_NUMLOCK : 0) |
		((e.mod & (ZLKMOD_CAPS)) ? RETROKMOD_CAPSLOCK : 0) |
		((e.mod & (ZLKMOD_RESERVED)) ? RETROKMOD_SCROLLOCK : 0);
	retro_keyboard_event_cb(true, rk, 0, mod);
}

static void OnKeyUp(ZL_KeyboardEvent& e)
{
	if (OnKeyUseHotKey(e)) return;
	unsigned rk = (unsigned)ZLKtoRETROKEY[e.key];
	if (rk == RETROK_UNKNOWN || !RETROKDown[rk]) return;
	RETROKDown[rk] = false;
	retro_keyboard_event_cb(false, (unsigned)ZLKtoRETROKEY[e.key], 0, 0);
}

static void OnDropFile(const ZL_String& path)
{
	DBPS_OpenContent(path.c_str());
}

static void OnJoyDeviceChange(bool added, int which)
{
	RefreshJoysticks();
}

static void OnResized(ZL_WindowResizeEvent& ev)
{
	if (ev.window_fullscreen) ZL_Application::SettingsSet("screen_fullscreen", "true");
	else 
	{
		ZL_Application::SettingsDel("screen_fullscreen");
		if (ev.window_maximized) ZL_Application::SettingsSet("screen_maximized", "true");
		else
		{
			ZL_Application::SettingsDel("screen_maximized");
			ZL_Application::SettingsSet("screen_width", (int)ZLWIDTH);
			ZL_Application::SettingsSet("screen_height", (int)ZLHEIGHT);
		}
	}
	DirtySettings();
	DoApplyGeometry = true;
}

static void ApplyGeometry()
{
	DoApplyGeometry = false;
	float core_ar = av.geometry.aspect_ratio, win_w = ZLWIDTH, win_h = ZL_Math::Max(ZLHEIGHT, 1.0f), win_ar = win_w / win_h, osd_w = DBPS_OSD_WIDTH, osd_h = DBPS_OSD_HEIGHT, osd_ar = osd_w / osd_h;
	if (DrawStretched) core_ar = win_ar;

	if (Scaling == 'I')
	{
		int int_w = (int)av.geometry.base_width, int_h = (int)(av.geometry.base_width / core_ar + 0.499f);
		while (int_w * 2 <= win_w && int_h * 2 <= win_h) { int_w *= 2; int_h *= 2; }
		while (int_w     >  win_w || int_h     >  win_h) { int_w /= 2; int_h /= 2; }
		core_rec = ZL_Rectf::BySize((float)(int)(win_w / 2 - int_w / 2 + .4999f), (float)(int)(win_h / 2 - int_h / 2 + .4999f), (float)int_w, (float)int_h);
	}
	else if (win_ar > core_ar) core_rec = ZL_Rectf((float)(int)((win_w - win_h * core_ar) * 0.5f + .4999f), 0, (float)(int)((win_w + win_h * core_ar) * 0.5f + .4999f), win_h);
	else                       core_rec = ZL_Rectf(0, (float)(int)((win_h - win_w / core_ar) * 0.5f + .4999f), win_w, (float)(int)((win_h + win_w / core_ar) * 0.5f + .4999f));

	if (win_ar > osd_ar) { osd_rec = ZL_Rectf((float)(int)((win_w - win_h * osd_ar) * 0.5f + .4999f), 0, (float)(int)((win_w + win_h * osd_ar) * 0.5f + .4999f), win_h); }
	else                 { osd_rec = ZL_Rectf(0, (float)(int)((win_h - win_w / osd_ar) * 0.5f + .4999f), win_w, (float)(int)((win_h + win_w / osd_ar) * 0.5f + .4999f)); }

	// Default (sharper) scaling will use nearest neighbor filter with no shader if scale factor is either large or (near) integer
	const float coreScale = (core_rec.Height() / av.geometry.base_height), coreScaleFrac = (coreScale - (int)coreScale);
	const bool coreScaleLinear = (CRTFilter || Scaling == 'B' || ((!Scaling || Scaling == 'D') && (coreScale < 3 && coreScaleFrac > 0.01f && coreScaleFrac < 0.99f)));
	srfCore.SetTextureFilterMode(coreScaleLinear, coreScaleLinear);
	DrawCoreShader = (shdrCore && (CRTFilter || !(!Scaling || Scaling == 'D') || coreScaleLinear));
}

static void ApplyInterfaceOptions()
{
	DoApplyInterfaceOptions = false;

	#define xstr(a) str(a)
	#define str(a) #a

	// GLSL shader code for CRT shader
	const char fragment_shader_crt_src[] = ZL_SHADER_SOURCE_HEADER(ZL_GLES_PRECISION_LOW) xstr(
		uniform sampler2D u_texture;
		varying vec2 v_texcoord;
		uniform float TextureSize_x;
		uniform float TextureSize_y;
		uniform float InputSize_x;
		uniform float InputSize_y;
		uniform float ShadowMask;
		uniform float ScanlineThinness; // Scanline Intensity (0.50 = fused scanlines, 0.70 = recommended default, 1.00 = thinner scanlines (too thin))
		uniform float HorizontalBlur; // Horizontal scan blur/sharpness (<-3.0 = sharper, -3.0 = pixely, -2.5 = default, -2.0 = smooth, -1.0 = too blurry)
		uniform float MaskValue; // Shadow mask effect (0.25 = large amount of mask (not recommended, too dark), 0.50 = recommended default, 1.00 = no shadow mask)
		uniform float Curvature; // Screen curvature (0.02 default, 0.0 none)
		uniform float Corner; // Rounded corner size (3 small, 11 large, 0 none)

		vec3 ToSrgb(vec3 c) { return vec3(1.055)*pow(c, vec3(1.0/2.4)) - vec3(0.055); }
		vec3 FromSrgb(vec3 c) { return pow((c + vec3(0.055))/vec3(1.055), vec3(2.4)); }
		vec3 PixFetch(vec2 uv) { return FromSrgb(texture2D(u_texture, uv, -16.0).rgb); }

		void main()
		{
			// Convert to {-1 to 1} range
			vec2 texsize = vec2(TextureSize_x, TextureSize_y);
			vec2 inpsize = vec2(InputSize_x, InputSize_y);
			vec2 pos = (texsize * v_texcoord * 2.0) / inpsize - 1.0;
			float vin = 1.0;

			if (Curvature != 0.0)
			{
				// Optional apply warp
				// Warp scanlines but not phosphor mask (0.0 = no warp, 1.0/64.0 = light warping, 1.0/32.0 = more warping)
				// Want x and y warping to be different (based on aspect)
				vec2 warp;
				warp.x = Curvature;
				warp.y = (3.0 / 4.0) * warp.x; // assume 4:3 aspect
				// Distort pushes image outside {-1 to 1} range
				pos *= (pos * pos).yx * warp + 1.0;
				vin = ((pos.x < -1.0 || pos.x > 1.0 || pos.y < -1.0 || pos.y > 1.0) ? 0.0 : 1.0);
			}

			if (Corner != 0.0)
			{
				vec2 dst = clamp(pos*pos, 0.0, 1.0);
				vin *= clamp(0.001 * (Corner + 998.0) * (dst.x * dst.y - dst.x - dst.y) * InputSize_y + InputSize_y, 0.0, 1.0);
			}

			// Leave in {0 to inpsize}
			pos = (pos + 1.0) * inpsize * 0.5;
			//--------------------------------------------------------------
			// Snap to center of first scanline
			float y0 = floor(pos.y - 0.5) + 0.5;
			// Snap to center of one of four pixels
			float x0 = floor(pos.x - 1.5) + 0.5;
			// Inital UV position
			vec2 texel = 1.0 / texsize;
			vec2 uv = vec2(x0, y0) * texel;
			vec3 colA0 = PixFetch(uv); uv.x += texel.x;
			vec3 colA1 = PixFetch(uv); uv.x += texel.x;
			vec3 colA2 = PixFetch(uv); uv.x += texel.x;
			vec3 colA3 = PixFetch(uv); uv.y += texel.y;
			vec3 colB3 = PixFetch(uv); uv.x -= texel.x;
			vec3 colB2 = PixFetch(uv); uv.x -= texel.x;
			vec3 colB1 = PixFetch(uv); uv.x -= texel.x;
			vec3 colB0 = PixFetch(uv);

			//--------------------------------------------------------------
			// Vertical filter
			// Scanline intensity is using sine wave
			// Easy filter window and integral used later in exposure
			float off = pos.y - y0;
			float pi2 = 6.28318530717958;
			float scanA = cos(min(0.5, off * ScanlineThinness) * pi2) * 0.5 + 0.5;
			float scanB = cos(min(0.5, -off * ScanlineThinness + ScanlineThinness) * pi2) * 0.5 + 0.5;
			//--------------------------------------------------------------
			// Horizontal kernel is simple Gaussian filter
			float off0 = pos.x - x0;
			float off1 = off0 - 1.0;
			float off2 = off0 - 2.0;
			float off3 = off0 - 3.0;
			float pix0 = exp2(HorizontalBlur*off0*off0);
			float pix1 = exp2(HorizontalBlur*off1*off1);
			float pix2 = exp2(HorizontalBlur*off2*off2);
			float pix3 = exp2(HorizontalBlur*off3*off3);
			float pixT = (1.0/(pix0 + pix1 + pix2 + pix3));
			// Get rid of wrong pixels on edge
			pixT *= vin;
			scanA *= pixT;
			scanB *= pixT;
			// Apply horizontal and vertical filters
			vec3 color = (colA0*pix0 + colA1*pix1 + colA2*pix2 + colA3*pix3)*scanA + (colB0*pix0 + colB1*pix1 + colB2*pix2 + colB3*pix3)*scanB;

			//--------------------------------------------------------------
			// Apply phosphor mask
			if (ShadowMask == 1.0) {} // no mask
			else if (ShadowMask == 2.0) // Aperture-grille
			{
				vec3 m = vec3(1.0);
				float x = fract(gl_FragCoord.x*(1.0 / 3.0));
				if      (x < (1.0 / 3.0)) m.r = MaskValue;
				else if (x < (2.0 / 3.0)) m.g = MaskValue;
				else                      m.b = MaskValue;
				color *= m;
			}
			else
			{
				vec3 m = vec3(MaskValue);
				float x;
				if (ShadowMask == 3.0) // Aperture-grille
				{
					x = fract(gl_FragCoord.x*(1.0 / 3.0));
				}
				if (ShadowMask == 4.0) // Stretched VGA style shadow mask
				{
					x = fract((gl_FragCoord.x + gl_FragCoord.y*2.9999)*(1.0 / 6.0));
				}
				if (ShadowMask == 5.0) // VGA style shadow mask.
				{
					x = fract((gl_FragCoord.x + floor(gl_FragCoord.y * 0.5)*2.9999)*(1.0 / 6.0));
				}
				if      (x < (1.0 / 3.0)) m.r = 1.0;
				else if (x < (2.0 / 3.0)) m.g = 1.0;
				else                      m.b = 1.0;
				color *= m;
			}

			//--------------------------------------------------------------
			// Optional color processing
			// Tonal curve parameters
			float contrast = 1.0;
			float saturation = 0.0;
			float tonemask = MaskValue;
			//--------------------------------------------------------------
			if (ShadowMask == 1.0) tonemask = 1.0;
			//--------------------------------------------------------------
			if (ShadowMask == 2.0)
			{
				// Normal R mask is {1.0,mask,mask}
				// LITE   R mask is {mask,1.0,1.0}
				tonemask = 0.5 + tonemask*0.5;
			}
			//--------------------------------------------------------------
			float midOut = 0.18 / ((1.5 - ScanlineThinness)*(0.5*tonemask + 0.5));
			float pMidIn = pow(0.18, contrast);
			vec4 tone;
			tone.x = contrast;
			tone.y = ((-pMidIn) + midOut) / ((1.0 - pMidIn)*midOut);
			tone.z = ((-pMidIn)*midOut + pMidIn) / (midOut*(-pMidIn) + midOut);
			tone.w = contrast + saturation;
			// Tonal control, start by protecting from /0
			float peak = max(1.0 / (256.0*65536.0), max(color.r, max(color.g, color.b)));
			// Compute the ratios of {R,G,B}
			vec3 ratio = color*(1.0/peak);
			// Apply tonal curve to peak value
			peak = pow(peak, tone.x);
			peak = peak*(1.0/(peak*tone.y + tone.z));
			// Apply saturation
			ratio = pow(ratio, vec3(tone.w, tone.w, tone.w));
			// Reconstruct color
			gl_FragColor.rgb = ratio*peak;
			gl_FragColor.rgb = ToSrgb(gl_FragColor.rgb);
			gl_FragColor.a = 1.0;
		}
	);

	// Simplified shader code to do nicer upscaling
	const char fragment_shader_scaling_src[] = ZL_SHADER_SOURCE_HEADER(ZL_GLES_PRECISION_LOW) xstr(
		uniform sampler2D u_texture;
		varying vec2 v_texcoord;
		uniform float TextureSize_x;
		uniform float TextureSize_y;

		vec3 ToSrgb(vec3 c) { return vec3(1.055)*pow(c, vec3(1.0/2.4)) - vec3(0.055); }
		vec3 FromSrgb(vec3 c) { return pow((c + vec3(0.055))/vec3(1.055), vec3(2.4)); }
		vec3 PixFetch(vec2 uv) { return FromSrgb(texture2D(u_texture, uv, -16.0).rgb); }

		void main()
		{
			vec2 texsize = vec2(TextureSize_x, TextureSize_y);
			vec2 pos = texsize * v_texcoord;
			float y0 = floor(pos.y - 0.5) + 0.5;
			float x0 = floor(pos.x - 1.5) + 0.5;
			vec2 texel = 1.0 / texsize;
			vec2 uv = vec2(x0, y0) * texel;
			vec3 colA0 = PixFetch(uv).rgb; uv.x += texel.x;
			vec3 colA1 = PixFetch(uv).rgb; uv.x += texel.x;
			vec3 colA2 = PixFetch(uv).rgb; uv.x += texel.x;
			vec3 colA3 = PixFetch(uv).rgb; uv.y += texel.y;
			vec3 colB3 = PixFetch(uv).rgb; uv.x -= texel.x;
			vec3 colB2 = PixFetch(uv).rgb; uv.x -= texel.x;
			vec3 colB1 = PixFetch(uv).rgb; uv.x -= texel.x;
			vec3 colB0 = PixFetch(uv).rgb; 
			float off = pos.y - y0;
			float scanA = cos(min(0.5, off * 0.5) * 6.28318530717958) * 0.5 + 0.5;
			float scanB = cos(min(0.5, 0.5 - off * 0.5) * 6.28318530717958) * 0.5 + 0.5;
			float off0 = pos.x - x0;
			float off1 = off0 - 1.0;
			float off2 = off0 - 2.0;
			float off3 = off0 - 3.0;
			float pix0 = exp2(-6.0*off0*off0);
			float pix1 = exp2(-6.0*off1*off1);
			float pix2 = exp2(-6.0*off2*off2);
			float pix3 = exp2(-6.0*off3*off3);
			float pixT = (1.0/(pix0 + pix1 + pix2 + pix3));
			gl_FragColor = vec4(ToSrgb((colA0*pix0 + colA1*pix1 + colA2*pix2 + colA3*pix3)*(scanA*pixT) + (colB0*pix0 + colB1*pix1 + colB2*pix2 + colB3*pix3)*(scanB*pixT)), 1.0);
		}
	);

	mtxCoreOptions.Lock();
	ZL_String hkmstr = ZL_Application::SettingsGet("interface_hotkeymod");
	int hkm = (hkmstr.empty() ? 1 : atoi(hkmstr.c_str()));
	HotkeyMod = ((hkm & 1) ? ZLKMOD_CTRL : 0) | ((hkm & 2) ? ZLKMOD_ALT : 0) | ((hkm & 4) ? ZLKMOD_SHIFT : 0) | ((hkm & 8) ? ZLKMOD_META : 0) | ((hkm & 16) ? ZLKMOD_MODE : 0);

	SpeedModHold = ((ZL_Application::SettingsGet("interface_speedtoggle").c_str()[0]|0x20) == 'h');
	const float newFastRate = (ZL_Application::SettingsHas("interface_fastrate") ? ZL_Math::Max((float)atof(ZL_Application::SettingsGet("interface_fastrate").c_str()), 1.001f) : 5.0f);
	const float newSlowRate = (ZL_Application::SettingsHas("interface_slowrate") ? ZL_Math::Min((float)atof(ZL_Application::SettingsGet("interface_slowrate").c_str()), 0.999f) : 0.3f);
	const bool defaultPointerLock = ((ZL_Application::SettingsGet("interface_lockmouse").c_str()[0]|0x20) == 't');
	DisableSystemALT = ((ZL_Application::SettingsGet("interface_systemhotkeys").c_str()[0]|0x20) == 'f');
	UseMiddleMouseMenu = ((ZL_Application::SettingsGet("interface_middlemouse").c_str()[0]|0x20) == 't');
	DrawStretched = !strcmp(ZL_Application::SettingsGet("dosbox_pure_aspect_correction").c_str(), "fill");
	Scaling = (ZL_Application::SettingsGet("interface_scaling").c_str()[0]&0x5f);
	CRTFilter = atoi(ZL_Application::SettingsGet("interface_crtfilter").c_str());
	const int audlatency = (ZL_Application::SettingsHas("interface_audiolatency" ) ? ZL_Math::Max(atoi(ZL_Application::SettingsGet("interface_audiolatency").c_str()), 5) : 25);

	static const char* sLastShaderSrc;
	const bool useCoreShader = (CRTFilter || !Scaling || Scaling == 'D');
	const char* shaderSrc = (CRTFilter ? fragment_shader_crt_src : (useCoreShader ? fragment_shader_scaling_src : NULL));
	if (shaderSrc != sLastShaderSrc) { sLastShaderSrc = shaderSrc; shdrCore = (shaderSrc ? ZL_Shader(shaderSrc, NULL, "TextureSize_x", "TextureSize_y", (CRTFilter ? 8 : 0), "InputSize_x", "InputSize_y", "ShadowMask", "ScanlineThinness", "HorizontalBlur", "MaskValue", "Curvature", "Corner") : ZL_Shader()); }

	int crtscanline  = (ZL_Application::SettingsHas("interface_crtscanline" ) ? atoi(ZL_Application::SettingsGet("interface_crtscanline" ).c_str()) : 1); //{ "0", "No scanline gaps" },{ "4", "Weak gaps" },{ "8", "Strong gaps" },
	int crtblur      = (ZL_Application::SettingsHas("interface_crtblur"     ) ? atoi(ZL_Application::SettingsGet("interface_crtblur"     ).c_str()) : 2); //{ "0", "Blurry" },{ "1", "Smooth" },{ "2", "Default" },{ "3", "Pixely" },{ "4", "Sharper" },
	int crtmask      = (ZL_Application::SettingsHas("interface_crtmask"     ) ? atoi(ZL_Application::SettingsGet("interface_crtmask"     ).c_str()) : 2); //{ "0", "Disabled" },{ "1", "Weak" },{ "2", "Default" },{ "3", "Strong" },{ "4", "Very Strong" },
	int crtcurvature = (ZL_Application::SettingsHas("interface_crtcurvature") ? atoi(ZL_Application::SettingsGet("interface_crtcurvature").c_str()) : 2); //{ "0", "Disabled" },{ "1", "Weak" },{ "2", "Default" },{ "3", "Strong" },{ "4", "Very Strong" },
	int crtcorner    = (ZL_Application::SettingsHas("interface_crtcorner"   ) ? atoi(ZL_Application::SettingsGet("interface_crtcorner"   ).c_str()) : 2); //{ "0", "Disabled" },{ "1", "Weak" },{ "2", "Default" },{ "3", "Strong" },{ "4", "Very Strong" },

	mtxCoreOptions.Unlock();

	float ScanlineThinness = 0.5f + (crtscanline * 0.05f);
	float HorizontalBlur = (crtblur == 0 ? -1.f : crtblur == 1 ? -2.f : crtblur == 2 ? -2.5f : (float)-crtblur);
	float MaskValue = 1.0f - (0.25f * crtmask);
	float Curvature = 0.01f * crtcurvature;
	float Corner = 2.2f * crtcorner;

	if (useCoreShader)
		shdrCore.SetUniform(s(srfCore.GetWidth()), s(srfCore.GetHeight()), s(srfCore.GetWidth()*srfCore.GetScaleW()), s(srfCore.GetHeight()*srfCore.GetScaleH()), s(CRTFilter), s(ScanlineThinness), s(HorizontalBlur), s(MaskValue), s(Curvature), s(Corner));

	if (newFastRate != FastRate || newSlowRate != SlowRate)
	{
		FastRate = newFastRate;
		SlowRate = newSlowRate;
		if (ThrottleMode) ApplyFPSLimit(); // apply change
	}

	if (audlatency != AudioLatency)
	{
		AudioSkip = true;
		AudioLatency = audlatency;
		ZL_Audio::Init(audlatency * 44100 / 1000);
	}

	if (defaultPointerLock != DefaultPointerLock)
	{
		if (DefaultPointerLock == PointerLock) { PointerLock ^= true; vecNotify.push_back({ ZL_TextBuffer(fntOSD, (PointerLock ? "Locked mouse pointer" : "Unlocked mouse pointer")), 500, RETRO_LOG_INFO, ZLTICKS, 0.0f }); }
		DefaultPointerLock = defaultPointerLock;
	}

	ApplyGeometry(); // apply int scaling, shader use, texture scaling mode
}

static bool DrawIntro()
{
	static ZL_Surface srfLogo;
	if (!srfLogo)
	{
		enum { dbplogopng_size = 12829 };
		extern const unsigned char* dbplogopng;
		srfLogo = ZL_Surface(ZL_File(dbplogopng, dbplogopng_size));
	}

	static ticks_t animstart;
	if (!animstart) animstart = ZLTICKS+50;

	const float early = ZL_Easing::OutCubic(ZL_Math::Clamp01(ZLSINCEF(animstart, 250)));
	const float f = ZL_Easing::InOutQuart(ZL_Math::Clamp01(ZLSINCEF(animstart+300, 1000)));
	const float scale = 1.5f*(ZLASPECTR > 1 ? 1/ZLASPECTR : 1); //ZL_Math::Min(ZLWIDTH, ZLHEIGHT) / 1800.0f;;
	const float logoW = scale * 0.93769841772151898734177215189873f, spineW = scale * 0.52397310126582278481012658227848f;
	const float dist = (1-f)*-10;
	const float rot = (1-f)*0.69813170079773183076947630739551f;
	const float ups = f * 0.21f * scale;

	const ZL_Color colBG     = ZLLUMA(0, (early < 1 ? 1.0-(early*.49f) : (0.7f-f)*.7f));
	const ZL_Color colLogo   = ZLLUMA(early, early < 1 ? early : f < 0.5 ? 1.0 : ZL_Easing::InQuad(1.0f - (f * 2.0f - 1.0f)));
	const ZL_Color colSpine  = ZLLUMA(early*0.94901960784313725490196078431373f, colLogo.a);
	const ZL_Matrix mtxBase  = ZL_Matrix::MakePerspectiveHorizontal(40, ZLASPECTR, 0, 1) * ZL_Matrix::MakeTranslate(0, ups, dist);
	const ZL_Matrix mtxSpine = mtxBase * ZL_Matrix::MakeRotate(ZL_Quat::FromRotateY(rot+PIHALF)) * ZL_Matrix::MakeTranslate(spineW, 0, -logoW);
	const ZL_Matrix mtxLogo  = mtxBase * ZL_Matrix::MakeRotate(ZL_Quat::FromRotateY(rot));

	ZL_Display::FillRect(0, 0, ZLWIDTH, ZLHEIGHT, colBG);
	ZL_Display::PushMatrix();
	if (mtxSpine.PerspectiveTransformPositionTo2D(ZL_Vector3(-spineW,0)).x > mtxSpine.PerspectiveTransformPositionTo2D(ZL_Vector3(spineW,0)).x)
	{
		ZL_Display::SetMatrix(mtxSpine);
		ZL_Display::FillQuad(-spineW,-scale , spineW,-scale , spineW,scale , -spineW,scale , colSpine);
	}
	ZL_Display::SetMatrix(mtxLogo);
	srfLogo.DrawQuad(-logoW,-scale , logoW,-scale , logoW,scale , -logoW,scale , colLogo);
	ZL_Display::PopMatrix();

	if (f == 1.0f) { srfLogo = ZL_Surface(); return true; }
	return false;
}

static void OnDraw()
{
	SynchronizeSettings(false);

	if (UseMiddleMouseMenu && ZL_Input::Down(ZL_BUTTON_MIDDLE)) DBPS_ToggleOSD();

	bool showOSD = DBPS_IsShowingOSD();
	static bool doPointerLock, doHideCursor, lastHiddenCursor;
	if (PointerLock && !showOSD)
	{
		if (!doPointerLock) { ZL_Display::SetPointerLock((doPointerLock = true)); PointerLockPos = ZL_Input::Pointer(); }
		PointerLockPos = (showOSD ? osd_rec : core_rec).Clamp(PointerLockPos + ZL_Input::MouseDelta());
	}
	else
	{
		if (doPointerLock) ZL_Display::SetPointerLock((doPointerLock = false));
		if (!doHideCursor && ZL_Input::Held()) doHideCursor = true; // start hiding cursor from the first click
		if (doHideCursor && lastHiddenCursor != ((showOSD ? osd_rec : core_rec) + 10).Contains(ZL_Input::Pointer()))
		{
			SDL_ShowCursor((int)lastHiddenCursor);
			lastHiddenCursor ^= true;
		}
	}

	if (!ThrottlePaused)
	{
		retro_run();
		if (ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING) ThrottlePaused = true;
		if (ThrottleMode == RETRO_THROTTLE_FAST_FORWARD && (av.timing.fps * FastRate) >= FAST_FPS_LIMIT)
			for (int repeats = (int)FastRate; --repeats;)
				retro_run();
		if (ThrottleMode == RETRO_THROTTLE_FAST_FORWARD && !FastRate)
			for (retro_time_t rt = dbp_cpu_features_get_time_usec(), rtMax = rt + ((retro_time_t)1200000 / (retro_time_t)av.timing.fps); rt < rtMax; rt = dbp_cpu_features_get_time_usec())
				retro_run();
	}

	if (DoSave) RunSave();
	if (DoLoad) RunLoad();
	if (DoApplyInterfaceOptions) ApplyInterfaceOptions();
	if (DoApplyGeometry) ApplyGeometry();

	extern void ZL_GL_ResetFrameBuffer();
	ZL_GL_ResetFrameBuffer();

	ZL_Display::ClearFill();

	float win_w = ZLWIDTH, win_h = ZLHEIGHT, win_ar = win_w / win_h;
	{
		const float VerticesBox[] = { core_rec.left,core_rec.high , core_rec.right,core_rec.high , core_rec.left,core_rec.low , core_rec.right,core_rec.low };
		const float u = srfCore.GetScaleW(), v = srfCore.GetScaleH(), TexCoordBox[] = { 0,v , u,v , 0,0 , u,0 };
		if (DrawCoreShader) shdrCore.Activate();
		srfCore.DrawBox(VerticesBox, TexCoordBox, ZLWHITE);
		if (DrawCoreShader) shdrCore.Deactivate();
	}

	static float osdf;
	static bool pastFirstFrame;
	if (!pastFirstFrame) { osdf = (showOSD ? 1.0f : 0.0f); pastFirstFrame = true; }
	osdf = (showOSD ? ZL_Math::Min(osdf + ZLELAPSEDF(3), 1.0f) : ZL_Math::Max(osdf - ZLELAPSEDF(3), 0.0f));
	if (osdf)
	{
		ZL_Rectf fill1, fill2;
		float u = srfOSD.GetScaleW(), v = srfOSD.GetScaleH(), osd_w = srfOSD.GetWidth() * u, osd_h = srfOSD.GetHeight() * v, osd_ar = osd_w / osd_h, TexCoordBox[] = { 0,0 , u,0 , 0,v , u,v };
		if (win_ar > osd_ar) { fill1 = ZL_Rectf(0, 0, osd_rec.left, win_h); fill2 = ZL_Rectf(osd_rec.right-1, 0, win_w, win_h); }
		else                 { fill1 = ZL_Rectf(0, 0, win_w, osd_rec.low);  fill2 = ZL_Rectf(0, osd_rec.high-1, win_w, win_h);  }
		const float VerticesBox[]   = { osd_rec.left,osd_rec.high , osd_rec.right,osd_rec.high , osd_rec.left,osd_rec.low , osd_rec.right,osd_rec.low };
		ZL_Color osdcol = ZLALPHA(ZL_Easing::InOutQuad(osdf));
		shdrOSD.Activate();
		srfOSD.DrawBox(VerticesBox,   TexCoordBox, osdcol);
		shdrOSD.Deactivate();
		ZL_Display::FillRect(fill1, colOSDBG * osdcol);
		ZL_Display::FillRect(fill2, colOSDBG * osdcol);
	}

	if (ThrottleMode != RETRO_THROTTLE_NONE)
	{
		float x = 10, y = ZLFROMH(60); ZL_Color colbg = ZLLUMA(.2, .5), colfg = ZLLUMA(.8, .5);
		ZL_Display::FillRect(x, y, x+50, y+50, colbg);
		if (ThrottleMode == RETRO_THROTTLE_FAST_FORWARD)   ZL_Display::FillTriangle(x+ 5,y+5 , x+ 5,y+45 , x+25,y+25, colfg);
		if (ThrottleMode == RETRO_THROTTLE_FAST_FORWARD)   ZL_Display::FillTriangle(x+25,y+5 , x+25,y+45 , x+45,y+25, colfg);
		if (ThrottleMode == RETRO_THROTTLE_SLOW_MOTION)    ZL_Display::FillTriangle(x+ 5,y+5 , x+ 5,y+45 , x+25,y+25, colfg);
		if (ThrottleMode == RETRO_THROTTLE_SLOW_MOTION)    ZL_Display::FillRect(    x+30,y+5 , x+35,y+45 , colfg);
		if (ThrottleMode == RETRO_THROTTLE_SLOW_MOTION)    ZL_Display::FillRect(    x+40,y+5 , x+45,y+45 , colfg);
		if (ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING) ZL_Display::FillRect(    x+10,y+5 , x+20,y+45 , colfg);
		if (ThrottleMode == RETRO_THROTTLE_FRAME_STEPPING) ZL_Display::FillRect(    x+30,y+5 , x+40,y+45 , colfg);
	}

	if (txtOSD.GetWidth(1) && ZLSINCE(txtOSDTick) < 1500)
	{
		float x = ZLFROMW(8), y = ZLFROMH(25);
		for (int ofs = 0; ofs != 9; ofs++) txtOSD.Draw(x + 2*(ofs%3), y + 2*(ofs/3), ZLBLACK);
		txtOSD.Draw(x + 2, y + 2);
	}

	for (size_t i = vecNotify.size(); i--;)
	{
		SNotify& n = vecNotify[i];
		if (!n.ticks) n.ticks = ZLTICKS; // fix notifications fired during startup
		int since = ZLSINCE(n.ticks), duration = (int)n.duration;
		if (since >= duration + 500) { vecNotify.erase(vecNotify.begin() + i); continue; }

		if (!n.y)
		{
			n.y = 10;
			restart: for (SNotify& nn : vecNotify)
				if (&n != &nn && nn.y && n.y > (nn.y - 5) && n.y < nn.y + nn.txt.GetHeight() + 30)
					{ n.y = nn.y + nn.txt.GetHeight() + 30; goto restart; }
		}
		float y = n.y, w = n.txt.GetWidth() + 20, h = n.txt.GetHeight() + 20;
		float x = -w * (since < 100 ? ZL_Easing::OutCubic(1.f - since / 100.f) : (since < duration ? 0 : ZL_Easing::InCubic((since - duration) / 500.f)));
		ZL_Color colbg = ZLLUM(.2);
		ZL_Display::FillRect(x, y +  0, x + w,      y + h -  0,     colbg);
		ZL_Display::FillRect(x, y + 10, x + w + 10, y + h - 10,     colbg);
		ZL_Display::FillCircle(         x + w,      y     + 10, 10, colbg);
		ZL_Display::FillCircle(         x + w,      y + h - 10, 10, colbg);

		ZL_Color col = (n.level == RETRO_LOG_ERROR ? ZLRGB(1,.1,.1) : (n.level == RETRO_LOG_WARN ? ZLRGB(1,.9,0) : ZLRGB(.3,.8,1)));
		n.txt.Draw(x + 10 + 3, y + 17 - 3, ZLBLACK);
		n.txt.Draw(x + 10 + 0, y + 17 + 0, col);
	}

	#if defined(ZILLALOG)
	extern Bit32u DBP_MIXER_DoneSamplesCount();
	const float dbgy = ZLFROMH(30);
	ZL_Display::FillRect(ZLFROMW(428), dbgy, ZLFROMW(4), dbgy - 88, ZLLUMA(0, .5));
	fntOSD.Draw(ZLFROMW(420), dbgy - 24, ZL_String::format("FPS: %u - Video: %.0f x %.0f\nTexture: %d x %d - Viewport: %.0f x %.0f", ZL_Application::FPS, 
		srfCore.GetWidth() * srfCore.GetScaleW(), srfCore.GetHeight() * srfCore.GetScaleH(),
		srfCore.GetWidth(), srfCore.GetHeight(), ZLWIDTH, ZLHEIGHT),                          ZLLUMA(1, .75), ZL_Origin::TopLeft);
	fntOSD.Draw(ZLFROMW(420), dbgy - 78, "[AUDIO] Samples:           - Stretch:",                    ZLLUMA(1, .75), ZL_Origin::TopLeft);
	fntOSD.Draw(ZLFROMW(220), dbgy - 78, ZL_String::format("%d", (int)DBP_MIXER_DoneSamplesCount()), ZLLUMA(1, .75), ZL_Origin::TopRight);
	fntOSD.Draw(ZLFROMW( 20), dbgy - 78, ZL_String::format("%5.3f", ui_last_audio_stretch),          ZLLUMA(1, .75), ZL_Origin::TopRight);
	if (ui_last_audio_stretch) ui_last_audio_stretch = ZL_Math::Lerp(ui_last_audio_stretch, 1.0f, 0.1f);
	#endif

	static bool introdone;
	if (!introdone) introdone = DrawIntro();
}

static void OnLoad(int argc, char *argv[])
{
	// Load early so the core can send RETRO_ENVIRONMENT_SET_MESSAGE_EXT
	enum { typomodernottfzlib_size = 8699, typomodernottf_size = 19568 };
	extern const unsigned char* typomodernottfzlib;
	std::vector<unsigned char> typomodernottf;
	ZL_Compression::Decompress(typomodernottfzlib, (size_t)typomodernottfzlib_size, typomodernottf, (size_t)typomodernottf_size);
	fntOSD = ZL_Font(ZL_File(&typomodernottf[0], typomodernottfzlib_size), 25);
	txtOSD = ZL_TextBuffer(fntOSD).SetDrawOrigin(ZL_Origin::TopRight);

	retro_system_info sys;
	retro_get_system_info(&sys); // #1
	retro_set_environment(retro_environment_cb); //#2
	retro_init(); //#3

	retro_game_info game = {0};
	if (argc > 1) game.path = argv[1];
	retro_load_game(&game); //#4
	for (int i = 2; i < argc; i++)
		DBPS_AddDisc(argv[i]);

	retro_set_input_poll(retro_input_poll_cb);
	retro_set_input_state(retro_input_state_cb);
	retro_set_video_refresh(retro_video_refresh_cb);

	retro_get_system_av_info(&av); // #5

	for (int i = 0; i != 4; i++)
		retro_set_controller_port_device(i, RETRO_DEVICE_JOYPAD);

	ZL_Application::SetFpsLimit((float)av.timing.fps);
	srfCore = ZL_Surface(av.geometry.max_width, av.geometry.max_height);
	srfOSD = ZL_Surface(DBPS_OSD_WIDTH, DBPS_OSD_HEIGHT, true);
	shdrOSD = ZL_Shader(ZL_SHADER_SOURCE_HEADER(ZL_GLES_PRECISION_LOW) "uniform sampler2D u_texture; varying vec4 v_color; varying vec2 v_texcoord; void main() { gl_FragColor = v_color * texture2D(u_texture, v_texcoord).bgra; }");
	DoApplyGeometry = DoApplyInterfaceOptions = true;
}

static struct sDOSBoxPure : public ZL_Application
{
	sDOSBoxPure() : ZL_Application(70) { }

	virtual void Load(int argc, char *argv[])
	{
		ZL_String basePath;
		if (FILE* curdirf = fopen_wrap("DOSBoxPure.cfg", "rb")) // If cfg file exists in current working directory, use that
		{
			fclose(curdirf);
			Cross::MakePathAbsolute(basePath.assign("DOSBoxPure")); // relative to current working directory
		}
		else
		{
			bool useHome = false;
			#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
			// On Windows we use app data only if user has created a DOSBoxPure directory manually
			wchar_t appDataBuf[300];
			if (GetEnvironmentVariableW(L"APPDATA", appDataBuf, 300) < 300)
			{
				extern char* utf16_to_utf8_string_alloc(const wchar_t*);
				char* appDataU8 = utf16_to_utf8_string_alloc(appDataBuf);
				basePath.assign(appDataU8).append("\\DOSBoxPure");
				free(appDataU8);
				if (libretro_vfs_implementation_dir *d = retro_vfs_opendir_impl(basePath.c_str(), true)) { useHome = true; retro_vfs_closedir_impl(d); }
			}
			#else
			// On Linux/macOS we default to home directory
			if (const char* homeEnvU8 = getenv("HOME"))
			{
				#if defined(__APPLE__)
				basePath.assign(homeEnvU8).append("/Library/Application Support/DOSBoxPure");
				#else
				basePath.assign(homeEnvU8).append("/.config/DOSBoxPure");
				#endif
				useHome = (retro_vfs_mkdir_impl(basePath.c_str()) != -1);
			}
			#endif

			if (useHome)
			{
				// Use home subdirectory only if DOSBoxPure.cfg can be written there
				FILE* f = fopen_wrap(basePath.append(1, CROSS_FILESPLIT).append("DOSBoxPure.cfg").c_str(), "ab+");
				useHome = (f != NULL);
				if (useHome) { fclose(f); basePath.resize(basePath.length() - 4); } // success
			}
			if (!useHome)
			{
				// Otherwise default to directory of EXE (on Windows) or current working directory (on Linux/macOS)
				#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
				Cross::MakePathAbsolute(Cross::NormalizePath(basePath.assign(argc ? argv[0] : "1").append("/../DOSBoxPure")));
				#else
				Cross::MakePathAbsolute(basePath.assign("DOSBoxPure")); // relative to current working directory
				#endif
			}
		}

		ZL_String customPathSaves = ZL_Application::SettingsGet("path_saves"), customPathSystem = ZL_Application::SettingsGet("path_system");
		for (bool useCustomPathSaves = !customPathSaves.empty(), useCustomPathSystem = !customPathSystem.empty();;)
		{
			if (useCustomPathSaves) Cross::NormalizePath(Cross::MakePathAbsolute(PathSaves.assign(customPathSaves)));
			else PathSaves.assign(basePath.c_str(), basePath.length() - 10).append("saves"); // home path is absolute

			if (useCustomPathSystem) Cross::NormalizePath(Cross::MakePathAbsolute(PathSystem.assign(customPathSystem)));
			else PathSystem.assign(basePath.c_str(), basePath.length() - 10).append("system");

			bool invalidPathSaves = (retro_vfs_mkdir_impl(PathSaves.c_str()) == -1), invalidPathSystem = (retro_vfs_mkdir_impl(PathSystem.c_str()) == -1);
			if ((!invalidPathSaves || !useCustomPathSaves) && (!invalidPathSystem || !useCustomPathSystem)) break;
			useCustomPathSaves &= !invalidPathSaves;
			useCustomPathSystem &= !invalidPathSystem;
		}

		ZL_Application::SettingsInit(basePath.c_str());
		bool screen_fullscreen = (((*ZL_Application::SettingsGet("screen_fullscreen").c_str())|0x20) == 't'); // 't'rue
		bool screen_maximized = (((*ZL_Application::SettingsGet("screen_maximized").c_str())|0x20) == 't'); // 't'rue
		int screen_width = atoi(ZL_Application::SettingsGet("screen_width").c_str());
		int screen_height = atoi(ZL_Application::SettingsGet("screen_height").c_str());

		if (!ZL_Display::Init("DOSBox Pure", (screen_width < 80 ? 1280 : screen_width), (screen_height < 60 ? 720 : screen_height), ZL_DISPLAY_RESIZABLE | ZL_DISPLAY_MINIMIZEDAUDIO | ZL_DISPLAY_PREVENTALTENTER | ZL_DISPLAY_PREVENTALTF4 | (screen_fullscreen ? ZL_DISPLAY_FULLSCREEN : 0) | (screen_maximized ? ZL_DISPLAY_MAXIMIZED : 0))) return;
		ZL_Display::ClearFill(ZL_Color::White);
		ZL_Display::SetAA(true);
		ZL_Input::Init();

		ZL_Display::sigKeyDown.connect(OnKeyDown);
		ZL_Display::sigKeyUp.connect(OnKeyUp);
		ZL_Display::sigDropFile.connect(OnDropFile);
		ZL_Display::sigJoyDeviceChange.connect(OnJoyDeviceChange);
		ZL_Display::sigResized.connect(OnResized);
		ZL_Joystick::Init();
		RefreshJoysticks();

		OnLoad(argc, argv);

		if (ZL_Application::SettingsHas("interface_contentpath"))
			DBPS_BrowsePath.assign(ZL_Application::SettingsGet("interface_contentpath"));

		DefaultPointerLock = PointerLock = ((ZL_Application::SettingsGet("interface_lockmouse").c_str()[0]|0x20) == 't');
		AudioLatency = (ZL_Application::SettingsHas("interface_audiolatency") ? ZL_Math::Max(atoi(ZL_Application::SettingsGet("interface_audiolatency").c_str()), 5) : 25);
		ZL_Audio::Init(AudioLatency * 44100 / 1000);
		ZL_Audio::HookAudioMix(AudioMix);
	}

	virtual void AfterFrame()
	{
		OnDraw();
	}

	virtual void OnQuit()
	{
		SynchronizeSettings(true);
		retro_unload_game();
	}

} DOSBoxPure;

const unsigned char* typomodernottfzlib = (const unsigned char*)"x\1l\221\3\260\34K\24\206\277\356\231\331\235\330\266m\333\266\235\254\256\315\330\266m\333\266\223\302\263Ux\266\315y\323\267\243M\355\277\365\357A\37\17\2\310\307,\f\6v\357>\264_\324\357\367\252@\337\241@\331\1C\32\64\336\177\263Ia\20#\1_0\311\237\272\364\364\230e`\236\2\261(\306\237\221Jw\32B\337\256@\301\230\304\311\321\237\36\372y\36x/A\333\212\261Q\376\220\363\307\233*\367\3\240y\254\353(0\314\23\3\242(P56)s\322/q\265\276\7q\t\344\37\tQ\351\311T\376\67\n\272\274\3\326\310\304\224\240?\177\340\277m\340i\tbF\222\177R*w\351\2\302\265\251\230\354O\212\232\261\352\245L\350\325\20\344\217\251)\31\231Nm6@\337\252\352\35\1\210\263MR\3I\367'\24h\373+yl\24^\314\377u\301\307\322)\364_\252\367\260\275\t\201\215DC\200\275\351\277T\260q\n9-\275\207\21\204\343\6B\375\323\5\223\310\60\214_\345\65,lk\213\325\4D\31-\215W\211\226\205\355\2RX\272\21\263\340\344@|\177\3\25\1z\366\255X\21\365s\254I\356\f/\332\233\304*_yJ\255\372\25\214/\254Iz3\33\r%\345H\20\253\b\311$\332Y\313ig\336\245\235\347\212\253\327\246\235\350B;\345wY\313\330B;Eq\227\356\362\25%\335\234W\234w\315\240\233\343\23\321\246\217\266J\217L\35o\374\253k(\351\306\267\267~\326}T\317'\364\241\251\364\207n\336*\235\247t3\250g\324\71\332\367$\347\261\355\323\61\336\217Q\266\216\365\351=\324\274J\252\31\314\276\220\243\377\33\201-\320r\213\356/::\205\214\267\335\374\334\272\266\253\207\305\313\327\321{5\247\266\222fP\337P\327\320T\271.w\273T\376'{Y\265h\223s\213\332\372]\305\272\366.u\33=\237\316W\266\232\371\271z:f\213\236Kzh\355\372Rt\36j\206\335\217\371\270G$z\347\252\373E\370f\253\234\226jN\231\333\71\350\352\333\254U\272F\330\367Z\0J\32[\"\327P\361J\352[E\246\347Jd\377\363\375]_\23}\373\b\334\362X\206\61\314\17\30h\226E\240\60\7\3\201\262M\242\201\334\330H$\255\231\301\5\34\307\1*\322\226]\\r\215\217\235\217\300\271\201\316\365\240`\340'@\220\20QD\23C,q\304\223@\"\5I\"\231\24RI#\235\f2\311\2\341q\211\206\220\200$\34\2\f\323\362x\355\\\271\363\344\315\227\277@"
	"\301B\205\213\24-V\274D\311R\245\313\224-G\371\n\25+U\256R\265Z\365\32\65k\325\256S\267^\375\6\r\33\65n\322\264Y\363\26-[\265n\323\266]\373\16\35;u\356\322\265[\367\36={\365\356\323\267_\377\1\3\7\r\36\62t\330\360\21#G\215\36\63v\334\370\t>\346\314\235\277p\345\272-;w\354\332\263{\357\376\203\7\16\35>z\344\330\361\223\247O\235\71w\366\322\305\313WH\t\4c\310\332\236\24GF,\363\66\220\nQ\231(\304Ob\337\205\351\376D\200\204\311\30\63f\257\5\215\363\\\347\31\314Z4s\361\202\245\313\226/Y\275\206U\233\66o\4\222\201l\302P\222\206\304rB\344\25\335\305p\221,\26\210c\342\236xC|$\276\23\377\311\206\262\243L\227\307\344=\371\223Q\327\350h,0\356\30o\31\337\230\230\305\315\336\346*\363\230\371?A\360\f@U\24\0\0\364\206k>\363\277\354\226l\267ds\311\306\232\261ec\311\266m\333\266m\267\304s^B\37\326\206\355\341@\270\24\356\205\247\341K\224\37\271\250<j\214F\242\245\350.&\270!\356\210\207\342\251x#\276\210_\22@\\R\224T%MIg\322\237\314'\227)\240\25i_:\236\236\244o\351_\226\262\272\254#\233\316\66\263\347\334\347\215yW>\227o\347\327\371[ADq\321Z\364\25\263\305RqQ\274\225yeu\331_\256\225\327\345kUTUV3\325Q\365R\375\324Mug=V/\326G\365{\375\333d\246\246ioz\233\231f\267yl\371V{k\262\265\325\272m\3\273\274\335\331\236k\37v\200S\330i\350\364w\246;+\235\375\316M\347\243[\330m\351NvO{\310k\352M\364Vz\357}\337\257\353\17\366\347\373\373\375\257A\311\240g\260\70x\34\26\17{\206;#\25\65\217&Ggc\26\67\216\273\307C\343\231\361\372\370cR4\351\232\314O\356\247E\323\236\351\350\364p\372\66\347\346\352\347\6\347\226\347ng\6\0\220\27\364\4 OYT\27@@\1\370\317\272\227@7u\235\t\277\357-\22\16nA\310\262\rd\261x\261\25\7eAB\270\305YhN\226.pJ\3m\374\227\343t\323t\240[\224\tf\334\216\273s&\335\353\241E\263d%\vuwgO\246>\35\222n7\vY\235.\304\63C\326CM\366\270\v\262\336e\276\357\273\367IW\212\204\311\177J\254\247\247\247\253o\273\337\376\335@\314\361\235X<\341\347 }\374\67\216\27\"\262\246\274_\26`\324\351*}E\bZ\177\246;n\257\340\365\363q}\"\352\247rI\276\202\227\311g`\6/\221\65B\377\243\365\30\203\337\203\361f\251"
	"u\2\256\217's\311\276l\224^\331\204\357$\23\311\250\337G/\304\350$\343\260\354d\371\n,Y$\26\301(^\366={2,\220\317\341\235,\320G\370\335\311\60\212\177\366@\261\204\377\212\301\230z\267\7\202\61{@\331\314\231\207\337\342~\25\351\243h\231\315\264'\374\225=~rY$\321\326\236M\342\307\266\310r@\366\374e=\271,?Z\225\243\5\370q\345\252~\310e\355\337dvO\213\241\rrr\303\320\320\6H\357\236\236\336\235\21\364\214>\321\63\71\251\236y\303\323\273\351\363\206\241\351\335\370\265\250\371D\264\270\304{\344\263\310\373\2\344\375\64\353\34\244hY\344\70hk\357\207\314\252\63\1\361z\204\64\323\216\317\220,\244\241\376\373d\345+\374\270*\233\4\37\334\27z\373\373{\363t\221\205\361\221\374\310\70_\202\261\312c8v\241~\356\34(\7\317\332;a\206\277\341\v=\327_n\257>=-\27>/}\205\262\62\265o\356\vH\373<\253\233\351f\1\306\274\225}=\v\211f\26\354\362\360y\302\217\241\ba/\313l\264\274\377\272\302\364n\301\62\23\364\310~Q\4c\273\247\355\1\222\17\214\212\373\206\267\302\333\247C\201\341w\323z\357\334q\360X\267,\250\352\224R'\265\267W\332\347ySV\224>,\4\37_\366yr\17\254\231\222\223\220\366\246\344\256\362?\313\177\207w8\267\303\333hI/\256_Q\273\36<H\313\311)X#\367\340\372\377*\277S\336\1\177\357|\6>\300\360\355\tg\20y\356\300\17\271l\342l$\"\321\221\70\215\24f\1\344\372r\251\\\7l\21yQ,\342\205\337\334\317\225J\371L1\303\257\352-\301\202\264\63\350\256e+A5\217\241\204r\311X\326^A\370\5\321+\234.\365\16i\\\177>F\270\213\255\22\305\244\356\230\27\207\213\345\201\343K\362\272<\334\243ec\17\330;\235.\372\36\22\311\234\275\63\30\303\337\v\376\216l8\374-\331\356\305d\25\26\34\376\3\362\177\b\371\307\347Q\310B\312~\327\343\301m\371\310\232C{\254\312\36wk\375\264hOcZ\17c\250l\240\367\67\246\364\21\61\222\336\v\272\300(o,_\310\f\202\61zj\17\354\236\226\5z\34\214\321\225\351\6\214\245\366\237\21\7\355\251\217\22\360\355S\5\252\200;.J_!\332\317\320\64$\225\226\371l\21l\247d\f$:\347,\350\7e\247\244aB\23\201\270\235\256r'\0\335\":w|h\3>G\353\33\272\360^D\201\357\250\\Jv\32G/~\320\240\331\374\65\330\345\270\331"
	"\352\306\240`\241\363\345\353j\224X\24\351Zd\2\212\250\314\217\16\337\267\273\252\303ta\374\362N\313\304\251\274j\226\266\37\262\216\63(Dy?\234\372\360+\366\0\271T\30E\23\335o\256\247\235H\22u\206\27 \363\"\25J*O\201R\250R%\v\202@\300(\321\242\250\311o\30\312\263jM\357\316\357\236\66\367\271\323:\211\241\363\376\"D\22\60\61\253\301\206(\265 \b\r\356\365\214l\335\60\364\377.a\231\313Bu\343y\303\211\216\225+YN0\252\277\30\272\320\344'B\372\n>$sn\267\274\364\61\370v\371\240\67,f\327\243\2p\316\24\256\213\243\6\234f\356O\305\323,\364\214\335ikw\24\231\372A\270K\327\25\230[\331\232\327\"\30\372^\225\324p\257\330\357\320\26\365\320v\321K\351+\276\254z9\205\366\340\243\331\222P\252^X\313\7ef\32\206l\205\31\222S\23\3\61%%\v\206\244\302\30|\261s\300r\330S\241\5\307C+v\16\224J\301\242R)\324)\341\334\355\216\223L\343\220hA\271\236\351l\17.\267?]\36q\272`t\237|N>\267\17F\25/p\217\63\350\334\36\302L\346\350\345\f\226\367;]\370\272\275X\224\327\25\213\265\60[ \207\177\340\334]\36\261?\35\\\356l\267\7da\37,\201%\373d\201`\236\205\362\351A\377\354X\335\312\323\70\370\252\206\60\237\204B\17\262U-\262\213B(\213\35\37\351\355\27ta\326\205@\275\315\347\331\220G\306\373{\205\272\322N\330\24\303\275\333\274)\324\210\345\226E\"\367zRh\6\312\31e\23\313,T`\vc\267\225\210%\303\250\311\341\t\366\242\354\277\266\360\234\201\21Y\240\200\366\220|\2\272\37\202\321\207\240\233\266@\251(\214\332\23\273\247S\357=g`|$8wd\\\340\22{\2\227\310'\36*\217T6\317>\275&\16\332X\21X\340$=\307\207,\370\216\275\363\26\370\250\334\370\215\307`F\300\314c\337p\337\37H!\v\336\60Y\265\326\353\257\262o]l-\263,\22Oh\301\321\366\216v\240\17,\245\270NAb\356W\177\362\271\362~{b\307\3\205\216v\324L\324\250{6\f]\324\275\374\255h\346\255\220\376\334O\274\341\7vd\256\333\32k\37\262?C\246\230=\353\242r\347[\227\337\201\213I?4N\366!\335\b5K8Ck7R!\22\242}\210\35\231Jf m\344<x\243\274\71]Co\256\364\64\204\337b\305\311\253y\210#\206\60O\201.k1\264Y~7x0z\351\317\36\272\233\214\1\332\340\\\371\360/\203\61wWy\325\202\237]\n\351\273"
	"\37\272\265MN\310\347\345\344/\345\303m\206\217\300\30\31'\7\247_g\222G\243\234\216\374\273\67\214R\245?\323\247\32\353\375\312j\241\226\6c5\262\360\361C\35\357,x\212\306\251\252|\354C5i\37y\6{\340\232\315_U\2\"\33\256\6\266\274\b\316]\277^\211\310\304\305|dcIv\367D\231\260'\4\256E\302(?-}\205\250\f\343u\32\327;\274\336\317\vAjS\211\33\316\335\370]\254&\16\344\20f\325\363\347y\357\24I\310}'\221a\322\321\312p\263\220l\201$\370D\310\31\366\233\240%\270\315\376\254\374K\360\332\247\64A\301\207\202\224\"\311\220\255\307\277\245]\20J\374\263\353)\367\322y<\303\6d\17|\272f[\300{\217\220\267L\313[\304\37a\27\354\">\334\345\301\343\366\362\331\307\21\311;\355\333U\336v\230m\350\30\202\235\240p\30\305_\23\2\n\207H\r\314\356sSh8\356)\263\223\206\236-\b59f\354^w\235V\223&\207\33gO\324\251\264=\20n\233\250*u\235.',\253\22ic\f\35\71\63 \267:\333\311\275\v\362\32\366\4K\37?Q\254!\265 X\\sp\f\363\265\245\243\257\365\r\32\241\216\7\367\253\344f\202}\305\220TY\250\62A\261\\\210\220\330\362~\203\v\303\16/b\332\27\327\322\36\215D\343\276S\241\177\247\312\20\234\256\241e=/\355{\\\16\n\202i\360\60\264\254\63x\4f4'\246\16\221\347M6*\222\360S\203\242\212\64\323,\221\64_\365\225\224Y#\231\367\6^\212V\311\34[6\34\226g\271k\345Y\224,b\306P\233W\305Q\227\310(\f\363\340\b-Tx\321)\tYG\325J\246\r=\216 \177\200\372\233\5{\205l\235\204\333\341\366I\222\303\354[\\a\330b\364\373J\347\311\222\262q\\\35G\263r^\223\237\227\177\225\217=\360\32\214>\b\243\257= \37\223\177uW\317\376\32\177\376]\367\23\364B\20\65~\1\21\361\237\217\177\360\242|\367A\350\207\376\203r=\214\37\224\277\222\277:hO\330\23\322\6\31\234\33\234\vR\332\204\337\250\301\254\70\375\234L\356\314\347\341\23\360\211\347e+\31\361\177\332\347\227\367\353\332\327\364\247Z\206~\7\220&:+\203\26\347\200\3A\253\375\347r'\t\23\312\213\205\320\277\251\326H\361\320\377\302\f\325%(\215)\24\273Zg\326\22\36\t\355\304\340\266\307\355wq1Q\17\307#\337\212/\22,U;0\223\27\234\356\231\65\26\345\205*\224\302\336;\376\0\313\357\203\345\177p\277(\373\355\211"
	"\340\\Zg\241M\255=\374 \343C\36\334\265\263\267f\212E\r\303\331\216\270\332\350;\222\n\274M\226_\2\317\331\16\243\262\240\354\361y\347n\314\247\26R/\274\273R\322\240\246t\344\262\61\250(\215N!\347m\32\21\224-\210\255\33\321[c\26\322{\315\65x\351\337\264i|\4FG\306\327\255+\273\366\303\262\300\305\365(^+8\310G\21\16\210\325\26K\260,\25\215\365\305\b\243J\"\355CU\260rrd\323]b\343V\306\ti\206\330\313\320e\260\351\22\357\375\31\351\257\33\347\4F\357)\363rB\303\70V\23\343\355\235\230\314 \250I&4\315\351\26\"\343F\300\370H\5K/=!\276,\203\217F\262B\16\336\270\254\340\207\231\322\326:i1\36\315G\202*\275\356d\302t\341\304\3e\366\211\212\247\211\300\26rKJBt\225\205\376~\206\306\354\tA\330\350\371\310\70\266/P\301\372{\31\23\313\313>\244\374\3d\23\224\222\352\362\333\317e\321}\331+\4R\313\65\70\2\361\206Q^yAI\261\300\346\7\323)\vHg\331Z@5@=\373(\224>\243&0\212e\257*\nH\343\376\"\305\230\3\365\366\353m\266'\f\201\200\263\351\222\331ky\257nZ\27n\205\246\235m\243M\305\330P\247\374P\225\234A\"\267\367\32\201\332\203\344\222\251\330\23\4\66\70\327;\237\325\206em\344\27Q\206\244;z\224s\227\313.\332\311\f\367\362pj5\t\36\257[\240\352\216\305@\354\251z\30<\222\62I\v\245\204.\321\331>\\v\211\350*\255\337\326r\326\264F\23D)9J$\365\333H\352\235\273\205\200/B\213\274\375\370\221q\222\71)\345\275\277AZ\313W\301c\330\276Rp`\257\316\235\360\327$\357\204\217~\234\70\315\323\257\334%\231\331[I\227\65^\362\337\270?=\270\236\265\264\275cU\237\66\64Cd\306\335\237\67\215dF\6\332\26$\332\6\360\206\244\236A\21\252\253\363\315M\3m\355\355mT\nhYV\337\21\213\346\23\357(\336\30h\f\340E\206YT\320L8N\331\320{\325c\211\325&/N\254\326\234\235\273C\213\205t\235-\223\6\205\206;\36\314\30^\303\262L\275e;\326\224\252\230\230B2\353\35\324\237\r3\266'\252\272\313\34\20\242u\322\367\276\21\234n\242\61m\243\253\211m\370\261#\330\204hf\f\245\255\360\337UCP6\274\23\361\314W\234$\374\212\254\205Pb\26\65\373\63\210\362Ma<\367\227\245ras\200\311P~,\21\266\16tg\311^\221\27\370\217\233\237L\20\265\362\252\37\217\27\307"
	"\223\67!\337b\334\22]'#]%\367\227(c\253\33}\v\361\351C\2\335\v\212\302\316P\t;\5\67\220\236+\7\65\3\67L\t\331\332\313~\211\374,\321\31\257zY3{\261o\330D\355Wm\337\344M\311\272\211${\242\354\206\261\335\71`\344-p\205\274n\22\272\240kR^\207\371\315\217\234\215\345Nm#\324w\306\265\255\210\213\362\225\371d\223K!\353\376\354\225G\344$\272\276{\345o\345c\217\274\362\312\276_9~\271\323\276\"\370;\4\360\277N\322\210\67*gY\312\71\313|\360\341ry\373\313\217>\372\262\274\35\336I\357\310\334\214\274\7\316\222\255\262\25\257\367(\274\262\300<\36[\237\233\325\267Q4\233\244j\244\4\354\315 ]\345x\366\326\320;\206\374\60\334\26\235\327pVco\b\246\235\256\340\240\235`\367-\260\0\24*\17A\177\62\305\64\240\22&X\347\243\355\272s\320\303ix\16\251\332E\21\361;\337\241hH[\354\216k{\333\272q\343V\262:$\216\314\16\237)\231\356B\270[(\377\321\365\317\26\341M\35\332\203\315\220Z\234\300h\bg\204\320 \272>\215\223\312\200-\204\252\212\330\331\316\b\310\342\5Y9\341\326q;\354\371\342^l'\357\254;\223YO\307|\330R%\221t\205\300\345\351\302\376\37{\311\344\377y\246\323\21\213g\343\61\7\373\331y\36\351`\325EyRy?\271\365\312\314\204{@)\212\231\270\60\333\70\321\320\223\23\266n\330\202S\221\327\247\33\374p\334\71 Dm\302!\204\31\347\320 \334\203\252O\1\312\21r\337H#\324Yh.K!\33\34\365C\241`QR\340taR*\204\273\226M7\257r\2\252\374\251\257l\251>\225p{\334q\324\341\345\225\331\5;\205\263!\233@]h\213v$|,\376\61K\363s}\324jL\345\262\35\330\266\262\63\272\35xsJ\374\362\203\37\374\245H}\352_\377\365S\306\275\263=\354\34n6\277\64\356\315\274\275=\314\373Ia\23\341\24\200\376\303\350\367\61\235\310\177\f2d\5H\277z\263_\264\277F\211}\260\255X,R\310\304\67\313\66\364/\312\326\265\330\366c\273p3/\304\325\333\344d\260\315\376\32\361N\272\350\f\342\272EV7a\217\1\71Ac^\3u\355o\\\1\236=P\336\217\301\200\304L\336\332\36\60>P\33\306\35\227\353\344_\354s\207I\344(\360\331\227\364M/\334\2-\301\4\342\275\26+\214=\356S\325\36f\22_\360\37%ls\273O\361<\302\341\330}\33\357\373i\326\331:\237\255k\246f\215qW\312P?2!\b\233~m\26j\212"
	"\205\335\314\220F\276\b&\32u\360\306\203\7o\204\64]\313?\201s\236\201\364\63p\216\374\371\63r\362\31\371sw\255\242[(\275\344\237\24\17\336('\303\37\311\202\261\32\177\315\62u\16\360\314\363M\244\261\232Z\335\343<\33b\251\323\243\21_\223\234\351\70\35\372\22\307\35\227\300\327\277l\334\330\222\333\274y\307\216\315\233s\336\360q\211<=\307\313\266\v\203\261\v[\336\261cs~\363\216w\350^m\32\355o\255\25A\fV\213\235\200\34x-@o\366\nU\211\1\6\374\360\316\71@><\17\27\323_\365\226\364\16\277\34D\177\341)?\31\363\365\224\310\236@\335\262\334\212\374[\255\4jG\216\274\25\366\vR~\202\373\5Q\332\212\4e~s\310\335\275h\313\25\317\376\303CB\333\245\340\232\346HBw\272.\37\224/\223\347\341&p?\34<\262\304\271\376\363~\340\256\345^\263\5\272\376\213\254\341\2\320f\277\70\310\71c\7~\346\372A\365&\22\206\255\333\227M\233m\6\356\303\253\371\220\bmX\325%\302\31d_\261\240vRGX\315i\35\244\61\316lW\26\211\367?!Z\270\26\355b\337\337F5:\376$\276\n%\246\235u\246\275-\372\347\337\311\302\363o&\205(\22\312\323\251\365\16\375\213zP\27\220\210\253\373L\30K\325\214\f]TG\310T6d\307\317\245\240\363\tl\257\36\267d5\1*\262\216%>\353\235_\374\327\261\67/\356g\226\224\346\301\206\332\32\231\332\26g\202\367\222,\353\22\331\210\325Dw\267\316\22_\237\220\344N\23\225\214d\241\374\240\67l\244$F/\205$\207z\261\322\352\243Q\25\377g\257xXN\301\211\17Sd&\307\f'>\364\20\234h\363\340\7[9`\255Fow\237%\350\267\35\232A=\366\313]\35\332f\36\221\263]\243\236\21\276KPNW\240\234H\277\271\242\203+\212Eo\n3\356\236b\255\235.\255\267S\217\267\3\37\260T+6*_\331A\26\212/\323B3\364(O\27\253\201}\222a\266@\267z\203\275\24\222HKd!\274s\327\342\334\26\377 \r\351|\365\226`9\344\307#\213\211\7\344<^\341\3\343j\f\343nL%v\207\213E\334\326R1_\304n\352\227\376<\373y\315\343\241=%\270\30\25\61\70\67\234\341\232\360\226X\307W\340\65\322\304\20\264{I\235J\226\356\64\221\224\336W\253\240\362[\204\314\304\305g9z\217\254\253\16\5\275\nW0\322@s\337\305X\335/2\243\r\325x\366TB\255x\346\270\216=\241\36\256W\273\325\204>\216/#`\320V\323\3\277\32P"
	"\340\301\306\363'\352\202\225;\33\17\240\230W\216\341\321\207y\356\363\346\232\311\17\311\261~\372#\377\211[P5C w\234\215\355\215\303\272\236m\365(`\305LX\363\1]y\324\217\327\203\333Yz\371S\227\274T\252\1\27Y#\v?\370\1\214V\340}\202\341-\251\241-L9;t\312\371z:U\314\345k\rx\257\315HG-W\343\230\247\371_T\213E\247\n\257\203~)\205\255Z\260\303\344KBxx\332\226\340\265[\313j\340Y\34\bTS)f\206\203z\370_G\7\23f1\370\252\23\67}G/\30\245o\351\245j\371\350y<\vl#\234Y\304\231\342\220Qy9\37x\366Yy\363KO\333\316\62\347\271\362S\366\373\202\37\70\307\227\227\310\233\61h\27\356\220\37\0\357\37\365\70\v\205\37\372a\356\21\237\252gv\324\303'\342\315C\tG\30\341\r]tY<\314\202\363\rgyC\27\265gV\243\3%\36\352\307z\341\314\33\365\211{@\244\216\236\301\20\334\310j\35\214\231\23\271\310\32\316V\303\261\334\234\60\366\262:\7_8:\30\21\322\17\255\312\246lm\255\307\262`\302\211>\254\24\71\30\253\200\262\34\rk\36\347\242q#\33\65\251\272\202\324+\330V\3l\236\20\212$\r\212zdtf<\315\274E\325\264\216<[^\210\223X2\244.\212\27\335w\263\7\314\265\61\265\226\5`,%\270\337\303\313Gx\355\61j\355R`\236w!\t\212S\372A\310\37\373\244\303W\"-3\232\257\371\32\203\346\r\261\b{\200\347\210\212\r=\257\367.\364\206u<\214u\207cZ\214\20\275\200r\b\7\270\36D\214y-\252&Q\251'\271\223N\227}\256\61\266\265\317E\21}^\317s\27\260\214B\37\302\36\304\234\360\325y\220\352\304\17f\f\327\341}\332\34\377E\256\67}\207\236\243\263\234h\272\306\332\25\257\233\246\201\66\216\260\271\6\277\343\275\311TGj\262\240\fCM\7\325.\224\276b\16\326\234.\372V\35d\232\3o\323I$\34\346\215>\246\331<2\304\333t,i\342\215\240g\353\t-a)\314\301\262\375\33\245\63\245f<\207\212\364\327\277\66\345\332!\2\334\265\270\217|^q\356\311\353\353\2\304D3\316\203m\306~7\33\313\232M\fW\311A\353z\207\225\252Zql\256\335\337K\226\335T\16l\34\207\366\64\337\373\332\263{-l\225\270\373\35}Y\373|\350x\365\352\253_\205\16\371Gz\247>\346\323O\223\265<\375\264\332;}\366\264C\347G\355\35\212,:\340\332\223\322\202\353\243\216\32\323\216P\231\227T\217\363\313\345\335CH\340}k"
	"\177\373!\276\203Uk\177\33\364\300'\246\361\34\312\66\371]|[\35\361\316Y\322=\304d\nIw$5Q\272\303\271\213\271p\316-_\300L\365{V\255O\356\320]\210\6\323\322\212\177\67\246\246ZMg\327\327\317N\217\26f\350\357\337\30\314\b\371\16\355\367\33\201\255\372\177\23n\250\326\207\366\324C\256\217\3K\253\32\324\220j\35\17\f\340ZWJ_\251\3m\372<\222E\253\216y\341\204\326^\241\4pleR\253\243\235\61\256\65\317^&U\327\60i\350\65\220\322\350\222L\333\232w\r\365\330\324\374\353\32*\244\251UI\203 \232\24\300L\245\237\336\253\272\367\330^\223\7\261\247n\236)a\177R\223[\370\71\325\245\64\306&\372\360\343\tx\254.\237'\256;\227\320\65\303B\311\350s\217\252\204v\34]I\227;\235\3\352\350\243\243\346\211h\3\64\207\70\361(\246\257T\234\66\235*^\313\352\331t\16\v\367\353p\372\267\306\233\347\35|Cxi&\222j0\25\251\247C\253\270\71\34\261\35\203\fy\235\322\363\232!I\260\315 %\262Q\351\274\201\337\233b\276O?\32\276\353\234vS\31\24\r\227\335T\22\366i\206\317\66f\342\264\7'\35\215\64\264I6\25G0Fv\331\\\30\336\67\310FC\334h\353$\207\264\365\226\243\220\304\242&\225BS\211|\256\266b8\202z4\250\36x\257\364\\\246\333ZA\367l\365\t\264\363H\345l\r\242\347IIGu0\36\63\246\342N\216\274C8+Dt\330qP\3\v\302G\330\250AHO\302\251\271\61\63g\n\331\211\320K\254[\247\247\347\372\16\322j\210./P\357\252\66\341Y\320\251\64[kR\205$\33\26-\260\67\337p\212)\6V\277\263M\327+Jz5s\202\201\325m\331~]\253(y\221m\341\335b\356f\223\311\316}r\340\62v\32\245#\235\37P\345t\271\263\331\61\202\206\270[`n\334W\260\343\220\377\363\377\217\334\304\35\301\211\322I\225\250\70\67\353\366G\224\357\310\34\t}X\373\227\276\322\204\0W\341\327\347\7\216\263z\315\nj\356s\33\334m\236<\22\5\252\216\237]\337\4\277\256\271X\366\372\374\2\325\\\220\326\65\227>\303\220\16}\260\256\273\214\365\213A\255\347\315\60\227\207u\27\257=F\301\326u\27\22\256\353.\365\3{B\211\251Rwiy\314\327\30B\211\244\251\356\62\316U\b\241h\312j\31.`\371\325\317\371=\235\30\206\16*\25\315\365\245\222\35}x\346\320\64\227\353`\321@x\"\340\262\367?\271\346'\260l\360\351\232#C\316W\313"
	"\372\260\63\313r\343\306\307;\257\177\357\306I\246A\237\321\230\242\n\246\361\351\205\372\f\276\346\64\3\274\323\210\0\257;\332\0\235U\317o\352,\325\16\376\334'\35\330\226\233\237v\220?\342\255n~\350\301\371\61\355\347\337\36o\201u\346\r\342\355\231\33\257\243m\270\71\352\242\322\276\346\250\275\63\215\270\257mtJ\327js\342\257\337\351#\311\340:c\337\233\223\343\306\f\5\250\372\f\265\17\251\271)\n-\350\210\273\301\301\277\71\tC\241\275\71a\315\246m\224gF\372\377\244\340\377\353\201M\223\272\17\64O\245\321*\377F\317\275)k8\211\316t\250\344 \5qU\240\205\5\334r\250\26p\375\200\205\240\265\263\267\277{\236\374\350\3\370n\267dz\321H\237\70\177\362\335+\324\315c\202\23\201\356Hp\257}\1\221\332g\357k\357Uf*\236\244;bE\373\t>\27\302\376\250Cw\31\32\234\16\321'\34/3\317\210\250\60\62\373\303\372\243\"G\rs\v\253\372\273\336\30\314\332:\255\1X{\205\322c\350\67\1W\202\316\322z\320:\207\245y^}\235\326\220\352>R\211\267\231\260U8)=]\17\231\317+i9t\351\b~\244S)Z \301\63\215N\247\350\240=\322\344\214\212>\33e\324sF\246I\365\\u\304\206\32\244\353\71=\270\276\306\236P\271\246:\37\5\351\372Z\316\217\316\343#\241,+\302\23\312\352\304\252\254\216\314\31\v-\330\327\210\61\35\214om\302\30Ob7\376\37\307\364\0,\331\25\204q\274\303\365\336\231h\315\330\276\17k\214\326\272\373\356l\254\342+\307\30\307N\212\271\245\330Z\243\20\253\20\333\266\261\356/\377:\203_\235\352\323\325\307\370\345^f\3\366,\332\301\26\373\333\330\355?b\217\377\202S\374qLu\bV\265\17\236\246M\230\371]\274\253h\367|\314\357>\26\17\366\215v\234\215\366\255\30\373m\330\355u\234\342\227a\252\4O\323\16\314\374\3;\311\"\377\1\363\376%\36\254\310N\261\330\206`\267\326a\217\336\305)\272\26O\263\261\364E\252`N\313\60\257\243q\\\360`\177\27O\324O8G{c\301\377\305\242\nX\322\1X\321\0\234o{\342bE\270D\5\\\352\237\342\62\215\307\345\376\7\256\b\365\23\337\205}\376-\246*`\325\337\307UJ\360\64\377\25O\17\355\373\203\17\250\203\17\352||(\264\37V\206O)\301\247\25\343\352\20_\243{q\255\36\305\365\272\20\67\350\65\334\250\2n\322s\270Y\333\254"
	"\233\365N\300\274&\341\b-\302qz\37\17\366\v\361\30\215\300\23\365\6\226\324\301\371\372\3W\350!Lt%\256\f\361>]\217\247\5\317\325\205x\241Vc\315_\305\272?\202\r\377\1\233\241\335\362\255\330\366/\260\343o\342\26m\266\36\213\364\22\346,\207y\275\202\343\354\2<X\207\340\61\272\23\347h5\226\364(r3p\251^\303\25\372\21\373\364\b\236\246\325X\323\31XW?6\202M%\330\322\371\330\326\205\330Q\301\246\60n\31s\332\37\363:\nG(\306\203\375\22<\306\267\342\234\320[\322\336\70_?\342R\r\305\345\376-\246\352`M#\260\356?`\303\267bS\21\262^l\7Y\257\225\371\216\266\25\214;\3s\206\306)`\301Fb\321&`I\333\261\242\37q\236\r\304\245!\262\\o\342\212\220\263\322z\261\317\272\60\265AX\325O\270*DN\v\325N\267\211\364\215V?\36\254\303\60\326\205\330\253Gp\212\62\254)\306\272\246cC\5l*\301V\210\264u\6v\24c\306J\253\66B\253q\264\nx\260\372\221\323\301X\t\366\206\366\224\20\257\351J\254\207xC\35l\352Fl\351^l\207\314\216\310\t\225O\263\310Fb\336FY\315\272\375U\354\321\0\234\342\67b\252\205XU\1\63\177\326\352\344\334\203=\212pJ\210\244\32\200U\305\230\371\323\326\60\352`\217\377\201SB;U\214\324\301\214\23iZ\267\6`\217\216\303)!\222j:\222\203\31\365[\324\371\0\31\v\251\203\314\7\253J0\243\267M\316V\244\16N\361/0U\202\324\301\214\273\221\331\b]\210\7\373V<F\3\220\373\203\334\37\254\7\271?\330taK\21\266\65\0;\254\302l\370\377\205\217\65\264\325J\24@w\4wo\220S\341\311\272I\360\n\247{\rNu\355<\315\315\274\65\271O{\264_\364%\25=}\211K_\242\35\322\341\263\6\377\360'\266\63Gf\37n\22\0\0G\271\346\71`\21\317<\207\304|\360\34\261#\270\340\71fyp\303\363\34\66\4\357<\317eYX\20\21\304\v\200+\340\71`-\367=\207\314\347\215\347\210\63A\350\71F\202K\236\347p \270\345y.\33\303U\234b\206q\224AZtP\204\255t\330\206p\202\342\217h\215\220 \234g\202\312\345\30*fi\261\205\22e\226\24!\247AF\356\370\20%%\202e\204!\206\351\273\36\326\365R,\223(]R853\256\203\255\216\312\326\316\66\71Q\370_\255%\221\363\23\225\234\60\325lkK\251\263\251\344\215,O\345PY\212\35\31\32\356\327b\265V;\251\335\24/l\350a\350\242X*\214knz\246\253\266\62\320"
	"\306P\322\205\266)\273p\34CE\337}-C(\342\334\33\b\7\220\277\264\24|\33\237\265\207\204\214\304\317^\300qS\365\217\33;\244\222\247\r9 ?)\310\327\203\345\200\354I\262\344\313\60\5\377{\6\277W\303\31\227V3\342\364\205\214\224\275\b\3\30&Qz\264]\206\220\223\263\23\361j\2g\324\326#\246\222,\335+\3fR{m\265\222\347;\345\213\214\374M\346\277T\234\t@\4\0\237\266p\235\377Z\201\313X\306q\226}\372\304\62 \0\270\263\316v\334\367\341\340\23\200\273K_\276\372\f\357\327D\333";

const unsigned char* dbplogopng = (const unsigned char*)"\211PNG\r\n\32\n\0\0\0\rIHDR\0\0\1\0\0\0\1\0\b\2\0\0\0\323\20?1\0\0\61\344IDATx\1\354]\205\177\344F\322\65\r3\263\332\270\314\273^fffffffffN\262\314La\274\273\357\230\231\357\217\370^\300m\331\361\214g<\245\35e\267\337\257\16\222\334\311+\253\237T]]\357U\32\23\20x\213!\b  \b   \b   \b   \b   \b   \b   \b   \b   \b   \b   \b   \b\360\35\352\207B\315\2\1\265E\243`\260v(\224\37\211\60Ib)\204\224\223\27\251V9TX=\320\262\226\277c]_\367\372\336><\352y{\343\357\324\366w\252\21hU%\330(?\\3[\312c\22\23\370!\21`\237\301\360*=]\235\361\62=\375^f\346\71\255v\247\321\270\300f\33\342v\203\30\331\312P\"7R\271F\240M#\367\260\326\216\71\235-\233z\31\17\17\324\275;Lso\204\346Q\374\61\\\363`\210\366F_\303\351\356\246=\35\254\253[:f4\364\f\255\351o\17nP\23C\252Z9\322\272Y``O\357\264Q\256e3\34\333\226Y\17o0\237\337e\274r\320p\373\270\376\356I=\342\366\t\375\365#\6\374\235s\273L\307\267\230\366\256\266l\\h[2\315\61y\204kHoo\307\326\201\272\265\302\71\71\312SV\20\200\226\22\273\214\306\61Ng\275P(\271%\224]-\330\254\211kL'\353\272\376\372\363X\273\t\255\365\4\211\361p\260\366zO\323\201v\266\245\215\335#\360=\301\267\202\305\215\354l\251q\203\320\210~\236\65s\355gv\232\236_\322\376\341e\306\177\277H\373\337\227\4\361\367\217\323~t'\353\326q\375\376\265\226y\23\235}\272\370j\327\b3\360U\20@\375d8\251\323\215v:+#S\212\33y\221\252\205\236A\235\254k\7\351\256\214\320<\304\352LI\f\323\334\355e<\324\332>\17d`e\241j\225\310\240\236\336M\213lwN\350c/w\362\300\317\372\351\203\314\313\373\215\313g\332\273\265\367\347\345J\202\0\252\216\7\31\31K\254\326Z\341\60\213\216\234H\245\206\236\301\335\314;\207i\356c\375\245<\206j\356t\264\256\256\355\357\214\17\221\374M\217\264d\355<\333\223\363\272\177|\202\265\250\212\370\323\373\351W"
	"\17\33\346Or6\252\37\22\4Po<\312\310\30\344v\263R\220X\265`\363\366\266eC\264\267\260\354\324\20\203tW\233\71'\342C$\377S\342E\213\4\375\377\356gb\301\251\66\376\363y\332\253w\264+g\331\353\327\t\t\2\250\356#0\336\351\314\221$\371\322\257\343\353\322\303\264\27\371\267J\226\376\20\355\315\346\316)\370\26\261\"T\253\22Y4\305\361\361\65\215\362\31\16e\374\373\263\264\233\307\364#\372{\260\201\26\4H}\34\324\353Kl\210%\206\322$2l\365,}\374I:[6\26\204k\263\"\24\326\r\355Zi\375\363\7\351XO?\334\370\361\335\254\271\23\234\5\371\222 @j\342Ez\372\f\273]^\30\255\22*\354j\336\256\236\245\217\30\254\275V\337\333\217\227UjU\17\357^e\371\373\307\251_\372T\361\213G\231\263\307;_\367^Y\20\340~FFO\257\267x\7)\345\242\356\216\352\n\326\234z\242\267\361X\345P}\276\307E\235\361w\317\63\260h\336\274\370\354\206f@\17\357k\252\237\n\2\274\247\321\64\r\4\370]\240\242\337\327p\22\vNU\321\325\274-\267(\343o\324 \364\360\214\216\347\372od\374\347\213\64\34\267U\257\32\21\4P6\316\350t5y\321SbM\\\243\325\366\342Gt\264\256\315\226r\330\67\30\71\300\363\307W<\347y\303\3\265\254\236\235|\202\0J\305\t\235\256j$RT\335\317\307\71.u\306O\20\330\362\262\357V\277\264b\226\375\337\237ce\274E\361\317O\323P\335\302\275\v\2\320\257\376*E\253\37\r6\350\330\301jS[\240\345\241\250\323A\332\262\330\252\376\264G\241\343\344}k-9\331\222 \200\"\357\376J\241z\375\365\347T\270\372\7\350/\201\231\354\33\254\235g\177;W?\347\300\351\355\246\\\222\263\2A\200\263Z-_\375U\202\r\261\316T\270\372\207k\356\327\362\267g\337`\354`\367\177x\346\363\26\307\311m&\224\277\4\1\222\212+YY\274\325\247R\250\356@\335;*\\\375\210\266\366E\354\33\240y\363\333]\257\b|\7\266,\261\n\2T<\356dfB\6P\224\367W\357g8\253\316\325\217\366\322\334H\25\306\0\351\346Q\375\353l\321\371\331\303\314G\347t\227\366\32\217l2\357Ym\331\272"
	"\304\272~\201m\305L;t\2\210\325s\355X\202\350v>\277\333x\353\230\376\223k\32\364\267\275\346\366\322\361C\334\202\0\25\211\247\351\351\355\374~~\324\325\303\264/\205\375\233P\272\240\264\217\327|s\347dHg\352\372z\326\362w\250\21h]5\330\4\212\60\236\372\17\357\347Qzy\375\346Y\306\205=\306\271\23\235\35[\373+W\212\260\204!\325\251\31F\353\377\342i\16\244\351_\335\316R\372\17\214\357a\223\302\304[\350\204\"l\250\313\305\353\375P\223\274\336\226\375\373=\215\7\321\262\337\300\333\37\353\33e\315\70\327\326\213\313Z\205\226\21\32(\316\356\64\r\354\341\245\336YJhu\236\63\301\t\355\201r\373\26\\<\233I\202\0\t\254~\310 Y\21\240by=\365~H\267:Z\327\324\363\366\202H\222%\16\64\66+\361\66\375\353\207\351\20=\342\265\35C\276\214\312\30T\235P*7t\17m\352\32\207\317TK\307\314V\216Y-\234\323\352\372z\260\370\320\244\60\270c\271\365\317\357\247\247\70\21\22\4\70`0\360.\267\312\241\6\350%V\272a\23\371U\3o?.V\254\30\366\256\261\220\257\233\253\207\fe\210Q\244l\250\311p\n\16\272\366\61\234D\206\26\273<\5J\263\270\1U\344\301\365f\264=\323\336\v\264\227\371y\322\33K\0t\344\67\f\6\251\242 \22\341O\272\247i\277\242K\277\213e3\227&&\3|\342\321!Ly\244\372I\32\372\215\31+\241s\250\31h\333\336\266\34{\356\304\366\60\332\333\211\336c\257\316>l\257i\311<c\214\353\215%\300\0\b\262\24@\23\367h%\273\65\217`\27[:5\225$t\332\315\265\331\220\203%\344\313\322\260~\210\60\207\206Z\0\246\17%t\16\276.\311\350\34\6\350/\243\214\306\22Aa\275\320\27\267()\375\301{\232\4Z$\4\1PZ\201\321\210BU\35$\312rI.\0MY\177\217\347\230^\377\262h\37\62\n\273\360\270\61\270\227\227j\241\374\353\323\64\70\227\310\64\315\5\35\254\253\222\337\5u3\357Ht\371\341L\343\327O3\b?\2]\332\371\5\1\342\203\304:\330V*\261\372\341\203\202\16j&\3\366\33C\335\356w5\232\357K\215\221\217\261\370\200t\205j\225\240\205N\356"
	"d\321\313x\204*\337\203/\vK\20c\6\271\tw\366\330d\v\2\304\205\252\301\306J\330\365\364\60\355\317\vWc2\264\361\373O\353t\321\356\353\210^\37\247\33\27\216\237H\226\310GW5\305r[)\33f\26\264\322\344\202p\35\226\b\320\313\0\63\"*\2|r=\236,H\20@bph#_\375\310\1r\244\374bC8I\232o\263=/\257 ;2\276Dh\347\n+\311\22\271q\324\300\257\t9%y\375\267\223e\3K\20\20=\222ew\237\245\325\251\25\26\4(\7\70^\305\353\237\274Q\31\22\202\342b_8|\234\277\370c\306\255\314\314x\354\267\316\355\62R\211\315\371;\22\306\243\344\277\7\\\20'\6\t\356\4(\367\367\375\273{\5\1\312\1|\243\250\363\376\vp\272-~\242\301\340\325\254\254\370\357n6*B\345\341\376i\232\26 \bh\32\324\r\361/a'\353z\362/aw\363\356\204\322\20t\366\303\240\216\212\0\370\236\b\2\304BA\270\26\255\312\21W\253\36h\305\257\217*'z\354\22\325\340WCGjL<\277H\226(\317\32[\274D\320\210Anh\207\264\n\365_\226\b\340\263\vc,\222\230:\312%\b\20\v0\220\242}\336\270 \277x\335P\350F\"\357~\36\260`a1q\357\24Y\23\350\203\63:&\3\64\226\n\350\66\67\60\65B\20@\312F\272B\370\244\373\32N\361\6\207<I:\31\63\357\217\275\23\310\217\271\23@\277\61\241\373Z\323\206A~e\244\354xg\223\237\204\360s1\25A\20\0\242*\302\207\215Ka\214\5\277\70j>\311\334#\216\311Xtl^l#<4\335.\257\227K\254\267\361(\371G\240\211k,S\33\4\1\320\350B\370\214\321;\300w{M\202\301g\311\335#~],:F\r\360\20\22\340\367/2`\36\312/\216\3,\5\216D\366\61uA\20@\312\201\177\62\341\63\256\347\355\311\257\275\303hL^\240S#\34\216\321D\371OR[\363\245\323\35\262\363\340jHZ\250\65\17\367pY\246\36\b\2@cE\230\377@;\17\35\31\373\6\350hxAq\233#b\36\212a0\21!\1~\376\60\263RA\361G\240\213e\v\371G\0\303\316\230z \b\320\332>\227\360\351B\21R\234Y\371|$\267\211\317\b\213\216\361C)\333f\20\213\247:\344\252 \5\204\374\v\231z \b\0m\7\341\366\267j\260)\277r\7\"\2\334\315\314\314\215\336\32\224\227'}u'\213"
	"\326{\271J%\356\5V\3I\v\371\351\70S\t\4\1\320\374Lx\354\337O\177V~\330\331\25\4 \322j\266\200/otL\34F\374\21\300l<Y-\350\b\271{;\227;\247\30\202\0\320\263\222\273\364p\364\366x\250\356t\254\323\31\273w\22\2p\332)]\350\214 O\21y_\20\316\335\231\32 \b\320\306\276\220t{\327\267\204Z\305\355\246\272\323\265f3\213\t\254WB\21\t\342\330fs\321;\242'\371\66\240\246\277\35S\3\4\1P\263W\356\305\206\327\66\241Ki\371\233\231\256\276\277\177LF\0\f\215\304h\231o\215\361\260\267Q\256\20\204J.\241\223\241 @\2@\275\22\302m\252\207\n\373\304RrGt\362P\335\351\355\314LHd\342\21RA\331H\305\201\1h!\6\244l\332s\22\204\\#\6\341\313\237^\245?\273\250=\274\321\f\201[\247\66~\356\343\240,\4\1\340}B\363n\343\342\327\222Xa\261P\335)\216\223\271Sol`\224\",}H\b0e\344w\347\17\250\333\320\22\240\231kB\214~>|\307\236^\320ad7\232\370\371\241\4=\4\1\320\357\245hy{\273\321Hh\332\25\277P\270K[?\6\257'O\0\314\27+RJ\254Q\256U\366\332aC,\177\256\217\322\61 u\301d\7\375\320lA\200:\276n\224m^\356\321\254$N\361&P\212\300\251\2\213\33\65\253\207/\355\63\242\66JB\0\230\275\321\22\240\245s:+\302\345}\306\70\35y\361Y\300!\35\366\f\214\4\202\0(\332\20>T\320\251\224\325\317M\256\1\240\b>\240\62nHh\225K\346S0y\304w)PS\327X\352/\300\344\nwt\243\367\t\234\351\333\325\227\254\316]\20\240\320\63\220\360\241BR\314d\300`%\336\7J\22}\321\27\235\70\340\344\f\277\223\212MJ\355\327\355\273oNC\317\20\345\232\242A\200\n;^\341\4\220\304\265W\20\200\240\6\232WR\352\321*\20xIJ\200AI\334l\325\312\221ES\35\60%O\310)\210\353c``JK\0\60\212\25\341\275\3\206d\362\264O\257k0\22\223\376k \b\220\240\17\346\255R\36\267C\210N\301x\364\303\27 9\240\334\16\347\303\363\273\214\260@\214\307&\21\345H%\316\313\21r\301\320\255\343\372\344]\275\356\237\326\265j\32d\tB\354\1\372\320\35\2\274\313\337Ar!\30\361\36\200\b\250-b"
	"\240\30FL\377\374Qf\264\215\362\207W\212\315\244\32x\6\320\22@n\232\373\344\202\216h\224A\332\312Y\366\324L\211\24U \210\200\225v\201\357\210*\20=\244\226M\202\63\307:A\206\217\257i\376%s'?\272\251\270\371\242\221{\4m/\20\237m\3||UCh\3\nuD\335\70=\260\4\1\340\372Mh~X\312\362\26&(\264\303;`+\304\24\6\276\f\355[\5\260\263\334\270\310\206\62\213\314\62c\22u7h6\267w\377\305cJ?t\4\n_m[\4\4\1\312G\225P!\325Ip\27\363V&\3\26+\355\16\30V\212\334#\350\365\3]\256\304\232i^\244*\210\374\375c\372\331\60\277\177\231\321\275\203_\20\240\34`\304\"\225\30\240\203u5\223\1\"F\332\374\347\36\357\5J\5\272\232\267\23\22\0\26\4\362\371H\362M\b\255\300\37_3A\200r\354\200`\\\254D\37\304\26\223\211\226\0g\320\r\232*H\f#\362\25\352\204\33$\237o@\35\30\66S\257vX\20 \266\36\362\4\311Cm\341\234*\337\0\334\"\335\0 \326C\17\220\"\340;I\250\212D\316Y-\330\234_|\345l\273\242\303R\261'\216\325k-\b\0\3@\362\267Z[\277\237v\3\200\230\350p\260\24\241\266\277\23\351\254\200\33\334\65\3@\243\233\322\23\263\321b-\b\20\25-\35\63h\274\200|=\371\65\27Z\255\344\363[A*\226\"\300\347\202\220\0\230\373\315\257\\\275j\204wn+\27\277}\236\201\37\304\242@h\202{\221<\327j\301\246<\377\271B\332\3\207x\220\221\1\207\320Tm\0z\221\212\342\61%\255x\3\320\323\213y\356J\23\0\261v\236M\20\240l`\324\63\262\322\344\235\320\371\214\353\316>\37e\376\303\335\21S\0\376\373\271O\270\1\200\361:\223\1\335\26#\373{0\237\6\335\316\n\356\206\37d\346\346J\202\0\321J\34\27\222>\6>\311\373 6S\327\177\20\343`\t\221\"4#=\2\353g\340\266\61\245\321\246y\340\312!\203B%Q\\VL\210\211\2\n\271\23\364\"\337\365V\204BO\251W?\216\300\32\204B)\312\177\262\373\31\316\20\22\0\16+\261\177\36\276\6\277y\226\241\4\7\60LM\20\240\24h\334\377P\326@\236\300\276\301\"\352\355/\2\63\305X\212\216\300\320+E\353\32\217\221\71\361\214\6\373\354"
	"\206\206\234\0\217\317\351\4\1\312\6\372\370\61\16\250\302}]h\25\376\316d7\34~\230\221AL\200(\343\"kT\v\243\335\205$\220{\224\335A)\261\36\246\275\270GZ\333\274x\200\323\253\37\337\313\242%\0L\223\320w$\bP6\272\233\366T\354\225&\227\366\255\344\36\20\244\365\237\62gEN\36\356\242Z\31\30\217\7\225\255\262\246\331\374\254\60ntn\343\377\7\251\355\373\337>J/\310\217\b\2\224BR\355\276\350i\341\357\24xw\22\n y,\267XXY@\253&\341\6\261\214\236\31)\247\227\361\60\351X\200\373\225\22\234\225}`\235\205\220\0\240S\225\312\202\0\321O\373\23\235\4\321\321\272\26\253\204\317\276\246\65\200\340^@\205\301`4#D\302\272\341\354q\316\357\275\21\206\215 }\375wE\267l\202h\331\64Hx\217\20\270\345\345\212\24(::XW\305\237\371t\260\256\314\226\371\33\317%\25\177\361\330`6\307\20\67\302\304\234jq@O\310\230$w\314\246\265\202\303o\254\266\257\vK\30\22\27\61'\37\237\337\314bb\17\20\3\325\2-\360\234\342y\226\255\34\263\345\26\210\60kx\241\300\352\177\312_\377\312\317\207\374\317\27i\275\273\370\370\225[8\247\361\373%\223\313\341\67\226\70\336\345z\371\244\343\326\61\275\250\2\225\203n\346\235\345I\231\256\327\367\366c2\264\363\373\225\250\374 PQe11m4\331>\30\361\321U\r7!\314\211\24t3\357\242#\300\303\206\236\241\254B8\267\313Du\203\260\22\22\4(\7\230c\36\355#\200\277\217..\370$3\31\320\240v_\231\325\17G\255r\235@k\327\f\23zA#.\354\61\346\344H\334\66\270\275m\31\356\232\302.\340\62\374\62*f\345\362\353'd\207b\7\327\233\5\1\312\203TFw4\26\1\254a\321\22\\\"\201\224\244\201n\367ceV\377K\334W|\16(\357\35\64\320\26\313\221V\25\344K\374\267\1\327\f\270]$\335(>\262b\17\203\266\n\264d\232C\20 .\277h\344\71h\377\202p\33\353\276\225cf\265`\263R{'\274\233WY,\344\35o<\266\232Lq\36\375\16\350\341%o\236\201Yy\363FAY}\254\22\332\241q\332]\261\325\217>\253\n\274\376qb\265a\201\215\360\326"
	"p\251>]|\202\0\361AB\224}h\230'Ic\234N\271\332\213<\256ee\305/~G-\b\366\200\344]\3\177\371 \35\63\302\340\251(\243A\345&\256\61\275\215\307\22J\212\360?F\233\tK\20\70\6\306\336\227\226\330\230\370\4\365\275 @E!I\315\3\201y6\333\215\322\215\376\364\225\237v\t\n_F\17\364(\324A\371\313\307\231\313g\332KHI$\6\37\r4\364cx\360 \335\225r\311\0\302$T\374A\177\7\32\367\377\360\222\276\31\356\322^\343\233\251\7\300A)\34\b\225\210\341.\327\4\247s\201\315\266\307`\300+\237\60\341\211\221\372\217v\271*\340v\370\350\254N\271Nz(\266Nn5!\327*\355A+\261\202p\355Z\376\216\250\360\240\307\241\265}^;\333\262\366\266\25\337\304rL^C\342\304EB\261\201N$\24a\217l2\343=\255P/\364\360\276\36U\23@\4V\377BY\335\223\347\337H\240\313}\211vh\35\370\347\247\212\213\252\320\245|j\233i\352(\27\272\65\223?QB\271\251u\263\300\254q\316s\273\214\277U\246\377\231\7zK\301^U\23@\304&\223\251\224\355O-\177{x1 \315\300\277c\17\212j\f\274I\340a\1W\251\356\346\335\235-\233\345\3\371\66/\266)M\0\371\v\25v#H\323\327\317\267\215\33\342\356\320*P\255J$\n%$\254<\30\25\302\266\26\315KSG\272\260\265\0\213\236_\324\376\205K\201\225\377\323b\222\276\252E\361\" \"\203\222\270d1\252~\271\315\b\350\256\341\37\7t\271`\310\34\365\352I`\221\375\341e:z\r>\271\256A\340\215\373\343\273Yh?\206\336\367\337\334l4Eq\347\204\272mQD\346\263\316l\346\253\237\267\342\364\323\237\213\247\306\322T6f\242a\275\320/K\232l\212\300~\32\t\233\212\215\261D\336o\263\225*\371\347E\252\366\61\34\217\273\307\370\36\266\241\305\6\206\355\375<\265\20\201\217\317\60\272\275\257 \0}\305\23\205\246\357k\323z\33\217&\352\264,\367Y@\271\6\312\17\261\372\321G=\207\324\17K\20\200\62`\34\324FV\357\347\231O\305|\32\7\350/\311\233\224\340\266\211\332\345[\376\356\237?\211f\365\v\2\320\247=\330\362\312\33\335\270Q{\177\375\371d\344\266\5"
	"\62\275\25\234\301\261\7};W?>\200\70\34d\200 \200\332\2\7j\375=\36y\322\317+\236\311\vP\320u\203\332\221\334[\341\375w\265o\333\352G%\252}K\2KtA\0zo\237\25\26K\31\35\316\22k\354\36E\345\275<P\367\16Z\367\370\265\241\1?\264\301\214\32\345[\222\364\343f\tT\277\202\0\344\71\317\36\243\261i\240\214\327RN\244\22\204\305\264\316\v\30wP\327\327\203\311\60\244\267\227\317\312~#\3\f\307<\277^\235i\232=\5\1\310\2:\311\335Fck\277\277\314\336\346\352\201\26\264S'\344VE\315\235S\344\r\24\350\353\334\276\334\312\325\63oR\300>h\312H\27\367\65\22\4PE<\312\310Xm\261\64\211\242\350E{\17\254\24\251\322\236\30\63\313\362\"\325J9\256\235\336nJ\371\321,U|q+\v\303-I\214\36\4\1\310^\371G\365z\30\271\301\312*\306\260\tTlh\327z\214\362(\27\262q4o\34\304\374\323\37\356Y\1\354}\320\206\204R/U\203\203 \0\301\221\326a\275~\202\303Q7\24\212\255\65\353l\331\224@\306O\224\16\265\263-\301f\243t\335\251Fx\305L;^\242?\240=\356\323\v\272\5\223\35r\23;A\200T.\372\23:\335R\253\25\323\333\371\373>\32\320\266\t\201\71a\316\223\370\247\340b]\f\260\221\312P\36\342\304`\317j\v\272;\325\271\356\321Nw\345\240\1\276]\r\352\204X\252 \b\200\"\346\325\254\254\3\6\3\364\67\243\235N\354k\v\22\231\327\2\v-\244\"m\354\v\220\374\274\326/@iW\213\355UB\r\243\251j:\266\16@\215\365\360\254\16{\345\324\226t~\367\"\343\352a\303\252\71v\220\223`\252\205\n\t0\326\351DQ\\\205\261\314b\201*e\226\335>\311\341@*\337\313\353\205@\21\211M.\235;9\316k!\223\205\261\34\274\242\210F\24'\20\320\373c$\24\213\211\374|\251GG\337\262\31vHU\276\274\235\365/\205\67\315\270>~\n\354-\266,\261\216\33\354n\332\60H\236\334\323\23@\200\312\220\24\16\314M]\343\61\241\3\362\331\241\332\333\344o}\34\v\300\343\266\235m)\274>!P\344f\246\361\3nY\220\274\214\31\344\206\226\345\310F\363\365#\6\24\335\177\365$\3\32\64\274\255\343\317\340"
	"\377\364*\35\"\201\307\347u\227\366\31w\255\264.\232\342\300,\f8\263\343\372\214\0\202\0o\0\244l\250lk\372\333\27z\6\242\220\217\315+6\315=L\373\372\30N\16\320]\306R\306$2|4\260\254\213\342\1\366\25C\265\267\240O\207~\0\v\35\326]\370\266@\214\v\235.\332+p5\246\330\313\24\232F\b\301 B\300\"\356\334\326\337\263\223\17\63\210\320\213\312\3\177\247Sk\177\213\306\301\272\265\303\30\4\226\264\212R\20@@b\330N\300\263\215\7{s\326\225 \300\377\263O\7R\0D!\0\300lR\311$\217,\202\310 \200\257u\36\367\66\207\201\0 \0\b\0\2\200\0 \0\b\0\2\200\0\277\26\21\231YU\335=3\273{w\357}\354\234\203\266\\K\20\206\37\245b\35\237\254\330\266m\257\330\266m[+\266m\336\330\311\211m\247\237\340~\327\354\32\365\236\243\375-\17\367\364\356\352\256\377\257\352\371\351\306\215\33w\357\336MII\271w\357\336\315\233\67/^\274x\364\350Q\236\345\65#G\216l\323\246M\251R\245\262f\315*>i<\0\270a)\36\300\264\270u\353\326\225+W\316\234\71s\340\300\201\215\33\67.X\260`\324\250Q\314\244\312\225+\347\316\235[\322\60\205\n\25\352\321\243\307\252U\253\256]\273\366\355\333\67\23.\37>|8q\342\304\214\31\63\32\66l\230#G\16\361\214\n\25*\334\276}[\271\35\334\205|\371\362I\24\251_\277>s \305\2KF\331\262e\323D\0l\332\264\311D\235\357\337\277\63\4k\327\256\355\335\273w\221\"E$m\300\232=i\322$\202\366\307\217\37\306\65o\337\276e\25h\321\242E\266l\331\304\3\206\r\33\246_6\337.\321\242@\201\2\317\236=3\26\270\316\276}\373\nd\252\0P\206\343\362\345\313$\30\371\363\347\227\324 {\366\354\235;w>u\352\224\303y\257\360\350\321\243q\343\306\305\307\307\213k\326\257_o\300\373i\247\303^w\372\364icg\311\222%\2~\0\374\213/_\276\254[\267\256d\311\222\321\234\372\375\373\367\277\177\377\276\211:\357\336\275\233\66mZLL\214\270\203\304\362\322\245K\306\316\307\217\37I\226\304c\26/^l\354\240\224\30v?\0\254|\375\372u\336\274"
	"yy\362\344\21\217!\33!o6\251\ny\2bC\334Q\264h\321\227/_\32;h\367\274y\363\212gt\355\332U\331H\311\376\23\23\23\5\374\0\320A@G(\222\24\270\7[\266l\t5\341\341\365\17\37><~\374\70\311\64\316\1\322v\362\344\311h\206\351\323\247\243\357\321\63\7\17\36D\330|\376\374\71\324\217\345\215$\315\342\210&M\232\350\302\235K\365N\213\243\373\215\205\367\357\337W\254XQ\202\301\17\0x\363\346M\215\32\65\304\65\30\62O\236<\t>+\303\364\34;vl\335\272u\203tQ\320\270\345\312\225\353\325\253\327\232\65k\236>}j\202\343\305\213\27M\233\66\25G \250\224\360\346\251\236={\212kbcc\357\334\271\243\330\36\30\200\2~\0\4\317\253W\257\212\27/.\356\30:t()V0\253\362\331\263gq\250\342\342\342$|\370\177\207,u\352\324Y\271r%\213\237\t\4\313\366\360\341\303\305\21T$\214\35\256\307\355\6\313/\335\263g\217\261\203\340\21\360\3 T0\260\31\\'wh\376\374\371\1\323\36^\260o\337\276\232\65k\212S\b\244\211\23'\262\247\5\374\366\71s\346\210\vH\364)b\30;W\257^uX\215\231\60a\202\62\266\273v\355\n\263&\350\7\0\303\332\251S\247\310g\377\212\25+L \20\210$H\342\31h\217\325\253W\353A\310\263(\nqA\211\22%^\277~m\354P\351\23\27\220\274)\252\343\372\365\353z\366\230\376\2\340\301\203\7\365\376\17fO\313\226-\273u\353F\305w\371\362\345\230\301A*B\35>G\"\3[)`a\216\245\67g\316\234\342=\315\233\67\177\376\374\271\36\3,\250\256\234.~\232\362E\230\66\236\372N$\261\304\241@F\n\0\274\216\340\235\351\266m\333\342r0\326\221\224\215\223\222\222$\\\6\16\34\250\177;5\332V\255ZI\24)\\\270\60\31\210\1\357%#\251\227^\216(]\272t$\215R\27.\\PT\r\226\224@\346\t\0\305\233\323-\21\235\260\307\221l\36'\307\330\341\252\234{sAz&\310\33=,\251\t:\21?\244\340\306\16ex\346\261\204\5\31\235\262\275\214\30\61B\0\374\0\0V\32E\5\352\340*\206'\4i\330\324g?\375?\222J\20\3\364\35\351\271\237\23\355H\275\231[f\354,[\266LB\247_\277~\312\326J"
	"Q_\300\17\200\277C\237\260\t\v\306ZB\7\65\251\357\376\364\71K\252B# \25\0c\207\374M\\P\246L\31\266\24\207NC\365\352\325?}\372d,\234?\177\376\177v\25?\0\n\26,\250h2\205\60\22b\252QX\376\312-\357\320\241\203\244\1Z\267n\255\254\243\204\7\33\205\270\0\61\246\f>\233\63j5x;\v#\304X\240\316\370\377\205m?\0\330\320Yw\303pB\313\227//!\262s\347Nc\7WT\322\f\233\67o6v\350\266\20GP\215\62v\220\263\370`\301\224\272\17\35:d,\340\373Y\253(~\0\60vJMTYQBM\205\t\30e\265{\374\370\261\336\206\31\375DHI'\360L]U\254\30F\312|\306\16\215\234\22\b\232\240\224\245JSk~\0\24+V,\f?\224\21\227\20a\201w)\251\275g\351\322\245\306N\367\356\335\35\326\244\365\216\235\366\355\333\207\235G-\\\270P\334\342\213`*)d\234\241\232?\212\340\303\27b#\222\64\6\26\231\62\261\16\37>\354\266gS\331\207)\36\263N\331N\314)>\36]\203\332\300\372\1\220\234\234\254W@]\225*Q\314\306\16\375p\222&9y\362\244\322\224\232\220\220 \356\350\330\261\243\262\25\237;w\356\277'\230YV\224\342\35\307\177\265+\364\3\200\235\227\272O\250\263\177\356\334\271\22:\33\66lP$\232r\237R\227!C\206\270\365\301th\375\60vh\36\t~\236\260\237h\7\315\374\0h\320\240\201-\357T\240y3<\235\247\264\373\357\337\277_\322*\324}#\224\247\241\32\22dV\212\30\370\373\371\204\1\3\6\330v\f^\331\256];\1?\0\376\273\352w\351\322\205\63\240\241\n_\326i\216\352\206=\215\224\257c\225\225\264\n=\v\330S\306\2\347\23\274\350N\345\214\242\261\300\361\67J\20\277\245\376\312\71\257)S\246\bd\252\0\240:3\311\2N3k\325\216\35;h-\326k^J\21Qw\375u\250h\32;\374%\221\244a\224\63%hS/$f\225*U8)\257\374}\3;*\177md,p\243\211[q\202\177\36\200\325\210\243z\21v\277L\235:\325X\370\231\275\373F\270\32\t\202\0|\224\71\300\237r\2\262\365\336\234`\275\217 \304C\304\237\343\211\327{\33\223q\30\235`\277\365V%\351\315\350=\355\62\225\340~\236\221\324\63\323]\325\325j\355"
	"\201\237\337\2\216\217\217CF\264\222m\214\32\253\27\37\223sjg\v-5\r\204\376=\0l\24\216I\352\22MV8\35\353\351\344\266m\220<\35d\373R\277\37\26B\17@U\267j\17\0W\320\6\352\254\337V7\22\366k\4~\331\66\264%\f\343x\361\305\27\327\363G\342v\261\310\272\246\366\303\364\0`\220\206:\321\70v\352\324)\356\17\255\314\222\344\36\311\33p\333x\362\311'\207q0\334-\253\1K#\353\35\346\201\rci\213\236\3\250%\177\372\351\247\\\242*m\233\304\325\306\5p\1\344\331kS\1\1\226!i\322\60\205\273w\357\26\350\1\260\236g \333)\376\314e'\4y=\275M\331\66\224\277\206qP\351\254\355\351\311\344}\230\202\322v\17\200\325a)\242\201\333A\5)\265\370\357\356\0\232\63\207q\350p/k\302\322>\4\4\336\267\7\300z\6\211K\33v\3\rL\"Q\266\rJ\372a\34<\266\312j`X=d\354_\371\323\3\0p\220z\246\312l\210\231a\4\314)\312\266a\215\37\306\21\354\33*!\273X\312Z\222Q([?\240\1@\23{7\202Q='\2\354=\367c$K\275_\364\374\30`\363\37|\257\312\266\361\336{\357\5\213\21\302\204\225r_\253\314\330J\37\346\\\314\62\360\352b8\226>\304R\234b\235\35k\366\1\tb\345\327\301\371\207\226\277-@\332\23\f,\326\20\35hF\363\312c\334\63\17\257\323\247O\7rZ\325\256\7\300\2\27\20J\241\235\275\342\214~\260\347\326H!`\v\331[\200Bp\230+QZC_h\220\370\263\327\375\205#\363\63\241V\301\36\242\7\300\62\365U\320<f\314\61\365\346\365\320\336ed\3jP\362\375\346rh\342\360\320\210|\342\304\211\337o\231\7=\20\232DJ=\0\226\31\226\344\343P\330\4\346\354\351!\237\223\231\224\255B\216\273O\32\230\322\63$]/\274\360\302|\17,\6^8\204\36\0\vp\341\302\205\335\234\242'\5X\326Q]\277!\227\330\224\37\304\314\206i\347\306\266C\365\364\205\16\343\340m\\\376\1se\206q\20\215\366\0X\0\233f0\324\256\244!\21\311\303\70TZ\312&\21\\f\211\246J;p\360\16\27\237I\321\277f\333\246\266\61rls\274\354\1\0\273\315f\234\343\33n\234Q\330\257\335E7\270l\f\17?\374p\370\314\r\307\233\"\26"
	"\203\67\231|7h\261\264\206\5\273\r\235\373,\362{\0\314\5S\342a9\264\214\314i\vF\207\265\24\325\254\17C\373\202\70\252\25\3 \257\r\222OMg\32J'k\f\2\65\244\316\234\337{\0\324\312\66\353GM\345\63\256\351\250\233\"\4\224Y$\356k\267\303;\303\334\273w/\20m\216F\365\323F\310\351\246\25\\=\0\254\23\341\226\327\273\372\330\307\3\205\tf\233\226m\300~\305\60&\244\277F\260\64\251\261j?j\342@\212\31\b\26F\263\372.z\0\260\22\30\2Z(\"\363&`\301{\366\331g\313\6p\366\354\331a\34\22\372=\270\0\251\16\213\220E.\337a}qFRG*\1\235\7\b6\33\371\251\325\270\64_\335\236\307\20\271\205\25\213k\263\241\305\212\356\341\23\66\251~*|\205\203\273|)\344\30\341\223\273\35\341N\261\25\352\1\360\357\223\32\202\265\374\244c\312R\231\227\307+'\3\7d1\365\177\311;\303:\312(\240T\3\245\345\"\204\f\333z\264\263\202\332\207\234\233R\367\0\300\275+\25O\316\7h[iV6\235l9@\36\37\244\356\31zw\200|\260\311`\230\20c\322\60U\235\6\303\261\363\200\324\36\0\350[\303\373ik\207\n8\17\250c\354\220b:\340N\316\37\260W\224=\202<6\213A\220b\276l\275\361[\276\346W\257^-upB\v\336r`L\211\70\371\277\5\200'\346\255\b\304\rb\237\265\255\207O\341\271\276%\300+\274\373\356\273;\317Lwv\232\64c4\322p\17\4\231\22\241\274\66_\20\24a\275U\214:oh\215\0\355A\366\344&[\31\251\\\270q\n\36\275#\254\26\237\177\376y\315\323\251\177o\16\357F\324\345\\\276\352\70\260\311m\20=\322d\262\264\376\317l\302\327\252\247\61K\345\32\270\347\366\0\270\177\377>\237\335\372\315Z\207\332\34\233:\217\216\274\260\255\316\31\307$\272&wB\36\332\206S\224j\\\271r%\274\227\22\\s\223\71CQ\263\323G\356g\352\1\220\22)\4~+*\364\203\17>\230y\342\322\363JxW\231\303\235<y\22\301\64\63\371\341i\347\324^\252\341c\207\247\337?\275\361\306\33k\34\355\262TNl/.\351\366\0pN\265\366\267]\214\21O\312\202\363\215\332\31*\252\367)&z:\347\344\33\316\304\352\356\267o"
	"\337v\314\230\231\374\330v\344K\30\326&\336r\341D\276\352\30/EOR\271fYG\367\5R8\252q\212\316C\236m,\303r()\312%\204\204vA\372x\355\377\266~\277wki`vs\0\20'\242\253\264\200j\33\71Z\336d*\303,\203\312\60_\1q\336\3`\372\370\241\65vm\16\5Ol]W\213<l\220\223$9\230\265\32\377\310\366\"\247\327\201Go\205,\225K\7\260\36\0\370s\202\255}\226\344\321\300\216\4aB\304Jp\262\272~\375:\373\307\206\366\316\222\226\374\216\373\271\260>I.\277\272\332N\211=\0\376\262*h\366=w\356\34\211U9\4\30\267\\\272t\t_\261\37Ol%\232\346\372\v\35\211\371\n\207\226\232\346@bd).-\214\275\350\201\16\0i(\245\332\235;w\30=X\b7\"K\346y\357I\n7/#S\207\234I\275\376\32Y\r\351\245G|S\r\273Y*\7\4\325N\241\233\b\0\213\337\17\253\301\276\214\303\322\356\344\366;\357rYB\6\251\262o\331\240J\245H\231\34\61\254f\212\224\r\317V.\354(\374\351\257\245\312\326\205\270\36\307L\355\243c8\334\2\v\234\253}\220\251\257\371\331h1\376^\0\254\214\216\243\243\243G\36y\344\345\227_\376E\334\241\362\363\341\207\37~\365\325W\337\177\377\275\273\350\341\373\342\213/\24\202\324=\217\217\177d\307\16\b\0\0 \30\200\305{\251\207\20\231\b\2l\35\340\237\266\327\204$\371G[\f\0\30\0\60\0`\0\300\0\200\1\0\3\0\6\0\f\0,\373f\301\334\66\272\205\341\377p\371.\344\210m\31$3\311T;\206\200\35\330\244\314\354`1)3\303\66efn\246\260Pf^..3o\272\371\21W\227)\255\216*\307\361x\362\316S\356w\216\374\332\217\343P\366\5\330$\b;EQ\v;Dq\213 \264\231\315KL\246V\203a\214^\237fY\33I\202\346\324r\34\362\32R\f\3\232\63N\257G\256\223(\np1\23\4j&\242\341uf\363r\223i\226\321X\257\327\367\345\270\0E\321\220\315\260\0\362\256\334\340\247\250|\21\340K\267\273\303\353\355\16>w\273\337\264\331d+\252X\226\202\347I\213\301\200\334\225\311\306\227\36\254\27\4\344\272r\264o.\212R_\35\226\37<\236[\16\307\66A\220o\276U\363\63\216\201 \220{\265\223f\331\2\22"
	"\0\301gn\367fA\210\321t\257\0\335\304\317\36\317Y\233M\356\201\1\365\351\25 7\374\342\361\234\264Z\243\64\335+@\367\361\310\345j\344y\2T\247W\200\34\361\243\307\263\330d\"{\5\350NN\333lj\337\r\353\25 \247\234\260Zu\4\321+@\367q\337\351\364\342\337\327\354\25 \367\234\262Z\251^\1\272\223w\235N\203\302\263L\257\0\222\247#\340\355)V\210\346g\t`4t\370\275\30\62|6\4\20\5\344\272rV\215\0\1o\17\262\325*\342\5\310\331U\245\271\374\21 \346\353HJ\212|\25\363\355t[\376\207}\36\353q\237\375F\310\365\65jH\27\374\230\220\242\34\363T\1,\306\216\270\204!c\346\263 \200SD\256+\327\261X\1h\nY\305\365\220\253\316\314\377\17\365f~\262`Xl3\357\365X\337\355\343\376%\241\272\341\237\22\222\237\245Q\2\220\4r\246v\322\372\374\21\240\322\337\361JP\221\367\312\274\360\364\20\0\21\35\63\333e~\257\324\373\217#h\16E\354O\25\300a\352\250\n`\310X\rY\20\300oA\256+7\240\5`)d\17G#\16PJ\220\243\327K\226\37\253\2\252\32^!\211X\1p\3/%\334\36\206\322\2C@\336\b00\330\61$\254\310{5\22 B\2L\361\230\276\35\24\222\217 \371ap\310\302\220]\v\340\65w\f\16c\310\70\215Y\20 bC\256+7qX\1\70\n\331\303\321R'\340Rf\342>\355\37\300\67|\263\332\207\22\200\"\220\3\317\244\334\240\65\371#\300\350p\307\370\210\"\357\r\r\0:\325V\335\367c\373\310\247\220\324\371L]\v\20\24:\306F0d\274\246,\b\220\264#\327\225\213h\1t4\262\204\243Un@\247\237M\377\v\272\336\37\307Eh\2!\0M \7\236\351\353-\34\1\276\232Z\374dF\\\221\367\33\303\240&\v\313l\362)$[j]\320UZ\213\305'Sc\30\62!3h\316\206*'r]\271M\207\25\200\247\221%\34\33\342\3\65\271\60:\200\234\334\61#\356\320\323\312\2\60$r\340\231Q\376\302\21\340\353\345\311'me\212|0/\6jb\346\250\357W\226\310\7\61\234\233\22\356Z\200\224\365\311\312R\fuq!\v\2\fq#\327\225\273\365\200\213\333\304 K8\326\20P\367\24\323\327!\237B\261\246\314'\260\312\2\260$r\340\331\311\341\302\21\340"
	"\233\355\345\277\356\253P\344\203\266$\250\314\345E\305\362A\f\357\256Nt-@_\373\257\273\322\30\352\322\26\320\234\215\343}\310u)\211\307\n 0\310\22\332\247\253{T5UY\345SH\254\6\304[\0\216DN;7?Z@\2\34Lu\36\257T\344\336\326\22P\231\3\63\203\362A\f\217v\224v-\300 Gg{%\206\272jk\26\4h\226\220\353RA\3V\0\221A\226\320>O\235\0S\7\331\221\223\277=\224\"\0\60\2 \7\236[^H\2\234Lu\236\253T\344\336~\325\2\354_\20\224\17b\370`_\327\303[\207;:OWb\250\353\233\r\1Z$\344\272T\30-\200\225A\226\320\276L\235\0;\347\4\220\223\337x5\2\210\30t$r\340\271\265\205$\300\371T\347\215JE\356\265\253\26\340\374\266\250|\20\303\245\35\305]\v0\332\321y\265\22C\335\300l\b0SB\256KE\321\2\330\30d\t\355kT\b \30\251\257\317\225#'7\r\265!\5@\16<\267\265\220\4\270\226\352|\247R\221{\257\253\23@\307\221\337^GM\226\331\276\304\337\265\0\343\35\235w+1\324\r\311\206\0\363$\344\272T1Z\0;\203,\241}\203\n\1\366\255\16\"\307~|\276\214c\t\224\0z\22\71\363\334\356B\22\340n\252\363A\245\"\367\316\251\23`\342\70\233|\nIf\230\245k\1\352\35\235\367*1\324\r\317\206\0\213%\344\272T\34-\200\203A\226\320\276\25%\0E\302\306%\322\257\17*\220c\307\17\375G\267\30\1\220\63\257\34\213y\234\354\363a\267\322y&\300\7\251\316O+\25\271wU\205\0\222\227\375\342\275r\371\24\206\37\36\246\315F\252k\1\232\34\235\37Wb\250\33\231\r\1\226K\310u\251\244\n\1\220=\264\357R\26\240$\306_>Y\214\34(\263\177S\20\320\61\360$b\246Vn\237\211\347\227\0\217\356\225}\365YZ\221\233\327\22\200K\"\311\337\275U\"\37A\262~\215\4OI\313D\307\317_T`\310\214\266\200\346\254[%!\327\225\227\360\200\213\323\311 {\330\267;\324\305\363=\5\242\205N\226\362\23'9N\236\210~\371i\32_\354\361\243\21\226%\0\35\236'\21c\265r\341l,\277\4x\363v\362\362\375\62E\16+]7A@$\311/h\223.~P*\377\177$\247\357\224Xm\f<%\231F\333\273\217\313\61\f\35!\202\346,X\352E\256"
	"\213%\260\2X\355\f\276\215,\262|\275\237Q\371\25g:=\231\203\v\333}\"\222_\2l\274\30\333q;\241H\333\231\342\322\301\346\377!5\\\250\251\263\216\230\351l\331(\255;_,\377\67\265T?\363\201;(c=\366v\t\206\352\241\2hN\363|7r]0\246\7\\\314\66\32\337FV\330t)V;J\4\365a\365d\16.o\321\241p~\t\320\362zd\346\305\342\36\241\337\f'<3\251\321\342\212\253\61\f\361\201f\320\234\301\63\235\310u\356\250\16p\341\255t\316\372\234~6:p\266\223\27(x\256\60:2\7\27Y\267\63\220_\2\364=\22\32x\252O\356\211Ms\200\322\233\350\320p!s:\212\301\327\327\4\232S\332bG\256\23\303X\1X\v\235\203\62kv\7\372\324[9\221\2\r\241tD\16.\265b\243\224_\2\270w\371=\207\202\71\345@@\30f\6D\254\203\314\311\366\20\6\241\332\b\232\343\232`E\256\323\7\71\300\205\22\250\356k\322\271\306+\216\26u\1\26\262\21\202#rp\357\333W{\362K\200?o\360\375y\207?g\274\270\314\r~\26p\241\372\31u{\374\30\250\64\17\232C\327Y\220\353\b\364M(2S\335W\346\237\266J\177j\261\277\30\344\212 \v)b\211\34<\0^X\344\312/\1\376\330V\372\307M\251\\\260*\371B\215\245\210R#\347+\302o\267D0\274Pn\2\315\371\323\30;r\335K^\16-\0\223\203n_h\16\27\361\264f\1H\354\306\225\311?\327y\237\217\27\6;\362K\0X=\34\326\217\356V\212f\327\26\245=E4\t*\363r\332\371\302\306\376\30^\256p\201\346\274\64\246\17r]\221]\17\310\230t\332\vD\261l(\370,\240%,\205\335\325R]8_\n\241[1U\327\66\63\373\254je&\217\240j\23`5\301\363\206H\370\250uM\30\210\376q\320\34\262\241\6\271\16\f\34\340B\230xdcL\343P\370\317\360\34Q\354c\246\214P\321\371\352iTqH\203\0\fr\21;eT\341\b ,])\276\272N\231Um\346y\v\376\37\323\334\371\246\231\263\215-\255|S\223~\324(\256o-\35\217\221v\33\220\4h\16\351\367\32\326,\306\300\215\33\16\232\243\237\66\1\265n\345|\240(\264\0&\271=\f\206\372&\350*L<&,[\205\34\"\256Z\313\306K\236W\0\26\271\305\70ij\341\b\340^\274\337\263\262]\21\347\254\315\220"
	"\363\220f\321\276z\7\6\313\354\25\240\61\24e[\276\5\265k\326r@\207\62\212r{\30,\231\271\360\224\320\16\257k\301n\344\34\317\362\243\272P\31\250\17\301p\310\25\266\346e\205#@x\301\305\310\262[\212\4\246\37\207\334\207 \244\305'\374+N+\263\374M\306\342\1\r\321\371K0\213dlc\27\2:\264\321&\267\207\301=\256\r\236\36\326\21B\336S2\341\205W8GD\275\0z\344|o\343\366\302\21\240d\356\243\262\305_)\22k\271\t=\21\177\343\376\370\322\17\60\70\7.\2\r\361\216\336\204\\$\226\327\3:\214\321)\267\207!0z/<3\306`\277\262\205\237#\247%f\274K\363vP\23\222\341\221\303\303\365\307\vG\200\352\326\317kg\377\254H\252\371]\350\211\270\252\346W,\374\21Cj\336\327\234\20\204\347\no/\251X\360\3n\321\17\234\30\2tX\203Kn\17C\361\260#\240\24Oj>r\232L\351\270\213\4\311\250\21\300\200\234\234\30\363f\341\b0(\363pX\343\227\212\364\35y\3z\"\274X<\240\365{$\351\61\227)\232\7\225\241YSe\346\16v\305xu=pz\247\334\36\206\322\232\275\240\34\242\254\337A\344@\231D\305F@G\256\16\71\66=\240\200\336\2\214\36\370vf\350cE\206\326^\200\36J\355\240\263#\307\177\206\244\246\337)\216\263\1::\235\263\357\200\63\370\371\376\320D\225\337\32\352\220\333\303PU\272\35\20ah\323\320\276\227\220\63e\302\336&\300\205\246\364\310\231}\323\207\nG\200\246\362k\223*\336Qd\\\362u\350\241H\316\221\r\3\356\343\31Ws+\342n\244I\235\302\375M\352\243\356\346q\265w\360\223\307\326\334\244)\36\324D\317\330\345\366\60\364\17o\0\\D>\322\234\276\205\34;1\375\226\313\230\2D\344B\220\63\207Dw\27\216\0\255\301\263\263\303\327\24i\226\216A\17\205\0j\\\361\361\226\322\273\252\230\224\274:Pj\213[\306{\370\224\310\372\r\264CF\376\215\374G\371/\7Im\223\222\327\324\316,\266g@e\f\224Mn\17\303\60\347j@'j\36>+$\237B\321\22\70m\242\235\240\24\206\320!\7\66\371\16G\371\201\32q\263\361\274\20`\276p|\251xF\221i\246\275\320s\361\350J\346\372.\315"
	"\223.\367 \365\316\35\262\212\240\62F\302\"\267\207a\254a\t\250\311\60\303\34\371\24\222\26\343\16\6t\n\2\200\16?P;\243\370\5y!\300\n\342`\33q\\\221y\304V\350\321\364\347&\254\60\276\321S,0\34\65\223\16P\37\23\210r{\30\32\211y\240&403\211\365\362A$M0\237(\"\376\302~9(\311\22DQ\360O\362v\365<\333\266m\333\66\327\266m[\37\271\67\264x\254Q\357\240\62N\240\215S\331\220\277\263\\V\350j\201\345\215\344$\204\0\65\214\64\63\363\337\24\321!K\212'F+lb&\370\324\62\266W\16KD\254c\243\315\355\325| _\302d#[\253\31\266\334\177\23\323wy\365/\1X\241\253\5\226\267d;\1\302\303'\364\201\374 K\322T3\264\227\303\242$\236\0\312Q\316\66\61m\357\300\t.8\1\222U\0\305\303\323\307X#S\301\224\224C\203>eEIT\1\224\373\274\261\277\242:\306\266\263\307\t\220\300\2X\260\207Cy4\307\265\236ZF\257\363\330\340\213\222\330\2x\230\357T\330_Z\t]\253Y\353\4Hb\1\24\17\357\64W\363\343\240A\25C\367x3?D\22^\0e5\353\312\350\265\277\306\37T\32|'@\342\v`\365\66x\312\27\213\372\377\23\275\17\357\311;\301\305\20!\261'\376\2\330\337\207\6&\355\257\367\31_\305\t\240\234\342\322\31\256\376\67\307\70'\211\315\26v\234\343\206\312\360\223\252rz\233\230\372\367\357`%\3\231\324\276\344\347e\356\355b\277\301H\34X\306r\233\333\253\331\307Q\211\16\335\203\356\307>+X)\v0\30\235\31X\366r8\216\2\70\214\230\65\254W+v\262_\237\216\373\70\262\207\303:\320\267\260s-\33||q$>\1\b0\333>}\b\0\0\0A\0\362\267>\213\237\345\20\b\0\2\200\0 \0\b\0\2\200\0 \0\b\0\2\200\0 \0\364\ng\377uYDd\274\317\0\0\0\0IEND\256B`\202";
