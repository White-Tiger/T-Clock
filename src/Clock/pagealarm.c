/*-------------------------------------------
  pagealarm.c
  "Alarm" page of Options
  KAZUBON 1997-2001
---------------------------------------------*/
// Last Modified by Stoic Joker: Sunday, 03/13/2011 @ 11:54:05am
#include "tclock.h"
/// dialog events
static void OnInit(HWND hDlg);
static void OnDeinit(HWND hDlg);
static void OnApply(HWND hDlg);
static void OnChangeAlarm(HWND hDlg);
static void OnDropDownAlarm(HWND hDlg);
static void OnDay(HWND hDlg);
static void OnAlarmJihou(HWND hDlg, WORD id);
static void OnBrowseAction(HWND hDlg, WORD id);
static void On12Hour(HWND hDlg, int bOnChange);
static void OnDelAlarm(HWND hDlg);
static void OnFileChange(HWND hDlg, WORD id);
static void OnTest(HWND hDlg, WORD id, DWORD loops);
static void StopTest(HWND hDlg);
static void OnMsgAlarm(HWND hDlg, WORD id);
/// helpers
static void GetAlarmFromDlg(HWND hDlg, alarm_t* pAS);
static void SetAlarmToDlg(HWND hDlg, alarm_t* pAS);
static void SetDefaultAlarmToDlg(HWND hDlg, int select_only);

static void FormatTimeText(HWND hDlg, WORD id);
static void UpdateAMPMDisplay(HWND hDlg);

static char m_bTransition=0;
static int m_curAlarm;
static unsigned m_days;

static void SendPSChanged(HWND hDlg){
	if(!m_bTransition)
		SendMessage(GetParent(hDlg),PSM_CHANGED,(WPARAM)(hDlg),0);
}

/*------------------------------------------------
  Dialog procedure
--------------------------------------------------*/
INT_PTR CALLBACK Page_Alarm(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch(message) {
	case WM_INITDIALOG:
		OnInit(hDlg);
		return TRUE;
	case WM_DESTROY:
		OnDeinit(hDlg);
		break;
		
	case WM_COMMAND: {
		WORD id = LOWORD(wParam);
		WORD code = HIWORD(wParam);
		switch(id){
		/// global hourly chime and "chime hour"
		case IDC_ALARM:
		case IDC_JIHOU:
			OnAlarmJihou(hDlg, id);
			SendPSChanged(hDlg);
			break;
		// file name changed
		case IDC_HOURALARM:
		case IDC_MINUTEALARM:
			if(code==EN_CHANGE){
				if(!m_bTransition){
					FormatTimeText(hDlg,id);
					SendPSChanged(hDlg);
				}
			}
			break;
		// browse file
		case IDC_BROWSEALARM:
		case IDC_BROWSEJIHOU:
			OnBrowseAction(hDlg, id);
			OnFileChange(hDlg, (WORD)(id - 1));
			SendPSChanged(hDlg);
			break;
		// test sound
		case IDC_TESTALARM:
		case IDC_TESTJIHOU:{
			int loops=0;
			if(id==IDC_TESTALARM){
				if(IsDlgButtonChecked(hDlg,IDC_REPEATALARM)){
					loops=GetDlgItemInt(hDlg,IDC_REPEATIMES,NULL,1);
					if(!loops) loops=-1;
				}
			}
			OnTest(hDlg,id,loops);
			break;}
		/// alarm chooser
		case IDC_COMBOALARM:
			if(code==CBN_SELCHANGE){
				OnChangeAlarm(hDlg); // update currently selected alarm
			}else if(code==CBN_DROPDOWN){
				OnDropDownAlarm(hDlg); // update name if changed
			}else if(code==CBN_EDITCHANGE){
				if(m_curAlarm == 0)
					m_curAlarm = -1;
				SendPSChanged(hDlg);
			}
			break;
		// delete an alarm
		case IDC_DELALARM:
			OnDelAlarm(hDlg);
			SendPSChanged(hDlg);
			break;
		// file name changed
		case IDC_FILEALARM:
		case IDC_FILEJIHOU:
			if(code==CBN_EDITCHANGE){
				OnFileChange(hDlg,id);
				SendPSChanged(hDlg);
			}else if(code==CBN_SELCHANGE)
				PostMessage(hDlg,WM_COMMAND,MAKEWPARAM(id,CBN_EDITCHANGE),lParam);
			break;
		// day selector
		case IDC_ALARMDAY:
			OnDay(hDlg);
			break;
		// Message Window Options
		case IDCB_MSG_ALARM:{
			dlgmsg_t dlg;
			GetDlgItemText(hDlg, IDC_COMBOALARM, dlg.name, _countof(dlg.name));
			GetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, dlg.message, _countof(dlg.message));
			GetDlgItemText(hDlg, IDC_ALRMMSG_SETTINGS, dlg.settings, _countof(dlg.settings));
			if(BounceWindOptions(hDlg,&dlg)){
				SetDlgItemText(hDlg, IDC_ALRMMSG_SETTINGS, dlg.settings);
				SetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, dlg.message);
				SendPSChanged(hDlg);
			}
			break;}
		// checked "12 hour"
		case IDC_12HOURALARM:
			On12Hour(hDlg,1);
			SendPSChanged(hDlg);
			break;
		// checked PM - Toggle Display of AM or PM
		case IDC_AMPM_CHECK:
			UpdateAMPMDisplay(hDlg);
			SendPSChanged(hDlg);
			break;
		// checked "display message window"
		case IDC_MSG_ALARM:
			OnMsgAlarm(hDlg,id);
			SendPSChanged(hDlg);
			break;
		// checked repeat alarm or chime
		case IDC_CHIMEALARM:
			CheckDlgButton(hDlg, IDC_REPEATALARM, FALSE);
			EnableDlgItem(hDlg, IDC_REPEATIMES, FALSE);
			EnableDlgItem(hDlg, IDC_SPINTIMES, FALSE);
			/* fall through */
		case IDC_REPEATALARM:
			if(id==IDC_REPEATALARM) {
				int checked=IsDlgButtonChecked(hDlg,IDC_REPEATALARM);
				CheckDlgButton(hDlg, IDC_CHIMEALARM, FALSE);
				EnableDlgItem(hDlg, IDC_REPEATIMES, checked);
				EnableDlgItem(hDlg, IDC_SPINTIMES, checked);
			}
			/* fall through */
		case IDC_ALRM_ONCE: {
			int i;
			for (i = IDC_LABDATEALARM; i <= IDC_SPINMONTH; ++i)
				EnableDlgItem(hDlg, i, IsDlgButtonChecked(hDlg, IDC_ALRM_ONCE));
		}
			/* fall through */
		// checked other checkboxes
		case IDC_REPEATJIHOU:
		case IDC_BLINKALARM:
		case IDC_BLINKJIHOU:
			SendPSChanged(hDlg);
			break;
		}
		return TRUE;}
	case WM_NOTIFY:
		switch(((NMHDR*)lParam)->code) {
		case PSN_APPLY:
			OnApply(hDlg);
			break;
			
		case UDN_DELTAPOS:
			break;
			
		} return TRUE; //--+++--> End Of Case WM_NOTIFY:
		
		// playing sound ended
	case MM_WOM_DONE:
		StopFile();
		/* fall through */
	case MM_MCINOTIFY:
		if(message==MM_MCINOTIFY){
			if(OnMCINotify(hDlg))
				return TRUE;
		}
		SendDlgItemMessage(hDlg, IDC_TESTALARM, BM_SETIMAGE, IMAGE_ICON,
						   (LPARAM)g_hIconPlay);
		SendDlgItemMessage(hDlg, IDC_TESTJIHOU, BM_SETIMAGE, IMAGE_ICON,
						   (LPARAM)g_hIconPlay);
		return TRUE;
	}
	return FALSE;
}
/*------------------------------------------------
  initialize
--------------------------------------------------*/
void OnInit(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
	HWND file_boxes[2];
	#define file_cb file_boxes[0]
	#define file_hourly_cb file_boxes[1]
	wchar_t tmp[MAX_PATH];
	int i, count;
	file_cb = GetDlgItem(hDlg, IDC_FILEALARM);
	file_hourly_cb = GetDlgItem(hDlg, IDC_FILEJIHOU);
	/// add default sound files to sound file dropdown
	ComboBoxArray_AddSoundFiles(file_boxes, 2);
	/// add "new" entry
	ComboBox_SetItemData(alarm_cb, ComboBox_AddString(alarm_cb, MyString(IDS_ADDALARM)), 0);
	/// add alarms
	count = GetAlarmNum();
	for(i=0; i<count; ++i) {
		alarm_t* pAS = malloc(sizeof(alarm_t));
		ReadAlarmFromReg(pAS, i);
		ComboBox_SetItemData(alarm_cb, ComboBox_AddString(alarm_cb, pAS->dlgmsg.name), pAS);
		if(!i) SetAlarmToDlg(hDlg, pAS);
	}
	/// other
	m_curAlarm = 0;
	if(count > 0) {
		ComboBox_SetCurSel(alarm_cb, 1);
		OnChangeAlarm(hDlg);
	} else {
		SetDefaultAlarmToDlg(hDlg, 1);
	}
	
	CheckDlgButton(hDlg, IDC_JIHOU,
				   api.GetInt(L"", L"Jihou", 0));
				   
	api.GetStr(L"", L"JihouFile", tmp, _countof(tmp), L"Clock.wav");
	ComboBox_SetText(file_hourly_cb, tmp);
	
	CheckDlgButton(hDlg, IDC_REPEATJIHOU,
				   api.GetInt(L"", L"JihouRepeat", 0));
				   
	CheckDlgButton(hDlg, IDC_BLINKJIHOU,
				   api.GetInt(L"", L"JihouBlink", 0));
				   
	OnAlarmJihou(hDlg, IDC_JIHOU);
	
	/// add play icons
	SendDlgItemMessage(hDlg, IDC_TESTALARM, BM_SETIMAGE, IMAGE_ICON,
					   (LPARAM)g_hIconPlay);
	OnFileChange(hDlg, IDC_FILEALARM);
	SendDlgItemMessage(hDlg, IDC_TESTJIHOU, BM_SETIMAGE, IMAGE_ICON,
					   (LPARAM)g_hIconPlay);
	OnFileChange(hDlg, IDC_FILEJIHOU);
	
	SendDlgItemMessage(hDlg, IDC_DELALARM, BM_SETIMAGE, IMAGE_ICON,
					   (LPARAM)g_hIconDel);
	#undef file_cb
	#undef file_hourly_cb
}
/*------------------------------------------------
  deinitialize
--------------------------------------------------*/
void OnDeinit(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg,IDC_COMBOALARM);
	int count = ComboBox_GetCount(hDlg);
	StopFile();
	if(!count)
		return;
	for(; --count; ){ // free memory
		free((alarm_t*)ComboBox_GetItemData(alarm_cb,count));
		ComboBox_DeleteString(alarm_cb, count);
	}
}

/*------------------------------------------------
   apply - save settings
--------------------------------------------------*/
void OnApply(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
	wchar_t file[MAX_PATH];
	int i, count, n_alarm;
	alarm_t* pAS;
	
	if(m_curAlarm < 0) {
		wchar_t name[_countof(pAS->dlgmsg.name)];
		ComboBox_GetText(alarm_cb, name, _countof(name));
		if(name[0]) {
			pAS = malloc(sizeof(alarm_t));
			if(pAS) {
				GetAlarmFromDlg(hDlg, pAS);
				i = ComboBox_AddString(alarm_cb, pAS->dlgmsg.name);
				ComboBox_SetItemData(alarm_cb, i, pAS);
				m_curAlarm = i;
				ComboBox_SetCurSel(alarm_cb, i);
				EnableDlgItem(hDlg, IDC_DELALARM, 1);
			}
		}
	} else if(m_curAlarm) {
		pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, m_curAlarm);
		GetAlarmFromDlg(hDlg, pAS);
	}
	
	// update alarms
	count = ComboBox_GetCount(alarm_cb)-1;
	for(n_alarm=0; n_alarm<count; ++n_alarm) {
		pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, n_alarm+1);
		SaveAlarmToReg(pAS, n_alarm);
	}
	SetAlarmNum(n_alarm);
	// delete remaining
	for(; DeleteAlarmFromReg(n_alarm)==0; ++n_alarm);
	
	api.SetInt(L"", L"Jihou",
				 IsDlgButtonChecked(hDlg, IDC_JIHOU));
				 
	GetDlgItemText(hDlg, IDC_FILEJIHOU, file, _countof(file));
	api.SetStr(L"", L"JihouFile", file);
	
	api.SetInt(L"", L"JihouRepeat",
				 IsDlgButtonChecked(hDlg, IDC_REPEATJIHOU));
	api.SetInt(L"", L"JihouBlink",
				 IsDlgButtonChecked(hDlg, IDC_BLINKJIHOU));
				 
	InitAlarm(); // alarm.c
}
//================================================================================================
//------------------------------------+++--> Load Current Alarm Setting From Dialog into Structure:
void GetAlarmFromDlg(HWND hDlg, alarm_t* pAS)   //--------------------------------------+++-->
{
	if(m_curAlarm > 0){ // update alarm name in combobox
		size_t oldlen = wcslen(pAS->dlgmsg.name), newlen;
		wchar_t name[_countof(pAS->dlgmsg.name)];
		newlen = GetDlgItemText(hDlg, IDC_COMBOALARM, name, _countof(name));
		if(newlen != oldlen || wcscmp(name,pAS->dlgmsg.name)){
			HWND combo = GetDlgItem(hDlg, IDC_COMBOALARM);
			ComboBox_DeleteString(combo, m_curAlarm);
			ComboBox_InsertString(combo, m_curAlarm, name);
			ComboBox_SetItemData(combo, m_curAlarm, pAS);
			memcpy(pAS->dlgmsg.name, name, newlen * sizeof pAS->dlgmsg.name[0]);
		}
	}else
		GetDlgItemText(hDlg, IDC_COMBOALARM, pAS->dlgmsg.name, _countof(pAS->dlgmsg.name));
	
	pAS->iTimes = GetDlgItemInt(hDlg,IDC_REPEATIMES,NULL,1);
	
	pAS->uFlags=0;
	if(IsDlgButtonChecked(hDlg,IDC_ALARM)) pAS->uFlags|=ALRM_ENABLED;
	if(IsDlgButtonChecked(hDlg,IDC_ALRM_ONCE)) pAS->uFlags|=ALRM_ONESHOT;
	if(IsDlgButtonChecked(hDlg,IDC_12HOURALARM)) pAS->uFlags|=ALRM_12H;
	if(IsDlgButtonChecked(hDlg,IDC_CHIMEALARM)) pAS->uFlags|=ALRM_CHIMEHR;
	if(IsDlgButtonChecked(hDlg,IDC_REPEATALARM)) pAS->uFlags|=ALRM_REPEAT;
	if(IsDlgButtonChecked(hDlg,IDC_BLINKALARM)) pAS->uFlags|=ALRM_BLINK;
	if(IsDlgButtonChecked(hDlg,IDC_MSG_ALARM)) pAS->uFlags|=ALRM_DIALOG;
	
	pAS->hour = (int)SendDlgItemMessage(hDlg,IDC_SPINHOUR,UDM_GETPOS32,0,0);
	if(pAS->uFlags&ALRM_12H)
		pAS->hour = _12hTo24h(pAS->hour, IsDlgButtonChecked(hDlg,IDC_AMPM_CHECK));
	pAS->minute = (int)SendDlgItemMessage(hDlg,IDC_SPINMINUTE,UDM_GETPOS32,0,0);
	pAS->days = m_days;
	pAS->day = (int)SendDlgItemMessage(hDlg,IDC_SPINDATE,UDM_GETPOS32,0,0);
	pAS->month = (int)SendDlgItemMessage(hDlg,IDC_SPINMONTH,UDM_GETPOS32,0,0);
	
	GetDlgItemText(hDlg, IDC_FILEALARM, pAS->fname, MAX_PATH);
	
	GetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, pAS->dlgmsg.message, _countof(pAS->dlgmsg.message));
	GetDlgItemText(hDlg, IDC_ALRMMSG_SETTINGS, pAS->dlgmsg.settings, _countof(pAS->dlgmsg.settings));
}
//================================================================================================
//-------------------------------------------------+++--> Load Dialog With Settings for This Alarm:
void SetAlarmToDlg(HWND hDlg, alarm_t* pAS)   //--------------------------------------------+++-->
{
	HWND file_cb = GetDlgItem(hDlg, IDC_FILEALARM);
	
	SetDlgItemText(hDlg, IDC_COMBOALARM, pAS->dlgmsg.name);
	
	SendDlgItemMessage(hDlg, IDC_SPINMONTH, UDM_SETRANGE32, 1, 12);
	SendDlgItemMessage(hDlg, IDC_SPINMONTH, UDM_SETPOS32, 0, pAS->month);
	SendDlgItemMessage(hDlg, IDC_SPINDATE, UDM_SETRANGE32, 1, 31);
	SendDlgItemMessage(hDlg, IDC_SPINDATE, UDM_SETPOS32, 0, pAS->day);
	SendDlgItemMessage(hDlg, IDC_SPINMINUTE, UDM_SETRANGE32, 0,59);
	SendDlgItemMessage(hDlg, IDC_SPINMINUTE, UDM_SETPOS32, 0, pAS->minute);
	SendDlgItemMessage(hDlg, IDC_SPINTIMES, UDM_SETRANGE32, (WPARAM)-1,42);
	SendDlgItemMessage(hDlg, IDC_SPINTIMES, UDM_SETPOS32, 0, pAS->iTimes);
	ComboBox_AddStringOnce(file_cb, pAS->fname, 1, 0);
	
	SetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, pAS->dlgmsg.message);
	SetDlgItemText(hDlg, IDC_ALRMMSG_SETTINGS, pAS->dlgmsg.settings);
	
	CheckDlgButton(hDlg,IDC_ALARM,pAS->uFlags&ALRM_ENABLED);
	CheckDlgButton(hDlg,IDC_ALRM_ONCE,pAS->uFlags&ALRM_ONESHOT);
	CheckDlgButton(hDlg,IDC_CHIMEALARM,pAS->uFlags&ALRM_CHIMEHR);
	CheckDlgButton(hDlg,IDC_REPEATALARM,pAS->uFlags&ALRM_REPEAT);
	CheckDlgButton(hDlg,IDC_BLINKALARM,pAS->uFlags&ALRM_BLINK);
	CheckDlgButton(hDlg,IDC_MSG_ALARM,pAS->uFlags&ALRM_DIALOG);
	
//	SendDlgItemMessage(hDlg, IDC_SPINHOUR, UDM_SETRANGE32, 0,23); // On12Hour also does this
	CheckDlgButton(hDlg,IDC_12HOURALARM,pAS->uFlags&ALRM_12H);
	CheckDlgButton(hDlg, IDC_AMPM_CHECK, (pAS->hour>=12));
	if(pAS->uFlags&ALRM_12H)
		SendDlgItemMessage(hDlg, IDC_SPINHOUR, UDM_SETPOS32, 0, _24hTo12h(pAS->hour));
	else
		SendDlgItemMessage(hDlg, IDC_SPINHOUR, UDM_SETPOS32, 0, pAS->hour);
	On12Hour(hDlg,0); // enable/disable AM/PM, format hour (09 for 24h, 9 for 12h)
	
	m_days=pAS->days;
	
	FormatTimeText(hDlg,IDC_MINUTEALARM); // correctly format minutes (00, 09)
	OnFileChange(hDlg, IDC_FILEALARM);
	OnMsgAlarm(hDlg, IDC_MSG_ALARM);
	OnAlarmJihou(hDlg, IDC_ALARM);
}
//=========================================================================================
//-------------------------------------------------+++--> load dialog with default settings:
void SetDefaultAlarmToDlg(HWND hDlg, int select_only)   //--------------------------------------------+++-->
{
	alarm_t as = {0};
	as.days = DAYF_EVERYDAY_BITMASK;
	as.hour = 12;
	as.iTimes = -1;
	as.uFlags =  ALRM_ENABLED | ALRM_ONESHOT | ALRM_DIALOG | ALRM_BLINK | ALRM_REPEAT;
	if(api.GetInt(L"Format",L"Hour12",1)) // if user prefers 12h format (his clock is 12h)
		as.uFlags |= ALRM_12H;
	if(select_only)
		as.uFlags ^= ALRM_ENABLED;
	wcscpy(as.fname, L"Alarm.wav");
	SetAlarmToDlg(hDlg, &as);
	EnableDlgItem(hDlg, IDC_DELALARM, FALSE);
	if(select_only){
		ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_COMBOALARM), 0);
		m_curAlarm = 0;
	}else{
		m_curAlarm = -1;
	}
}
/*------------------------------------------------
   selected an alarm name by combobox
--------------------------------------------------*/
void OnChangeAlarm(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
	alarm_t* pAS;
	int index;
	
	index = ComboBox_GetCurSel(alarm_cb);
	if(index == m_curAlarm && m_curAlarm) // no change, ignore
		return;
//	if(index == 0 && m_curAlarm == -1){
	if(m_curAlarm == -1){
		wchar_t name[_countof(pAS->dlgmsg.name)];
		ComboBox_GetText(alarm_cb, name, _countof(name));
		if(name[0]) {
			pAS = malloc(sizeof(alarm_t));
			if(pAS) {
				int new_index;
				GetAlarmFromDlg(hDlg, pAS);
				new_index = ComboBox_AddString(alarm_cb, pAS->dlgmsg.name);
				ComboBox_SetItemData(alarm_cb, new_index, pAS);
			}
		}else if(ComboBox_GetCount(alarm_cb) > 1){
			index = 1;
			ComboBox_SetCurSel(alarm_cb, index);
		}
	}
	if(index == 0){
		ComboBox_SetCurSel(alarm_cb, -1);
		SetDefaultAlarmToDlg(hDlg, 0); // sets m_curAlarm
		return;
	}
	m_bTransition = 1; // start transition
	
	StopTest(hDlg);
	
	if(m_curAlarm > 0){ // update previous Alarm
		pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, m_curAlarm);
		GetAlarmFromDlg(hDlg, pAS);
	}
	
	pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, index);
	SetAlarmToDlg(hDlg, pAS);
	EnableDlgItem(hDlg, IDC_DELALARM, TRUE);
	m_curAlarm = index;
	
	m_bTransition = 0; // end transition
}
/*------------------------------------------------
  combo box is about to be made visible
--------------------------------------------------*/
void OnDropDownAlarm(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
	alarm_t* pAS;
	wchar_t name[_countof(pAS->dlgmsg.name)];
	
	if(m_curAlarm <= 0)
		return;
	pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, m_curAlarm);
	ComboBox_GetText(alarm_cb, name, _countof(pAS->dlgmsg.name));
	if(name[0] && wcscmp(name, pAS->dlgmsg.name)) {
		wcscpy(pAS->dlgmsg.name, name);
		ComboBox_DeleteString(alarm_cb, m_curAlarm);
		ComboBox_InsertString(alarm_cb, m_curAlarm, name);
		ComboBox_SetItemData(alarm_cb, m_curAlarm, pAS);
		ComboBox_SetCurSel(alarm_cb, m_curAlarm);
	}
}

/*------------------------------------------------
  "Day..."
--------------------------------------------------*/
void OnDay(HWND hDlg)
{
	int days=ChooseAlarmDay(hDlg,m_days);
	if(days&ALARMDAY_OKFLAG && (days^ALARMDAY_OKFLAG)!=m_days){
		m_days=days^ALARMDAY_OKFLAG;
		SendPSChanged(hDlg);
	}
}
/*------------------------------------------------
  checked "Enable" or "Play sound every hour"
--------------------------------------------------*/
void OnAlarmJihou(HWND hDlg, WORD id)
{
	int cstart, cend, i;
	BOOL enabled;
	
	enabled = IsDlgButtonChecked(hDlg, id);
	
	if(id == IDC_ALARM) {
		cstart = IDC_LABTIMEALARM; cend = IDC_SPINMONTH;
		if(enabled) {
			int repeat=IsDlgButtonChecked(hDlg,IDC_REPEATALARM);
			OnMsgAlarm(hDlg,IDC_MSG_ALARM);
			EnableDlgItem(hDlg,IDC_REPEATIMES,repeat);
			EnableDlgItem(hDlg,IDC_SPINTIMES,repeat);
		} else {
			ShowWindow(GetDlgItem(hDlg, IDC_BMPJACK), SW_HIDE);
			EnableDlgItem(hDlg, IDCB_MSG_ALARM, FALSE);
		}
	} else {
		cstart = IDC_LABSOUNDJIHOU; cend = IDC_BLINKJIHOU;
	}
	
	for(i=cstart; i<=cend; ++i)
		EnableDlgItem(hDlg, i, enabled);


	if (id == IDC_ALARM && enabled && !IsDlgButtonChecked(hDlg, IDC_ALRM_ONCE)) {
		cstart = IDC_LABDATEALARM;
		for (i = cstart; i <= cend; ++i)
			EnableDlgItem(hDlg, i, !enabled);
	}
	
	if(id == IDC_ALARM){
		OnFileChange(hDlg, IDC_FILEALARM);
		if(enabled) {
			HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
			if(ComboBox_GetCurSel(alarm_cb) == 0){
				ComboBox_SetCurSel(alarm_cb, -1);
				m_curAlarm = -1;
			}
			EnableDlgItem(hDlg,IDC_AMPM_CHECK, IsDlgButtonChecked(hDlg,IDC_12HOURALARM));
		}
	}else
		OnFileChange(hDlg, IDC_FILEJIHOU);
}
//-------------------------------------//------------------------------------+++-->
//-----------------------------+++--> Show/Hide the Bouncing Message Window Options:
void OnMsgAlarm(HWND hDlg, WORD id)   //-------------------------------------+++-->
{
	BOOL b; //--+++--> Here Begins the Jack Russel Terrier Window Feature!
	
	b = IsDlgButtonChecked(hDlg, id);
	if(b) {
		ShowWindow(GetDlgItem(hDlg, IDC_BMPJACK), SW_SHOW);
		EnableDlgItem(hDlg, IDCB_MSG_ALARM, TRUE);
	} else {
		ShowWindow(GetDlgItem(hDlg, IDC_BMPJACK), SW_HIDE);
		EnableDlgItem(hDlg, IDCB_MSG_ALARM, FALSE);
	}
}
/*------------------------------------------------
  browse sound file
--------------------------------------------------*/
void OnBrowseAction(HWND hDlg, WORD id)
{
	wchar_t deffile[MAX_PATH], fname[MAX_PATH];
	GetDlgItemText(hDlg, id - 1, deffile, _countof(deffile));
	
	StopTest(hDlg);
	
	if(!BrowseSoundFile(hDlg, deffile, fname)) // soundselect.c
		return;
		
	SetDlgItemText(hDlg, id - 1, fname);
	PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
}

/*------------------------------------------------
  checked "12 hour"
--------------------------------------------------*/
void On12Hour(HWND hDlg, int bOnChange)
{
	int b12h = IsDlgButtonChecked(hDlg, IDC_12HOURALARM);
	int h, u, l;
	wchar_t hour[5];
	Edit_GetText(GetDlgItem(hDlg,IDC_HOURALARM), hour, _countof(hour));
	h = _wtoi(hour);
	if(h > 23)
		h=0;
	
	if(b12h){
//		if(IsDlgButtonChecked(hDlg,IDC_AMPM_CHECK)){
			u=12; l=1; // PM
//		}else{
//			u=11; l=0; // AM
//		}
	}else{
		u=23; l=0; // 24h
	}
	if(bOnChange){
		if(b12h) // convert to 12h
			h = _24hTo12h(h);
		else // convert to 24h
			h = _12hTo24h(h, IsDlgButtonChecked(hDlg,IDC_AMPM_CHECK));
	}
	EnableDlgItem(hDlg,IDC_AMPM_CHECK,b12h);
	
	// set limits to spin controls
	SendDlgItemMessage(hDlg, IDC_SPINHOUR, UDM_SETRANGE32, l,u);
	SendDlgItemMessage(hDlg, IDC_SPINHOUR, UDM_SETPOS32, 0, h);
	FormatTimeText(hDlg,IDC_HOURALARM);
}
/*------------------------------------------------
  delete an alarm
--------------------------------------------------*/
void OnDelAlarm(HWND hDlg)
{
	HWND alarm_cb = GetDlgItem(hDlg, IDC_COMBOALARM);
	alarm_t* pAS;
	
	if(m_curAlarm <= 0)
		return;
	
	StopTest(hDlg);
	
	pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, m_curAlarm);
	PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
	ComboBox_DeleteString(alarm_cb, m_curAlarm);
	free(pAS);
	ComboBox_SetCurSel(alarm_cb, --m_curAlarm);
	if(!m_curAlarm){
		SetDefaultAlarmToDlg(hDlg, 1);
		return;
	}
	pAS = (alarm_t*)ComboBox_GetItemData(alarm_cb, m_curAlarm);
	SetAlarmToDlg(hDlg, pAS);
}

/*------------------------------------------------
   file name changed - enable/disable controls
--------------------------------------------------*/
void OnFileChange(HWND hDlg, WORD id)
{
	wchar_t fname[MAX_PATH];
	int enable = 0;
	int control;
	GetDlgItemText(hDlg, id, fname, _countof(fname));
	if(IsWindowEnabled(GetDlgItem(hDlg,id)) && *fname!='<' && *fname){
		enable = 1;
	}
	for(control=id+4; control>id+1; --control) // IDC_FILEALARM+2 -> IDC_REPEATALARM, IDC_FILEJIHOU+2 -> IDC_REPEATJIHOU
		EnableDlgItem(hDlg,control,enable);
	if(enable){
		enable = IsMMFile(fname);
		EnableDlgItem(hDlg,id+4,enable); // IDC_REPEATALARM, IDC_REPEATJIHOU
	}
	if(id==IDC_FILEALARM){
		EnableDlgItem(hDlg,IDC_CHIMEALARM,enable);
		enable = enable&&IsDlgButtonChecked(hDlg,IDC_REPEATALARM); // IsMMFile(fname)
		for(control=IDC_REPEATIMES; control<=IDC_SPINTIMES; ++control) // IDC_FILEALARM+2 -> IDC_REPEATALARM, IDC_FILEJIHOU+2 -> IDC_REPEATJIHOU
			EnableDlgItem(hDlg,control,enable);
	}
}

/*------------------------------------------------
  test sound
--------------------------------------------------*/
void OnTest(HWND hDlg, WORD id, DWORD loops)
{
	wchar_t fname[MAX_PATH];
	
	GetDlgItemText(hDlg, id - 3, fname, _countof(fname));
	if(!fname[0])
		return;
	
	if((HICON)SendDlgItemMessage(hDlg,id,BM_GETIMAGE,IMAGE_ICON,0) == g_hIconPlay) {
		if(PlayFile(hDlg,fname,loops)) {
			SendDlgItemMessage(hDlg,id,BM_SETIMAGE,IMAGE_ICON,(LPARAM)g_hIconStop);
			InvalidateRect(GetDlgItem(hDlg,id),NULL,FALSE);
		}
	} else {
		StopFile();
	}
}
void StopTest(HWND hDlg)
{
	if((HICON)SendDlgItemMessage(hDlg,IDC_TESTALARM,BM_GETIMAGE,IMAGE_ICON,0) == g_hIconStop) {
		StopFile();
	}
}
//======================================*
// Format 12/24hr Time Text 02:01 Properly
//========================================*
void FormatTimeText(HWND hDlg, WORD idc)
{
	wchar_t txt[5];
	wchar_t txt2[5];
	int iTxt;
	
	m_bTransition = 1; // start transition
	
	if(idc == IDC_MINUTEALARM){
		HWND edit = GetDlgItem(hDlg, IDC_MINUTEALARM);
		Edit_GetText(edit, txt, _countof(txt));
		iTxt = _wtoi(txt);
		if(iTxt>59)
			iTxt = 0;
		
		wsprintf(txt2, FMT("%02d"), iTxt);
		if(wcscmp(txt2,txt)){
			Edit_SetText(edit,txt2);
			Edit_SetSel(edit,2,2);
		}
	}else{
		int b12h = IsDlgButtonChecked(hDlg, IDC_12HOURALARM);
		HWND edit = GetDlgItem(hDlg,IDC_HOURALARM);
		Edit_GetText(edit, txt, _countof(txt));
		iTxt = _wtoi(txt);
		if(b12h){
			if(iTxt>12) iTxt = 12; // 12am / 12pm
			wsprintf(txt2, FMT("%d"), iTxt);
		}else{
			if(iTxt>23) iTxt = 0;
			wsprintf(txt2, FMT("%02d"), iTxt);
			CheckDlgButton(hDlg,IDC_AMPM_CHECK,iTxt>=12);
		}
		if(wcscmp(txt2,txt)){
			Edit_SetText(edit, txt2);
			Edit_SetSel(edit, 2 ,2);
		}
	}
	UpdateAMPMDisplay(hDlg);
	
	m_bTransition = 0; // end transition
}
//=============================*
// -- Toggle Display of AM or PM
// ---- Based on CheckBox State
//================================*
void UpdateAMPMDisplay(HWND hDlg)
{
	wchar_t time[5];
	int hour, min;
	
	GetDlgItemText(hDlg, IDC_HOURALARM, time, _countof(time));
	hour = _wtoi(time);
	if(IsDlgButtonChecked(hDlg,IDC_12HOURALARM))
		hour = _12hTo24h(hour, IsDlgButtonChecked(hDlg,IDC_AMPM_CHECK));
	GetDlgItemText(hDlg, IDC_MINUTEALARM, time, _countof(time));
	min = _wtoi(time);
	
	if(hour<12)
		SetDlgItemText(hDlg, IDC_AMPM_DISPLAY, (!hour&&!min ? L"midnight" : L"AM"));
	else
		SetDlgItemText(hDlg, IDC_AMPM_DISPLAY, (hour==12&&!min ? L"noon" : L"PM"));
}
