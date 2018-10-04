#include "../common/version.h"
#include "tclock.h"
#include <time.h>
#include <stdio.h>
#include <dsound.h>

static wchar_t g_alarmkey[6+6] = L"Alarm";
#define ALARMKEY_OFFSET 5

static Schedule* timetable_begin_ = NULL;
static Schedule* timetable_end_ = NULL;

static time_t AlarmNextTimestamp();
static void AlarmGetMSG(dlgmsg_t* msg);

/**
 * \brief get our update check interval based on \c VER_TIMESTAMP (with rarer update checks with increasing age)
 * \param time current time
 * \return update interval
 * \sa VER_TIMESTAMP */
static int GetUpdateInterval(time_t time) {
	time_t diff = (time - VER_TIMESTAMP);
	if(diff < 90*24*3600) {
		if(diff < 21*24*3600) {
			if(diff < 7*24*3600) {
				if(diff < 24*3600) {
					return 3*3600;
				}
				return 24*3600;
			}
			return 2*24*3600;
		}
		return 7*24*3600;
	}
	return 14*24*3600;
}

static void TimetableClean() {
	Schedule* iter = timetable_begin_;
	timetable_begin_ = timetable_end_ = NULL;
	
	while(iter) {
		Schedule* tmp = iter;
		iter = iter->next;
		free(tmp);
	}
	return;
}
Schedule* TimetableAdd(int id, time_t ts, unsigned data) {
	Schedule* schedule;
	schedule = TimetableSearchID(id);
	if(schedule) {
		TimetableQueue(schedule, 0);
	} else {
		schedule = (Schedule*)malloc(sizeof(Schedule));
		if(!schedule)
			return NULL;
		schedule->id = id;
		schedule->time = 0;
	}
	if(ts != -1) {
		if(ts <= 86400)
			ts = time(NULL) + ts;
		schedule->time = ts;
	}
	schedule->data = data;
	TimetableQueue(schedule, 1);
	return schedule;
}
void TimetableRemove(int id) {
	Schedule* schedule = TimetableSearchID(id);
	if(schedule) {
		TimetableQueue(schedule, 0);
		free(schedule);
	}
}
void TimetableQueue(Schedule* alert, int add) {
	if(add) {
		Schedule* iter = timetable_begin_;
		if(!iter) {
			alert->prev = alert->next = NULL;
add_chain_link:
			if(alert->prev)
				alert->prev->next = alert;
			else
				timetable_begin_ = alert;
			if(alert->next)
				alert->next->prev = alert;
			else
				timetable_end_ = alert;
		} else if(alert->time < (iter->time + 43200)) {
			// forward search (eg. hourly chime, win2k timer)
			for(;;) {
				if(alert->time < iter->time) {
					alert->prev = iter->prev;
					alert->next = iter;
					goto add_chain_link;
				}
				iter = iter->next;
				if(!iter) {
					alert->prev = timetable_end_;
					alert->next = NULL;
					goto add_chain_link;
				}
			}
		} else {
			// backward search (any alarm)
			iter = timetable_end_;
			for(;;) {
				if(alert->time >= iter->time) {
					alert->prev = iter;
					alert->next = iter->next;
					goto add_chain_link;
				}
				iter = iter->prev;
				if(!iter) {
					alert->prev = NULL;
					alert->next = timetable_begin_;
					goto add_chain_link;
				}
			}
		}
	} else {
		if(alert->prev)
			alert->prev->next = alert->next;
		else
			timetable_begin_ = alert->next;
		if(alert->next)
			alert->next->prev = alert->prev;
		else
			timetable_end_ = alert->prev;
	}
}
Schedule* TimetableSearchID(int id) {
	Schedule* iter = timetable_end_;
	for(; iter && iter->id!=id; iter=iter->prev);
	return iter;
}

static WAVEHDR m_wh;
static HPSTR m_pData = NULL;
static HWAVEOUT m_hWaveOut = NULL;
static WAVEFORMATEX* m_pFormat = NULL;

static DWORD m_countPlay=0;
static BOOL m_bMCIPlaying = FALSE;
char g_bPlayingNonstop = 0;
static BOOL m_bTrack;
static int m_nTrack;

static int m_maxAlarm = 0;

typedef struct _stPCBEEP {
	HWND hWnd;
	DWORD dwLoops;
	wchar_t szFname[MAX_PATH];
} PCBEEP;

static volatile char m_bKillPCBeep = 1;

int PlayWave(HWND hwnd, const wchar_t* fname, DWORD dwLoops);
int PlayMCI(HWND hwnd, int nt);
void StopWave(void);

void AlarmChimeEnable(int enable) {
	if(enable < 0) {
		int status = api.GetInt(L"", L"Jihou", 0);
		if(enable == -1) {
			enable = !status;
			goto update_and_continue;
		}
		enable = status;
	} else {
update_and_continue:
		api.SetInt(L"", L"Jihou", enable);
	}
	if(enable) {
		unsigned data = 0;
		time_t ts = time(NULL) - 1;
		ts += 3600 - (ts % 3600);
		if(api.GetInt(L"", L"JihouRepeat", 0))
			data |= ALRM_CHIMEHR;
		if(api.GetInt(L"", L"JihouBlink", 0))
			data |= ALRM_BLINK;
		TimetableAdd(SCHEDID_CHIME, ts, data);
	} else
		TimetableRemove(SCHEDID_CHIME);
}
void AlarmEnable(int idx, int enable) {
	Schedule* alarm;
	if(idx < 0 || idx >= m_maxAlarm)
		return;
	_ltow(idx+1, (g_alarmkey+ALARMKEY_OFFSET), 10);
	
	if(enable < 0) {
		int status = api.GetInt(g_alarmkey, L"Alarm", 0);
		if(enable == -1) {
			enable = !status;
			goto update_and_continue;
		}
		enable = status;
		alarm = NULL;
	} else {
update_and_continue:
		api.SetInt(g_alarmkey, L"Alarm", enable);
		alarm = TimetableSearchID(idx);
	}
	if(!enable) {
		if(alarm) {
			TimetableQueue(alarm, 0);
			free(alarm);
		}
		return;
	}
	if(!alarm) {
		alarm = (Schedule*)malloc(sizeof(Schedule));
		if(!alarm)
			return;
		alarm->time = AlarmNextTimestamp();
		alarm->id = idx;
		/// @note : on next backward incompatible change, store a single value including every "flag"
		alarm->data = 0;
		if(api.GetInt(g_alarmkey,L"Alarm",0)) alarm->data |= ALRM_ENABLED;
		if(api.GetInt(g_alarmkey,L"Once",0)) alarm->data |= ALRM_ONESHOT;
		if(api.GetInt(g_alarmkey,L"12h",0)) alarm->data |= ALRM_12H;
		if(api.GetInt(g_alarmkey,L"ChimeHr",0)) alarm->data |= ALRM_CHIMEHR;
		if(api.GetInt(g_alarmkey,L"Repeat",0)) alarm->data |= ALRM_REPEAT;
		if(api.GetInt(g_alarmkey,L"Blink",0)) alarm->data |= ALRM_BLINK;
		if(api.GetInt(g_alarmkey,L"jrMsgUsed",0)) alarm->data |= ALRM_DIALOG;
		TimetableQueue(alarm, 1);
	}
}
int GetAlarmNum()
{
	int num = api.GetInt(L"", L"AlarmNum", 0);
	if(num < 0)
		num = 0;
	if(num > MAX_ALARM)
		num = MAX_ALARM;
	return num;
}
void SetAlarmNum(int num)
{
	if(num > MAX_ALARM)
		num = MAX_ALARM;
	api.SetInt(L"", L"AlarmNum", num);
}
void AlarmGetMSG(dlgmsg_t* msg) {
	api.GetStr(g_alarmkey, L"Name", msg->name, _countof(msg->name), L"");
	api.GetStr(g_alarmkey, L"jrMessage", msg->message, _countof(msg->message), L"");
	api.GetStr(g_alarmkey, L"jrSettings", msg->settings, _countof(msg->settings), L"");
	
	if(!msg->name[0]) {
		int hour = api.GetInt(g_alarmkey, L"Hour", 12) % 24;
		int minute = api.GetInt(g_alarmkey, L"Minute", 0);
		wsprintf(msg->name, FMT("%02d:%02d"), hour, minute);
	}
}

time_t AlarmNextTimestamp() {
	time_t ts;
	struct tm tm;
	int hour, minute, days, dayf;
	
	hour = api.GetInt(g_alarmkey, L"Hour", 12) % 24;
	minute = api.GetInt(g_alarmkey, L"Minute", 0);
	days = api.GetInt(g_alarmkey, L"Days", 0);
	
	ts = time(NULL);
	localtime_r(&ts, &tm);
	dayf = DAYF_FromWDay(tm.tm_wday);
	
	if(hour < tm.tm_hour || (hour == tm.tm_hour && minute <= tm.tm_min)) {
		++tm.tm_mday;
		dayf <<= 1;
		if(dayf & DAYF_OVERFLOW)
			dayf = DAYF_MONDAY;
	}
	if(days & DAYF_EVERYDAY_BITMASK) {
		while(!(days & dayf)) {
			++tm.tm_mday;
			dayf <<= 1;
			if(dayf & DAYF_OVERFLOW)
				dayf = DAYF_MONDAY;
		}
	}
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = 0;
	

	if (api.GetInt(g_alarmkey, L"Once", 0) && api.GetInt(g_alarmkey, L"Day", 0) && api.GetInt(g_alarmkey, L"Month", 0)) {
		int month = api.GetInt(g_alarmkey, L"Month", 0) - 1;
		if (tm.tm_mon > month) ++tm.tm_year;
		tm.tm_mday = api.GetInt(g_alarmkey, L"Day", 0);
		tm.tm_mon = month;
	}

	return mktime(&tm);
}
void ReadAlarmFromReg(alarm_t* pAS, int idx)
{
	if(idx >= MAX_ALARM)
		return;
	_ltow(idx+1, (g_alarmkey+ALARMKEY_OFFSET), 10);
	
	pAS->hour = api.GetInt(g_alarmkey, L"Hour", 12) % 24;
	pAS->minute = api.GetInt(g_alarmkey, L"Minute", 0);
	pAS->day = api.GetInt(g_alarmkey, L"Day", 0);
	pAS->month = api.GetInt(g_alarmkey, L"Month", 0);
	pAS->days = api.GetInt(g_alarmkey, L"Days", 0);
	pAS->iTimes = api.GetInt(g_alarmkey, L"Times", 1);
	
	/// @note : on next backward incompatible change, store a single value including every "flag"
	pAS->uFlags=0;
	if(api.GetInt(g_alarmkey,L"Alarm",0)) pAS->uFlags|=ALRM_ENABLED;
	if(api.GetInt(g_alarmkey,L"Once",0)) pAS->uFlags|=ALRM_ONESHOT;
	if(api.GetInt(g_alarmkey,L"12h",0)) pAS->uFlags|=ALRM_12H;
	if(api.GetInt(g_alarmkey,L"ChimeHr",0)) pAS->uFlags|=ALRM_CHIMEHR;
	if(api.GetInt(g_alarmkey,L"Repeat",0)) pAS->uFlags|=ALRM_REPEAT;
	if(api.GetInt(g_alarmkey,L"Blink",0)) pAS->uFlags|=ALRM_BLINK;
	if(api.GetInt(g_alarmkey,L"jrMsgUsed",0)) pAS->uFlags|=ALRM_DIALOG;
	
	api.GetStr(g_alarmkey, L"File", pAS->fname, _countof(pAS->fname), L"");
	
	AlarmGetMSG(&pAS->dlgmsg);
}
void SaveAlarmToReg(alarm_t* pAS, int idx)
{
	if(idx >= MAX_ALARM)
		return;
	wsprintf(g_alarmkey+ALARMKEY_OFFSET, FMT("%d"), idx+1);
	api.SetStr(g_alarmkey, L"Name", pAS->dlgmsg.name);
	api.SetInt(g_alarmkey, L"Hour", pAS->hour);
	api.SetInt(g_alarmkey, L"Minute", pAS->minute);
	api.SetInt(g_alarmkey, L"Day", pAS->day);
	api.SetInt(g_alarmkey, L"Month", pAS->month);
	if(pAS->fname[0] == '<')
		pAS->fname[0] = '\0';
	api.SetStr(g_alarmkey, L"File", pAS->fname);
	
	api.SetStr(g_alarmkey, L"jrMessage", pAS->dlgmsg.message);
	api.SetStr(g_alarmkey, L"jrSettings", pAS->dlgmsg.settings);
	
	api.SetInt(g_alarmkey, L"Days", pAS->days);
	api.SetInt(g_alarmkey, L"Times", pAS->iTimes);
	
	api.SetInt(g_alarmkey,L"Alarm",pAS->uFlags&ALRM_ENABLED);
	api.SetInt(g_alarmkey,L"Once",pAS->uFlags&ALRM_ONESHOT);
	api.SetInt(g_alarmkey,L"12h",pAS->uFlags&ALRM_12H);
	api.SetInt(g_alarmkey,L"ChimeHr",pAS->uFlags&ALRM_CHIMEHR);
	api.SetInt(g_alarmkey,L"Repeat",pAS->uFlags&ALRM_REPEAT);
	api.SetInt(g_alarmkey,L"Blink",pAS->uFlags&ALRM_BLINK);
	api.SetInt(g_alarmkey,L"jrMsgUsed",pAS->uFlags&ALRM_DIALOG);
}
int DeleteAlarmFromReg(int idx)
{
	if(idx >= MAX_ALARM)
		return 1;
	wsprintf(g_alarmkey+ALARMKEY_OFFSET, FMT("%d"), idx+1);
	if(api.GetInt(g_alarmkey, L"Hour", -1) == -1)
		return 1;
	api.DelKey(g_alarmkey);
	return 0;
}

//=============================================================================================
//----------------------------------------------+++--> Load Configured Alarm Data From Registry:
void InitAlarm()   //--------------------------------------------------------------------+++-->
{
	time_t ts;
	time_t next_update;
	time_t diff;
	int update_interval;
	TimetableClean();
	m_maxAlarm = GetAlarmNum();
	if(m_maxAlarm > 0) {
		int i;
		for(i=0; i<m_maxAlarm; ++i) {
			AlarmEnable(i, -2);
		}
	}
	
	AlarmChimeEnable(-2);
	// this is for Windows 2000 only - EasterEgg function:
#	ifdef WIN2K_COMPAT
	if(api.OS == TOS_2000 && api.GetIntEx(L"Desktop",L"Transparent2kIconText",0)) {
		TimetableAdd(SCHEDID_WIN2K, 30, 30);
	}
#	endif // WIN2K_COMPAT
	// update check (always added, checks "enabled" on trigger)
	ts = time(NULL) + 300;
	next_update = api.GetInt64(NULL, UPDATE_TIMESTAMP, 0);
	if(!next_update) {
		// don't overwrite timestamp as required for proper first-time startup
		//next_update = ts + 86400;
		//api.SetInt64(NULL, UPDATE_TIMESTAMP, next_update);
		next_update = ts + 1200; // in 25 minutes
	}
	
	// see also OnTimerAlarm() case SCHEDID_UPDATE
	update_interval = GetUpdateInterval(ts);
	diff = (next_update - ts);
	if(diff > (update_interval + update_interval/4)) {
		next_update -= (diff - (diff % update_interval));
	}
	
	if(ts > next_update)
		TimetableAdd(SCHEDID_UPDATE, ts, (unsigned)(ts-next_update));
	else
		TimetableAdd(SCHEDID_UPDATE, next_update, 0);
}
//================================================================================================
//---------------------//--------------------------------------+++--> Shut Off the God Damn Siren!:
void EndAlarm(void)   //--------------------------------------------------------------------+++-->
{
	StopFile();
	TimetableClean();
}

//============================================== This Code Assumes...	===========================
//-----------------------------------------+++--> 12pm = Noon <--> However (See Below for Details):
void OnTimerAlarm(HWND hwnd, time_t time)   // 12am = Midnight --------------------------+++-->
{
	union {
		struct {
			wchar_t fname[MAX_PATH];
			int repeat;
			SYSTEMTIME st;
		} s;
		dlgmsg_t msg;
	} u;
	#define fname_u1 u.s.fname
	#define repeat_u1 u.s.repeat
	#define st_u1 u.s.st
	#define msg_u2 u.msg
	Schedule* schedule = timetable_begin_;
	if(!schedule)
		return;
	
	if(time >= schedule->time) {
		for(; schedule && time >= schedule->time; schedule=timetable_begin_) {
			TimetableQueue(schedule, 0);
			if(schedule->id & SCHEDID_START_FLAG_) {
				switch(schedule->id) {
				case SCHEDID_CHIME:
					if(schedule->data & ALRM_BLINK)
						PostMessage(g_hwndClock, CLOCKM_BLINK, TRUE, 0);
					api.GetStr(L"", L"JihouFile", fname_u1, _countof(fname_u1), L"");
					if(fname_u1[0]) {
						if(schedule->data & ALRM_CHIMEHR) { // chime hour
							GetLocalTime(&st_u1); // faster than localtime(&time)
							repeat_u1 = st_u1.wHour;
							if(repeat_u1 == 0)
								repeat_u1 = 12;
							else if(repeat_u1 > 12)
								repeat_u1 -= 12;
							--repeat_u1;
						} else {
							repeat_u1 = 0;
						}
						PlayFile(hwnd, fname_u1, repeat_u1);
					}
					schedule->time += 3600;
					break;
				case SCHEDID_UPDATE:
					if(!api.GetInt(NULL, UPDATE_RELEASE, 1)) {
						schedule->time = 0;
						break;
					}
					// see also InitAlarm()
					repeat_u1 = GetUpdateInterval(time);
					schedule->time += repeat_u1;
					schedule->time -= schedule->data;
					schedule->data = 0;
					if(time > schedule->time)
						schedule->time += (((time - schedule->time) / repeat_u1) * repeat_u1) + repeat_u1;
					if(repeat_u1 > 24*3600) // store only even 24h
						api.SetInt64(NULL, UPDATE_TIMESTAMP, schedule->time);
					api.ShellExecute(NULL, L"misc\\Options", L"-unotify", NULL, SW_HIDE, NULL);
					break;
#				ifdef WIN2K_COMPAT
				case SCHEDID_WIN2K:
					SetDesktopIconTextBk(1);
					schedule->time += schedule->data;
					break;
#				endif // WIN2K_COMPAT
				default:
					schedule->time = 0;
				}
			} else {
				_ltow(schedule->id+1, (g_alarmkey+ALARMKEY_OFFSET), 10);
				
				if(schedule->data & ALRM_BLINK)
					PostMessage(g_hwndClock,CLOCKM_BLINK,FALSE,0);
				if(schedule->data & ALRM_DIALOG) { // From BounceWindow.c
					AlarmGetMSG(&msg_u2);
					ReleaseTheHound(hwnd, msg_u2.name, msg_u2.message, msg_u2.settings);
				}
				api.GetStr(g_alarmkey, L"File", fname_u1, _countof(fname_u1), L"");
				if(fname_u1[0]) {
					if(schedule->data & ALRM_REPEAT) {
						repeat_u1 = api.GetInt(g_alarmkey, L"Times", 1) - 1;
						if(repeat_u1 < 0)
							repeat_u1 = -1;
					}else if(schedule->data & ALRM_CHIMEHR) { // chime hour
						GetLocalTime(&st_u1); // faster than localtime(&time)
						repeat_u1 = st_u1.wHour;
						if(repeat_u1 == 0)
							repeat_u1 = 12;
						else if(repeat_u1 > 12)
							repeat_u1 -= 12;
						--repeat_u1;
					}else
						repeat_u1 = 0;
					PlayFile(hwnd, fname_u1, repeat_u1);
				}
				if(schedule->data & ALRM_ONESHOT) {
					api.SetInt(g_alarmkey, L"Alarm", 0);
					schedule->time = 0;
				} else {
					schedule->time = AlarmNextTimestamp();
				}
			}
			if(!schedule->time) {
				free(schedule);
			} else {
				TimetableQueue(schedule, 1);
			}
		}
		#ifndef _DEBUG
		if(IsPlaying()) {
			EmptyWorkingSet(GetCurrentProcess());
		}
		#endif
	}
}
//================================================================================================
//-----+++--> For Computers WithOut Speakers, Play a NoSound (PC Beep) File Through the PC Speaker:
BOOL PlayNoSound(const wchar_t* fname, DWORD iLoops)   //-----------------------------------+++-->
{
	static const char seps[] = ", 	\r\n";
	char* szToken,*nxToken;
	#define dummy_channel 2
	#define dummy_sample_rate 44100 // 8.0 kHz, 11.025 kHz, 22.05 kHz, or 44.1 kHz
	#define dummy_bits_per_sample 16 // 8 or 16
	#define dummy_align (dummy_channel * dummy_bits_per_sample / 8)
	WAVEFORMATEX dummy_wavefmt = {WAVE_FORMAT_PCM, dummy_channel, dummy_sample_rate, (dummy_sample_rate*dummy_align), dummy_align, dummy_bits_per_sample, 0};
	DSBUFFERDESC dummy_dsbdesc = {sizeof(DSBUFFERDESC), DSBCAPS_GLOBALFOCUS, DSBSIZE_MIN*256, 0, NULL/*&dummy_wavefmt*/ /*, GUID_NULL*/};
	IDirectSound* dummy_ds = NULL;
	IDirectSoundBuffer* dummy_ds_buf = NULL;
	FILE* file = _wfopen(fname, L"r");
	
	dummy_dsbdesc.lpwfxFormat = &dummy_wavefmt;
	dummy_dsbdesc.guid3DAlgorithm = GUID_NULL;
	
	if(!file) {
		m_bKillPCBeep = 1;
		return FALSE;
	}
	
	// workaround 7+ (Beep() got nasty clicks&pops without other sounds playing)
	DirectSoundCreate(0, &dummy_ds, 0);
	if(dummy_ds) {
		dummy_ds->lpVtbl->SetCooperativeLevel(dummy_ds, g_hwndTClockMain, DSSCL_NORMAL);
		dummy_ds->lpVtbl->CreateSoundBuffer(dummy_ds, &dummy_dsbdesc, &dummy_ds_buf, 0);
		if(dummy_ds_buf) {
			void* ptr1;
			DWORD ptr1size;
			if(dummy_ds_buf->lpVtbl->Lock(dummy_ds_buf, 0, 0, &ptr1,&ptr1size, NULL,0, DSBLOCK_ENTIREBUFFER) == DS_OK) {
				memset(ptr1, 0, ptr1size); // silence
				dummy_ds_buf->lpVtbl->Unlock(dummy_ds_buf, ptr1,ptr1size, NULL,0);
				dummy_ds_buf->lpVtbl->Play(dummy_ds_buf, 0, 0, DSBPLAY_LOOPING);
				// if we stop right here, we'll only get a 20sec time-frame
//				dummy_ds_buf->lpVtbl->Stop(dummy_ds_buf);
			}
		}
	}
	
	while(!m_bKillPCBeep) {
		// If We Have a Line, Play Its Beep!
		char szTmp[TNY_BUFF];
		while(fgets(szTmp, _countof(szTmp), file)) {
			unsigned duration = 10, freq = 0, i;
			szToken = strtok_s(szTmp, seps, &nxToken);
			for(i=0; szToken; ++i) {
				int num = atoi(szToken);
				switch(i) {
				case 0: // Length of Beep
					if(num < 10) duration = 10;
					else duration = num;
					break;
				case 1: // Frequency
					if(num < 0) freq = 0;
					else freq = num;
					break;
				}
				szToken = strtok_s(NULL, seps, &nxToken);
			} // Get Next Beep Line.
			if(duration > 3600000) // don't allow a duration longer than 1 hour
				duration = 3600000;
			if(m_bKillPCBeep) {  // Just in case It's a Long File...
				break; //           Check for Kill Code between Beeps.
			} else if(!freq) {
				Sleep(duration);
			} else {
				Beep(freq, duration);
			}
		} // END OF IF LINE
		
		if(iLoops > 0) { //-> IF We're Looping, Go Back to
			if(ftell(file) <= 0 && !m_bKillPCBeep) // empty file, don't do an "idle loop" (high CPU usage)
				Sleep(100);
			fseek(file, 0, SEEK_SET); // Beginning of File
			--iLoops;
		}else{ // Or Die, Exit, Quit, Close, End
			m_bKillPCBeep = 1;
		}
	} // END OF WHILE NOT KILL BEEP
	fclose(file); // Make Sure File Gets Closed on Early Exit.
	// cleanup 7+ workaround
	if(dummy_ds_buf) {
		dummy_ds_buf->lpVtbl->Stop(dummy_ds_buf);
		dummy_ds_buf->lpVtbl->Release(dummy_ds_buf), dummy_ds_buf = NULL;
	}
	if(dummy_ds)
		dummy_ds->lpVtbl->Release(dummy_ds), dummy_ds = NULL;
	return TRUE;
}
#include <process.h> // Required for Worker Thread Creation - So Clock Can Send Alarm Kill Code.
//===============================================================================================
//--+++-->
unsigned __stdcall PlayNoSoundProc(void* param)
{
	PCBEEP* pcb = (PCBEEP*)param;
	
	PlayNoSound(pcb->szFname, pcb->dwLoops);
	SendMessage(pcb->hWnd, MM_WOM_DONE, 0, 0);
	
	free(pcb);
	_endthread();
	return 0;
}
//================================================================================================
//--+++-->
void PlayNoSoundThread(HWND hWnd, const wchar_t* fname, DWORD dwLoops)
{
	PCBEEP* pcb = (PCBEEP*)malloc(sizeof(PCBEEP));
	if(!pcb)
		return;
	pcb->hWnd = hWnd;
	pcb->dwLoops = dwLoops;
	wcsncpy_s(pcb->szFname, _countof(pcb->szFname), fname, _TRUNCATE);
	
	m_bKillPCBeep = 0;
	_beginthreadex(NULL, 0, PlayNoSoundProc, pcb, 0, 0);
}
//=================================================*
// ------------------------ Play Selected Alarm File
//===================================================*
int PlayFile(HWND hwnd, const wchar_t* fname, DWORD dwLoops)
{
	wchar_t file[MAX_PATH];
	if(!fname[0] || fname[0]=='<') return FALSE;
	if(fname[0]!='/' && fname[0]!='\\' && fname[1]!=':' // no abs path
	&&(fname[0]!='.' || (fname[1]!='/' && fname[1]!='\\' && (fname[1]!='.' ||  (fname[2]!='/' && fname[2]!='\\'))))) // no relative path (strict relative)
	{ // do it relative to "waves/"
		wchar_t unused[MAX_PATH];
		const int len = (int)wcslen(fname)+1; // incl. terminating null
		if(len < (MAX_PATH-api.root_len-7)){
			memcpy(file, api.root, api.root_size); // absolute path is at least required by .pcb / PlayNoSoundThread (not for .wav)
			memcpy(file+api.root_len, L"\\waves\\", (7*sizeof(file[0])));
			memcpy(file+api.root_len+7, fname, (len*sizeof(file[0])));
			if(api.GetFileAndOption(file, unused, NULL) == 0){
				fname = file;
			}
		}
	}
	
	if(ext_cmp(fname, L"wav") == 0) {
		if(m_bMCIPlaying)
			return FALSE;
		return PlayWave(hwnd, fname, dwLoops);
	}
	
	if(ext_cmp(fname, L"pcb") == 0) {
		if(m_bMCIPlaying)
			return FALSE;
		PlayNoSoundThread(hwnd, fname, dwLoops);
		return TRUE;
	}
	
	else if(IsMMFile(fname)) {
		wchar_t command[MIN_BUFF+MAX_PATH];
		if(m_bMCIPlaying)
			return FALSE;
		wcscpy(command, L"open \"");
		wcscat(command, fname);
		wcscat(command, L"\" alias myfile");
		if(mciSendString(command, NULL, 0, NULL) == 0) {
			wcscpy(command, L"set myfile time format ");
			if(wcscasecmp(fname, L"cdaudio") == 0 || ext_cmp(fname, L"cda") == 0) {
				wcscat(command, L"tmsf"); m_bTrack = TRUE;
			} else {
				wcscat(command, L"milliseconds"); m_bTrack = FALSE;
			}
			mciSendString(command, NULL, 0, NULL);
			
			m_nTrack = -1;
			if(ext_cmp(fname, L"cda") == 0) {
				const wchar_t* p;
				p = fname; m_nTrack = 0;
				while(*p) {
					if('0' <= *p && *p <= '9') m_nTrack = m_nTrack * 10 + *p - '0';
					p++;
				}
			}
			
			if(PlayMCI(hwnd, m_nTrack) == 0) {
				m_bMCIPlaying = TRUE;
				m_countPlay = dwLoops;
			} else mciSendString(L"close myfile", NULL, 0, NULL);
		} return m_bMCIPlaying;
	} else api.ExecFile(fname,hwnd);
	return FALSE;
}
//=================================================*
// ------------ Tie Into the Media Control Interface
//===================================================*
int PlayMCI(HWND hwnd, int nt)
{
	wchar_t command[80];
	
	wcscpy(command, L"play myfile");
	if(nt >= 0) {
		wchar_t cmdpos[64],out[32];
		wsprintf(cmdpos, FMT("status myfile position track %d"), nt);
		if(mciSendString(cmdpos, out, _countof(out), NULL) == 0) {
			wcscat(command, L" from ");
			wcscat(command, out);
			wsprintf(cmdpos+29, FMT("%d"), nt+1); // status myfile position track XXX
			if(mciSendString(cmdpos, out, _countof(out), NULL) == 0) {
				wcscat(command, L" to ");
				wcscat(command, out);
			}
		}
	}
	wcscat(command, L" notify");
	return mciSendString(command, NULL, 0, hwnd);
}
//=================================================*
// ----------------------- Stop Playing If/When Told
//===================================================*
void StopFile(void)
{
	m_bKillPCBeep = 1;
	StopWave();
	if(m_bMCIPlaying) {
		mciSendString(L"stop myfile", NULL, 0, NULL);
		mciSendString(L"close myfile", NULL, 0, NULL);
		m_bMCIPlaying = FALSE;
		m_countPlay = 0;
	}
	g_bPlayingNonstop = 0;
}
int IsPlaying()
{
	return m_hWaveOut || m_bMCIPlaying || !m_bKillPCBeep;
}
//=================================================*
// ----------------------------- Loop Play as Needed
//===================================================*
int OnMCINotify(HWND hwnd)
{
	if(m_bMCIPlaying) {
		if(m_countPlay) {
			mciSendString(L"seek myfile to start wait", NULL, 0, NULL);
			if(PlayMCI(hwnd, m_nTrack) == 0) {
				--m_countPlay;
				return 1;
			}
		}
		StopFile();
	}
	return 0;
}
//================================================================================================
//------------------------------------------------------//----+++--> Load & Play a Wave Audio File:
int PlayWave(HWND hwnd, const wchar_t* fname, DWORD dwLoops)   //-----------------------------+++-->
{
	MMCKINFO mmckinfoSubchunk;
	MMCKINFO mmckinfoParent;
	LONG lDataSize;
	LONG lFmtSize;
	HMMIO hmmio;
	
	if(m_hWaveOut != NULL)
		return FALSE;
	
	if((hmmio=mmioOpen((wchar_t*)fname, NULL, MMIO_READ|MMIO_ALLOCBUF))==0)
		return 0;
		
	mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if(mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF)) {
		mmioClose(hmmio, 0);
		return 0;
	}
	
	mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if(mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent,
				   MMIO_FINDCHUNK)) {
		mmioClose(hmmio, 0);
		return 0;
	}
	
	lFmtSize = mmckinfoSubchunk.cksize;
	m_pFormat = (WAVEFORMATEX*)malloc(lFmtSize);
	if(m_pFormat == NULL) {
		mmioClose(hmmio, 0);
		return 0;
	}
	
	if(mmioRead(hmmio, (HPSTR)m_pFormat, lFmtSize) != lFmtSize) {
		free(m_pFormat); m_pFormat = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	
	if(waveOutOpen(NULL, WAVE_MAPPER, m_pFormat,
				   0, 0, WAVE_FORMAT_QUERY)) {
		free(m_pFormat); m_pFormat = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	
	mmioAscend(hmmio, &mmckinfoSubchunk, 0);
	
	mmckinfoSubchunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if(mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent,
				   MMIO_FINDCHUNK)) {
		free(m_pFormat); m_pFormat = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	
	lDataSize = mmckinfoSubchunk.cksize;
	if(lDataSize == 0) {
		free(m_pFormat); m_pFormat = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	
	m_pData = (HPSTR)malloc(lDataSize);
	if(m_pData == NULL) {
		free(m_pFormat); m_pFormat = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	
	if(mmioRead(hmmio, m_pData, lDataSize) != lDataSize) {
		free(m_pFormat); m_pFormat = NULL;
		free(m_pData); m_pData = NULL;
		mmioClose(hmmio, 0);
		return 0;
	}
	mmioClose(hmmio, 0);
	
	if(waveOutOpen(&m_hWaveOut, WAVE_MAPPER,
				   m_pFormat, (DWORD_PTR)hwnd, 0,
				   CALLBACK_WINDOW)) {
		free(m_pFormat); m_pFormat = NULL;
		free(m_pData); m_pData = NULL;
		return 0;
	}
	
	memset(&m_wh, 0, sizeof(WAVEHDR));
	m_wh.dwBufferLength = lDataSize;
	m_wh.lpData = m_pData;
	if(dwLoops != 0) {
		if(dwLoops!=0xFFFFFFFF) ++dwLoops;
		m_wh.dwFlags = WHDR_BEGINLOOP|WHDR_ENDLOOP;
		m_wh.dwLoops = dwLoops;
	}
	if(waveOutPrepareHeader(m_hWaveOut, &m_wh, sizeof(WAVEHDR))) {
		waveOutClose(m_hWaveOut); m_hWaveOut = NULL;
		free(m_pFormat); m_pFormat = NULL;
		free(m_pData); m_pData = NULL;
		return 0;
	}
	
	if(waveOutWrite(m_hWaveOut, &m_wh, sizeof(WAVEHDR)) != 0) {
		waveOutUnprepareHeader(m_hWaveOut, &m_wh, sizeof(WAVEHDR));
		waveOutClose(m_hWaveOut);	m_hWaveOut = NULL;
		free(m_pFormat); m_pFormat = NULL;
		free(m_pData); m_pData = NULL;
		return 0;
	}
	
	return 1;
}
//================================================================================================
//---------------------//---------------------------------------+++--> End Play of Wave Audio File:
void StopWave(void)   //--------------------------------------------------------------------+++-->
{
	if(!m_hWaveOut) return;
	waveOutReset(m_hWaveOut);
	waveOutUnprepareHeader(m_hWaveOut, &m_wh, sizeof(WAVEHDR));
	waveOutClose(m_hWaveOut);
	m_hWaveOut = NULL;
	free(m_pFormat); m_pFormat = NULL;
	free(m_pData); m_pData = NULL;
}
//===============================================================================*
// -------------------------------------------------------------- The NIST Says...
//=================================================================================*
/*
From the National Institute of Standards and Technology - Time & Frequency Division

Q. Are noon and midnight 12 a.m. or 12 p.m.?

A. This is a tricky question. The answer is that the terms 12 a.m. and 12 p.m. are wrong
and should not be used.

To illustrate this, consider that "a.m" and "p.m." are abbreviations for "ante meridiem"
and "post meridiem." They mean "before noon" and "after noon," respectively. Noon is neither
before or after noon; it is simply noon. Therefore, neither the "a.m." nor "p.m." designation
is correct. On the other hand, midnight is both 12 hours before noon and 12 hours after noon.
Therefore, either 12 a.m. or 12 p.m. could work as a designation for midnight, but both would
be ambiguous as to the date intended.

When a specific date is important, and when we can use a 24-hour clock, we prefer to designate
that moment not as 1200 midnight, but rather as 0000 if we are referring to the beginning of a
given day (or date), or 2400 if we are designating the end of a given day (or date).

To be certain of avoiding ambiguity (while still using a 12-hour clock), specify an event as
beginning at 1201 a.m. or ending at 1159 p.m., for example; this method is used by the railroads
and airlines for schedules, and is often found on legal papers such as contracts and insurance
policies.

If one is referring not to a specific date, but rather to several days, or days in general, use
the terms noon and midnight instead of 12 a.m. and 12 p.m. For example, a bank might be open on
Saturdays from 8 a.m. to noon. Or a grocery store might be open daily until midnight. The terms
"12 noon" and "12 midnight" are also correct, though redundant.
*/
