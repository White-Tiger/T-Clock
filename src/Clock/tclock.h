//===============================================================================
//--+++--> tclock.h - KAZUBON  1997-1999 =========================================
//=================== Last Modified by Stoic Joker: Sunday, 03/13/2011 @ 11:54:05am
//-------------------------{ Stoic Joker 2006-2010 }-------------------------+++-->
#pragma once

#include "../common/globals.h"
#include "../common/resource.h"

#include <stdio.h>//sprintf
#include <uxtheme.h>//SetWindowTheme
#include <shlwapi.h>//PathFileExists
#include <psapi.h>//EmptyWorkingSet
#include "../common/newapi.h"
#include "../common/utl.h"
#include "../common/control_extensions.h"

// IDs for timer
#define IDTIMER_MAIN              3
#define IDTIMER_MOUSE             4
#define IDTIMER_DEKSTOPICON       5
#define IDTIMER_DESKTOPICONSTYLE  6
#define IDTIMER_MONITOR           7

// for mouse.c and pagemouse.c
#define MOUSEFUNC_NONE           0
#define MOUSEFUNC_MENU           1
#define MOUSEFUNC_TIMER          5
#define MOUSEFUNC_CLIPBOARD      6
#define MOUSEFUNC_SCREENSAVER    7
#define MOUSEFUNC_SHOWCALENDER   8
#define MOUSEFUNC_SHOWPROPERTY   9

#define MOUSEFUNCEXTRA_BEGIN     -1
#define MOUSEFUNCEXTRAFILE_BEGIN -100
#define MOUSEFUNC_EXEC           -100

//--+++--> main.c - Application Global Values:
extern HWND g_hwndTClockMain; /**< our main window for hotkeys, menus and sounds */
extern HWND g_hwndClock;      /**< the clock hwnd */
extern HWND g_hDlgTimer;      /**< timer dialog handle */
extern HWND g_hDlgStopWatch;  /**< stopwatch dialog handle */
extern HWND g_hDlgTimerWatch; /**< timer watch dialog handle */
extern HWND g_hDlgSNTP;       /**< SNTP options dialog handle */
extern HWND g_hwndSheet;      /**< property sheet window */
extern int g_explorer_restarts; /**< number of detected Explorer restarts/crashes */
/** frequently used icon handles */
extern HICON g_hIconTClock, g_hIconPlay, g_hIconStop, g_hIconDel;
#ifdef WIN2K_COMPAT
/** make background of desktop icon text labels transparent:
 * (for Windows 2000 only)
 * UnAdvertized EasterEgg Function */
void SetDesktopIconTextBk(int enable);
#endif // WIN2K_COMPAT

/**
 * \brief returns full path to currently started Clock[64].exe
 * \param out \c wchar_t buffer of size \c MAX_PATH
 * \return \p out with path to Clock.exe incl. filename
 * \remark requires the API to be initialized \sa api */
wchar_t* GetClockExe(wchar_t out[MAX_PATH]);

void TranslateDispatchTClockMessage(MSG* msg);
void RegisterSession(HWND hwnd);
void UnregisterSession(HWND hwnd);
void ToggleCalendar(int type);
/**
 * \brief retrieves the startup file path if any
 * \param[out] filename path to startup file
 * \return boolean
 * \sa AddStartup(), RemoveStartup(), IsStartupAdded() */
int GetStartupFile(wchar_t filename[MAX_PATH]);
/**
 * \brief check whether a startup file is present and points to this T-Clock
 * \return boolean
 * \sa AddStartup(), RemoveStartup(), GetStartupFile() */
int IsStartupAdded(void);
/**
 * \brief adds T-Clock to "Startup" folder for automated launch on logon
 * \sa RemoveStartup(), IsStartupAdded(), GetStartupFile() */
void AddStartup(void);
/**
 * \brief removes the startup file
 * \sa AddStartup(), IsStartupAdded(), GetStartupFile() */
void RemoveStartup(void);
/**
 * \brief read or write shell links (shortcuts)
 * \param[in] file shell link file name (output parameters are allowed to overlap \p file)
 * \param[in] read_len do a read operation with \p read_len characters per buffer (use \c 0 to write/create the link instead)
 * \param[in,out] target links target file
 * \param[in,out] params arguments for target file \e [optional]
 * \param[in,out] workingdir working directory "Start in" \e [optional]
 * \param[in,out] comment user / app created comment (description) \e [optional]
 * \param[in,out] icon path to preferred icon (or \c NULL for default) \e [optional]
 * \param[in,out] icon_index index of the icon \e [optional]
 * \param[in,out] show \c one of ShowWindow() values \e [optional] \sa SW_*
 * \param[in,out] hotkey virtual key code in low-order, HOTKEYF_* modifier flags in high-order byte \e [optional] \sa HOTKEYF_*
 * \return boolean. if any string value couldn't be read, it'll result in an empty string. */
int WindowsShellLink(wchar_t* file, int read_len, wchar_t* target, wchar_t* params, wchar_t* workingdir, wchar_t* comment, wchar_t* icon, int* icon_index, int* show, WORD* hotkey);

// Macros
BOOL EnableDlgItemSafeFocus(HWND hDlg,int control,BOOL bEnable,int nextFocus);
#define EnableDlgItem(hDlg,id,b) EnableWindow(GetDlgItem((hDlg),(id)),(b))
#define ShowDlgItem(hDlg,id,b) ShowWindow(GetDlgItem((hDlg),(id)),(b)?SW_SHOW:SW_HIDE)

// used by PageMisc.c and main.c
extern const wchar_t kSectionImmersiveShell[56+1]; ///< SOFTWARE/Microsoft/Windows/CurrentVersion/ImmersiveShell
extern const wchar_t kKeyWin32Tray[27+1]; ///< UseWin32TrayClockExperience

// settings.c
int CheckSettings();

// propsheet.c
extern char g_bApplyClock;
extern char g_bApplyTaskbar;
void MyPropertySheet(int page);
/** wrapper for GetOpenFileName()
 * \param[in] hDlg parent window to be disabled during selection \e [optional]
 * \param[in] filter list of filters ( \c Name\0*.ext;*.ext2\0... ), terminated by \0\0
 * \param[in] filter_index index of selected filter in \a filter counting from 1 \e [optional]
 * \param[in] deffile initial (previously selected) file or directory \e [optional]
 * \param[out] retfile holds selected file; or "" on error
 * \return boolean, \p deffile will be non-empty on success
 * \sa GetOpenFileName() */
BOOL SelectMyFile(HWND hDlg, const wchar_t* filter, DWORD filter_index, const wchar_t* deffile, wchar_t retfile[MAX_PATH]);

// alarm.c
typedef struct Schedule Schedule;
struct Schedule {
	Schedule* prev;
	Schedule* next;
	time_t time;
	int id;
	unsigned data;
};

/**
 * \brief add a schedule to our timetable
 * \param id schedule ID, should have the \c SCHEDID_START_FLAG_ flag as everything else is used for alarms
 * \param ts target time_t or time offset from 0 to 86400 (1 day)
 * \param data schedule specific data (such as ALRM_* flags for hourly chime)
 * \return pointer to queued schedule or \c NULL on failure
 * \sa TimetableRemove(), AlarmEnable() */
Schedule* TimetableAdd(int id, time_t ts, unsigned data);
void TimetableRemove(int id);
void TimetableQueue(Schedule* alert, int add);
Schedule* TimetableSearchID(int id);

enum{
	SCHEDID_START_FLAG_ = 0x80000000,
	SCHEDID_CHIME,
	SCHEDID_UPDATE,
	SCHEDID_WIN2K,
};

#define MAX_ALARM 999999
enum{
	DAYF_MONDAY   = 0x01, DAYF_TUESDAY = 0x02, DAYF_WEDNESDAY = 0x04,
	DAYF_THURSDAY = 0x08, DAYF_FRIDAY  = 0x10, DAYF_SATURDAY  = 0x20, DAYF_SUNDAY = 0x40,
	DAYF_EVERYDAY_BITMASK = /*0x7f*/ (DAYF_MONDAY | DAYF_TUESDAY | DAYF_WEDNESDAY | DAYF_THURSDAY | DAYF_FRIDAY | DAYF_SATURDAY | DAYF_SUNDAY),
	DAYF_OVERFLOW = 0x80,
};
#define DAYF(x) (1 << (x))
#define DAYF_FromWDay(x) (((x) > 0 ? DAYF((x)-1) : DAYF_SUNDAY))
enum{
	ALRM_ENABLED =0x00000001,
	ALRM_ONESHOT =0x00000002,
	ALRM_12H     =0x00000004,
	ALRM_CHIMEHR =0x00000010,
	ALRM_REPEAT  =0x00000020,
	ALRM_BLINK   =0x00000040,
	ALRM_DIALOG  =0x00000080,
};
typedef struct{
	wchar_t name[TNY_BUFF];
	wchar_t message[512];
	wchar_t settings[TNY_BUFF];
} dlgmsg_t;
typedef struct alarm_t {
	int days;
	int hour;
	int minute;
	int day;
	int month;
	int iTimes;
	dlgmsg_t dlgmsg;
	unsigned char uFlags;
	wchar_t fname[MAX_PATH];
} alarm_t;

/**
 * \brief enable / disable alarm
 * \param idx alarm id
 * \param enable 1: enable, 0: disable, -1: toggle, -2: read from reg */
void AlarmEnable(int idx, int enable);
/**
 * \brief enable / disable hourly chime
 * \param enable 1: enable, 0: disable, -1: toggle, -2: read from reg */
void AlarmChimeEnable(int enable);

int GetAlarmNum();
void SetAlarmNum(int num);
void ReadAlarmFromReg(alarm_t* pAS, int idx);
void SaveAlarmToReg(alarm_t* pAS, int idx);
int DeleteAlarmFromReg(int idx);

void StopFile();
int IsPlaying();
void EndAlarm();
void InitAlarm();
int OnMCINotify(HWND hwnd);
void OnTimerAlarm(HWND hwnd, time_t time);
/** \brief play sound or execute program \p fname
 * \param[in] hwnd
 * \param[in] fname sound to be played, or program to be executed. Can be relative to \e TCLOCK/waves/
 * \param[in] dwLoops number of times to repeat
 * \return boolean (started to play a sound) */
int PlayFile(HWND hwnd, const wchar_t* fname, DWORD dwLoops);

// alarmday.c
#define ALARMDAY_OKFLAG 0x80000000
int ChooseAlarmDay(HWND hDlg, unsigned days);

// soundselect.c
/**
 * \brief adds a list of sound files from \e /waves/ to given combobox controls
 * \param boxes[] array of combobox controls
 * \param num number of controls in \a boxes
 * \remark first entry is always "&lt;  no sound  &gt;" and will be selected if combobox is empty */
void ComboBoxArray_AddSoundFiles(HWND boxes[], int num);
BOOL IsMMFile(const wchar_t* fname);
BOOL BrowseSoundFile(HWND hDlg, const wchar_t* deffile, wchar_t* fname);

// pageformat.c
void InitFormat();

// menu.c
LRESULT OnTClockCommand(HWND hwnd, WPARAM wParam);
/**
 * \brief Shows the context menu
 * \param hwnd menu's parent window (eg. hidden T-Clock window)
 * \param hwnd_from the current / previous parent to focus on closing the menu (eg. the clock)
 * \param xPos mouse pointer X coordinate for menu positioning
 * \param yPos mouse pointer Y coordinate for menu positioning
 * \remark set \p xPos and \p yPos to \c -1 to use the current cursor position.
 * \sa TrackPopupMenu() */
void OnContextMenu(HWND hwnd, HWND hwnd_from, int xPos, int yPos);

// mouse.c
void OnTimerMouse(HWND hwnd);
void OnMouseMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
// PageMouse.c
void CheckMouseMenu();

// timer.c
void UpdateTimerMenu(HMENU hMenu);

/**
 * \brief enable / disable timer
 * \param id timer id (or \c -1 for all)
 * \param enable 1: enable, 0: disable, -1: toggle */
void TimerEnable(int id, int enable);
/**
 * \brief process menu item click on a timer
 * \param hmenu menu handle
 * \param itemid id of clicked timer item */
void TimerMenuItemClick(HMENU hmenu, int itemid);

void WatchTimer(int reset);

void EndAllTimers();
void DialogTimer(int select_id);
void OnTimerTimer(HWND hwnd);

// StopWatch.c
BOOL IsDialogStopWatchMessage(HWND hwnd, MSG* msg);
void DialogStopWatch();
void StopWatch_Start(HWND hDlg);
void StopWatch_Stop(HWND hDlg);
void StopWatch_Reset(HWND hDlg);
void StopWatch_Pause(HWND hDlg);
void StopWatch_Resume(HWND hDlg);
void StopWatch_Lap(HWND hDlg,int bFromStop);
void StopWatch_TogglePause(HWND hDlg);

// ExitWindows.c
/**
 * \brief Helper function that calls \c ExitWindowsEx() to Shutdown, Reboot or LogOff Windows (requests \c SE_SHUTDOWN_NAME privilege, except for \c EWX_LOGOFF)
 * \param ewx_type is the \c uFlags parameter of \c ExitWindowsEx(). e.g. \c EWX_LOGOFF, \c EWX_REBOOT, \c EWX_SHUTDOWN
 * \return bool
 * \remark it adds the \c EWX_FORCEIFHUNG flag to ensure shutdown without further user interaction
 * \sa EWX_LOGOFF, EWX_REBOOT, EWX_SHUTDOWN, WindowsSleep(), ExitWindowsEx() */
int WindowsExit(int ewx_type);
/**
 * \brief Helper function that calls \c SetSystemPowerState() to switch into sleep or hibernation (requests \c SE_SHUTDOWN_NAME privilege)
 * \param standby \c 1 = standby, \c 0 = hibernate
 * \return bool
 * \sa WindowsExit(), SetSystemPowerState() */
int WindowsSleep(int standby);

// PageHotKey.c
typedef union {
	struct {
		uint8_t vk;
		uint8_t fsMod;
	} key;
	uint16_t word;
} hotkey_t;
hotkey_t GetHotkey(int idx);
void SetHotkey(int idx, hotkey_t hotkey);
void HotkeyBox_Init(HWND hDlg, int idx);
hotkey_t HotkeyBox_GetValue(HWND box);
void HotkeyBox_SetValue(HWND box, hotkey_t hotkey);
/**
 * \brief (un-)registers hotkeys
 * \param hwnd target hotkey window
 * \param want_register \c 0 to remove them, \c 1 to add them, \c 2 for re-registration
 * \sa RegisterHotKey(), UnregisterHotKey() */
void RegisterHotkeys(HWND hwnd, int want_register);
/**
 * \brief processes \c WM_HOTKEY messages (feature requested from \c eweoma at \c DonationCoder.com)
 * \param hwnd receiving window
 * \param wParam received wParam
 * \param lParam received lParam
 * \sa RegisterHotKey(), UnregisterHotKey() */
LRESULT HotkeyMessage(HWND hwnd, WPARAM wParam, LPARAM lParam);

// SNTP.c
void SyncTimeNow();
void NetTimeConfigDialog(int justElevated);

// BounceWind.c
int BounceWindOptions(HWND hDlg, dlgmsg_t* dlg);
void ReleaseTheHound(HWND hwnd, const wchar_t* title, const wchar_t* text, wchar_t* settings);

