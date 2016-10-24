/*-------------------------------------------
  pageformat.c
  "Format" page of Options
                       KAZUBON 1997-1998
---------------------------------------------*/

#include "tclock.h"

static wchar_t* m_entrydate[FORMAT_NUM]={
	L"Year4", L"Year", L"Month", L"MonthS", L"Day", L"Weekday",
	L"Hour", L"Minute", L"Second", L"AMPM", L"InternetTime",
	L"Lf", L"Hour12", L"HourZero", L"Custom",
};
#define ENTRY(id) m_entrydate[(id)-FORMAT_BEGIN]
#define CHECKS(id) checks[(id)-FORMAT_BEGIN]
static void CreateFormat(wchar_t* format, char* checks);

static void OnInit(HWND hDlg);
static void OnApply(HWND hDlg,BOOL preview);
static void OnLocale(HWND hDlg);
static void OnCustom(HWND hDlg, BOOL bmouse);
static void OnFormatCheck(HWND hDlg, WORD id);

static int m_ilang;  // language code. ex) 0x411 - Japanese
static int m_idate;  // 0: mm/dd/yy 1: dd/mm/yy 2: yy/mm/dd
static wchar_t* m_pCustomFormat = NULL;
static wchar_t m_sMon[10];  //

static char m_bDayOfWeekIsLast;   // yy/mm/dd ddd
static char m_bTimeMarkerIsFirst; // AM/PM hh:nn:ss

static wchar_t m_sep_date[4]; // date seperator such as / .
static wchar_t m_sep_time[4]; // time seperator such as : .


static char m_transition = -1; // can become a problem if not initializes.. see pagecolor.c
static inline void SendPSChanged(HWND hDlg){
	if(m_transition==-1) return;
	g_bApplyClock = 1;
	g_bApplyTaskbar = 1;
	SendMessage(GetParent(hDlg), PSM_CHANGED, (WPARAM)(hDlg), 0);
	
	OnApply(hDlg,1);
	SendMessage(g_hwndClock, CLOCKM_REFRESHCLOCKPREVIEWFORMAT, 0, 0);
}
/*------------------------------------------------
   Dialog Procedure for the "Format" page
--------------------------------------------------*/
INT_PTR CALLBACK Page_Format(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		OnInit(hDlg);
		return TRUE;
	case WM_DESTROY:
		if(m_pCustomFormat) {
			free(m_pCustomFormat);
			m_pCustomFormat = NULL;
		}
		break;
	case WM_CTLCOLORSTATIC:{
		HDC hdcStatic = (HDC)wParam;
		int id = GetDlgCtrlID((HWND)lParam);
		switch(id){
		case IDC_FORMAT:
			SetTextColor(hdcStatic, GetSysColor(COLOR_GRAYTEXT));
			break;
		case IDC_FORMAT_LNK:
			LinkControl_OnCtlColorStatic(hDlg, wParam, lParam);
			break;
		}
		return FALSE;
	}
	case WM_COMMAND: {
		WORD id=LOWORD(wParam);
		switch(id){
		case IDC_LOCALE:
			if(HIWORD(wParam)==CBN_SELCHANGE)
				OnLocale(hDlg);
			break;
		case IDC_CUSTOM:
			OnCustom(hDlg, TRUE);
			break;
		case IDC_AMSYMBOL:
		case IDC_PMSYMBOL:
			if(HIWORD(wParam)==CBN_EDITCHANGE || HIWORD(wParam)==CBN_SELCHANGE)
				SendPSChanged(hDlg);
			break;
		case IDC_ZERO:
		case IDC_FORMAT:
			SendPSChanged(hDlg);
			break;
		default: // "year" -- "Internet Time"
			if(id>=IDC_YEAR4 && id<=IDC_12HOUR)
				OnFormatCheck(hDlg, id);
		}
		return TRUE;}
	case WM_NOTIFY:{
		PSHNOTIFY* notify=(PSHNOTIFY*)lParam;
		switch(notify->hdr.code) {
		case PSN_APPLY:
			OnApply(hDlg,0);
			if(notify->lParam)
				m_transition=-1;
			break;
		case PSN_RESET:
			if(m_transition==1){
				SendMessage(g_hwndClock, CLOCKM_REFRESHCLOCK, 0, 0);
				SendMessage(g_hwndClock, CLOCKM_REFRESHTASKBAR, 0, 0);
				api.DelKey(L"Preview");
			}
			m_transition=-1;
			break;
		}
		return TRUE;}
	}
	return FALSE;
}

/*------------------------------------------------
  Initialize Checks with locale settings (2nd used to overwrite values based on locale default instead of users)
--------------------------------------------------*/
void ChecksLocaleInit(char checks[FORMAT_NUM], int ilang/*=0*/)
{
	wchar_t ampm[10];
	int ival;
	const int aLangDayOfWeekIsLast[] = {LANG_JAPANESE,LANG_KOREAN};
	char bTimeMarker; // use AM/PM (+12h format)
	
	if(ilang)
		m_ilang = ilang;
	else
		m_ilang = api.GetInt(L"Format", L"Locale", GetUserDefaultLangID());
	/// init language "member" variables
	// seperator
	GetLocaleInfo(m_ilang, LOCALE_SDATE, m_sep_date, _countof(m_sep_date));
	GetLocaleInfo(m_ilang, LOCALE_STIME, m_sep_time, _countof(m_sep_time));
	// AM/PM (seems to always use user's choice, not matter which locale. Also true for "is first"?)
	*ampm = '\0';
	GetLocaleInfo(m_ilang,LOCALE_S1159, ampm, _countof(ampm));/*!AM*/
	if(!*ampm)
		GetLocaleInfo(m_ilang,LOCALE_S2359, ampm, _countof(ampm));/*!PM*/
	bTimeMarker = (char)*ampm; // use 24h w/o AM/PM or 12h w/ AM/PM based on user locale
	GetLocaleInfo(m_ilang, LOCALE_ITIMEMARKPOSN|LOCALE_RETURN_NUMBER, (wchar_t*)&ival, sizeof(ival));
	m_bTimeMarkerIsFirst=(char)ival;
	// date
	GetLocaleInfo(m_ilang, LOCALE_IDATE|LOCALE_RETURN_NUMBER, (wchar_t*)&m_idate, sizeof(m_idate));
	GetLocaleInfo(m_ilang, LOCALE_SABBREVDAYNAME1, m_sMon, _countof(m_sMon));
	
	m_bDayOfWeekIsLast = 0;
	for(ival=0; ival<_countof(aLangDayOfWeekIsLast); ++ival) {
		if((m_ilang&0x00ff) == aLangDayOfWeekIsLast[ival]) {
			m_bDayOfWeekIsLast = 1;
			break;
		}
	}
	/// init checks
	for(ival=IDC_YEAR4; ival<=IDC_SECOND; ++ival) {
		CHECKS(ival) = (char)api.GetInt(L"Format", ENTRY(ival), 1);
	}
	
	if(CHECKS(IDC_YEAR)) CHECKS(IDC_YEAR4) = 0;
	else if(CHECKS(IDC_YEAR4)) CHECKS(IDC_YEAR) = 0;
	
	if(CHECKS(IDC_MONTH)) CHECKS(IDC_MONTHS) = 0;
	else if(CHECKS(IDC_MONTHS)) CHECKS(IDC_MONTH) = 0;
	
	/// @note : on next backward incompatible change, remove "Custom" key and detect it by comparing "Format" with "CustomFormat". If they match, it's custom
	for(ival=IDC_AMPM; ival<=IDC_CUSTOM; ++ival) {
		CHECKS(ival) = (char)api.GetInt(L"Format", ENTRY(ival), 0);
	}
	/// init locale based checks
	// use AM/PM and 12h format from new selected locale
	CHECKS(IDC_AMPM) = (char)bTimeMarker;
	CHECKS(IDC_12HOUR) = (char)bTimeMarker;
	// leading zero for 12h format // 05 vs 5 am
	GetLocaleInfo(m_ilang, LOCALE_ITLZERO|LOCALE_RETURN_NUMBER, (wchar_t*)&ival, sizeof(ival));
	CHECKS(IDC_HOUR) = (ival?2:1);
	if(!ilang){
		CHECKS(IDC_AMPM) = (char)api.GetIntEx(L"Format",ENTRY(IDC_AMPM),CHECKS(IDC_AMPM));
		CHECKS(IDC_12HOUR) = (char)api.GetIntEx(L"Format",ENTRY(IDC_12HOUR),CHECKS(IDC_12HOUR));
		CHECKS(IDC_HOUR) = (char)api.GetIntEx(L"Format",ENTRY(IDC_HOUR),CHECKS(IDC_HOUR));
	}
}

/*------------------------------------------------
  Initialize Locale Infomation (2nd to 3rd param used to overwrite values based on locale default)
--------------------------------------------------*/
void Checks2Dialog(char checks[FORMAT_NUM], HWND hDlg, int bLocaleOnly/*=0*/)
{
	if(!bLocaleOnly){
		int i;
		for(i=FORMAT_BEGIN; i<=FORMAT_END; ++i) {
			CheckDlgButton(hDlg,i,CHECKS(i));
		}
	}else{
		// use AM/PM and 12h format from new selected locale
		CheckDlgButton(hDlg,IDC_AMPM,CHECKS(IDC_AMPM));
		CheckDlgButton(hDlg,IDC_12HOUR,CHECKS(IDC_12HOUR));
		// leading zero for 12h format
		CheckDlgButton(hDlg,IDC_HOUR,CHECKS(IDC_HOUR));
	}
}

static HWND m_hwndPage;
/*------------------------------------------------
  for EnumSystemLocales function
--------------------------------------------------*/
BOOL CALLBACK EnumLocalesProc(wchar_t* lpLocaleString)
{
	HWND locale_cb = GetDlgItem(m_hwndPage, IDC_LOCALE);
	wchar_t str[80];
	int x, index;
	
	x = wtox(lpLocaleString);
	if(GetLocaleInfo(x,LOCALE_SLANGUAGE,str,_countof(str)) > 0)
		index = ComboBox_AddString(locale_cb, str);
	else
		index = ComboBox_AddString(locale_cb, lpLocaleString);
	ComboBox_SetItemData(locale_cb, index, x);
	return 1;
}

/*------------------------------------------------
  Initialize the "Format" page
--------------------------------------------------*/
void OnInit(HWND hDlg)
{
	const wchar_t* AM[] = {L"AM", L"am", L"A", L"a", L" ",};
	const wchar_t* PM[] = {L"PM", L"pm", L"P", L"p", L" ",};
	const int AMPMs = _countof(AM);
	HWND doc_lnk = GetDlgItem(hDlg, IDC_FORMAT_LNK);
	HWND format_cb = GetDlgItem(hDlg, IDC_FORMAT);
	HWND locale_cb = GetDlgItem(hDlg, IDC_LOCALE);
	HWND am_cb = GetDlgItem(hDlg, IDC_AMSYMBOL);
	HWND pm_cb = GetDlgItem(hDlg, IDC_PMSYMBOL);
	HFONT hfont;
	wchar_t fmt[MAX_FORMAT];
	int i, count;
	wchar_t ampm_user[TNY_BUFF];
	wchar_t ampm_locale[TNY_BUFF];
	char checks[FORMAT_NUM];
	
	m_transition=-1; // start transition lock
	m_hwndPage = hDlg;
		
	LinkControl_Setup(doc_lnk, LCF_SIMPLE|LCF_RELATIVE, L"T-Clock Help.rtf");
	
	// Fill and select the "Locale" combobox
	EnumSystemLocales(EnumLocalesProc, LCID_INSTALLED);
	i = ComboBox_AddString(locale_cb, L"<  user default  >");
	ComboBox_SetItemData(locale_cb, i, LOCALE_USER_DEFAULT);
	ComboBox_SetCurSel(locale_cb, i);
	
	ChecksLocaleInit(checks,0);
	Checks2Dialog(checks,hDlg,0);
	count = ComboBox_GetCount(locale_cb);
	for(i=0; i < count; ++i) {
		int x;
		x = (int)ComboBox_GetItemData(locale_cb, i);
		if(x == m_ilang) {
			ComboBox_SetCurSel(locale_cb, i); break;
		}
	}
	
	api.GetStr(L"Format", L"Format", fmt, _countof(fmt), L"");
	Edit_SetText(format_cb, fmt);
	
	m_pCustomFormat = malloc(MAX_FORMAT * sizeof m_pCustomFormat[0]);
	if(m_pCustomFormat)
		api.GetStr(L"Format", L"CustomFormat", m_pCustomFormat, MAX_FORMAT, L"");
	
	// "AM Symbol" and "PM Symbol"
	ComboBox_ResetContent(am_cb);
	api.GetStr(L"Format", L"AMsymbol", ampm_user, _countof(ampm_user), L"");
	if(*ampm_user)
		ComboBox_AddString(am_cb, ampm_user);
	if(GetLocaleInfo(m_ilang, LOCALE_S1159, ampm_locale, _countof(ampm_locale)) && wcscmp(ampm_user,ampm_locale))
		ComboBox_AddString(am_cb, ampm_locale);
	else
		*ampm_locale = '\0';
	for(i=0; i<AMPMs; ++i){
		if(wcscmp(ampm_locale,AM[i]) && wcscmp(ampm_user,AM[i]))
			ComboBox_AddString(am_cb, AM[i]);
	}
	ComboBox_SetCurSel(am_cb, 0);
	
	ComboBox_ResetContent(pm_cb);
	api.GetStr(L"Format", L"PMsymbol", ampm_user, _countof(ampm_user), L"");
	if(*ampm_user)
		ComboBox_AddString(pm_cb, ampm_user);
	if(GetLocaleInfo(m_ilang, LOCALE_S2359, ampm_locale, _countof(ampm_locale)) && wcscmp(ampm_user,ampm_locale))
		ComboBox_AddString(pm_cb, ampm_locale);
	else
		*ampm_locale='\0';
	for(i=0; i<AMPMs; ++i){
		if(wcscmp(ampm_locale,PM[i]) && wcscmp(ampm_user,PM[i]))
			ComboBox_AddString(pm_cb, PM[i]);
	}
	ComboBox_SetCurSel(pm_cb, 0);
	
	OnCustom(hDlg, 0);
	m_transition=0; // end transition lock, ready to go
}

//================================================================================================
//---------------------------------------------------------------------------+++--> "Apply" button:
void OnApply(HWND hDlg,BOOL preview)   //---------------------------------------------------+++-->
{
	const wchar_t* section = (preview ? L"Preview" : L"Format");
	HWND am_cb = GetDlgItem(hDlg, IDC_AMSYMBOL);
	HWND pm_cb = GetDlgItem(hDlg, IDC_PMSYMBOL);
	HWND format_edt = GetDlgItem(hDlg, IDC_FORMAT);
	HWND locale_cb = GetDlgItem(hDlg, IDC_LOCALE);
	wchar_t str[MAX_FORMAT];
	int i;
	
	api.SetInt(section, L"Locale", (DWORD)ComboBox_GetItemData(locale_cb, ComboBox_GetCurSel(locale_cb)));
				 
	for(i = IDC_YEAR4; i <= IDC_CUSTOM; i++) {
		api.SetInt(section, ENTRY(i), IsDlgButtonChecked(hDlg, i));
	}
	
	i = ComboBox_GetCurSel(am_cb);
	if(i!=CB_ERR)
		ComboBox_GetLBText(am_cb, i, str);
	else
		ComboBox_GetText(am_cb, str, _countof(str));
	api.SetStr(section, L"AMsymbol", str);
	i = ComboBox_GetCurSel(pm_cb);
	if(i!=CB_ERR)
		ComboBox_GetLBText(pm_cb, i, str);
	else
		ComboBox_GetText(pm_cb, str, _countof(str));
	api.SetStr(section, L"PMsymbol", str);
	
	Edit_GetText(format_edt, str, _countof(str));
	api.SetStr(section, L"Format", str);
	
	if(m_pCustomFormat) {
		if(IsDlgButtonChecked(hDlg, IDC_CUSTOM))
			wcscpy(m_pCustomFormat, str);
		api.SetStr(section, L"CustomFormat", m_pCustomFormat);
	}
	if(!preview){
		api.DelKey(L"Preview");
		m_transition=0;
	}else
		m_transition=1;
}
//================================================================================================
//-------------------------------------------+++--> When User's Location (Locale ComboBox) Changes:
void OnLocale(HWND hDlg)   //---------------------------------------------------------------+++-->
{
	HWND locale_cb = GetDlgItem(hDlg, IDC_LOCALE);
	char checks[FORMAT_NUM];
	wchar_t fmt[MAX_FORMAT];
	int ilang = (int)ComboBox_GetItemData(locale_cb, ComboBox_GetCurSel(locale_cb));
	// change locale
	ChecksLocaleInit(checks, ilang);
	Checks2Dialog(checks, hDlg, 1);
	// update format
	if(!IsDlgButtonChecked(hDlg, IDC_CUSTOM)){
		CreateFormat(fmt, checks);
		SetDlgItemText(hDlg, IDC_FORMAT, fmt); // SendPSChanged
	}else
		SendPSChanged(hDlg);
}
//================================================================================================
//-----------------------------------+++--> Handler for Enable/Disable "Customize format" CheckBox:
void OnCustom(HWND hDlg, BOOL bmouse)   //--------------------------------------------------+++-->
{
	int use_custom;
	int i;
	
	use_custom = IsDlgButtonChecked(hDlg, IDC_CUSTOM);
	Edit_SetReadOnly(GetDlgItem(hDlg,IDC_FORMAT), !use_custom);
	
	for(i = IDC_YEAR4; i <= IDC_12HOUR; i++)
		EnableDlgItem(hDlg, i, !use_custom);
	
	if(m_pCustomFormat && bmouse) {
		if(use_custom) {
			if(m_pCustomFormat[0])
				SetDlgItemText(hDlg, IDC_FORMAT, m_pCustomFormat);
		} else {
			GetDlgItemText(hDlg, IDC_FORMAT, m_pCustomFormat, MAX_FORMAT);
		}
	}
	
	if(!use_custom)
		OnFormatCheck(hDlg, 0);
	SendPSChanged(hDlg);
}
/*------------------------------------------------
  When clicked "year" -- "am/pm"
--------------------------------------------------*/

void OnFormatCheck(HWND hDlg, WORD id)
{
	int i;
	wchar_t fmt[MAX_FORMAT];
	char checks[FORMAT_NUM];
	char oldtransition = m_transition;
	m_transition = -1; // start transition lock
	
	for(i = IDC_YEAR4; i <= IDC_12HOUR; ++i) {
		CHECKS(i) = (char)IsDlgButtonChecked(hDlg, i);
	}
	
	if(id == IDC_YEAR4 || id == IDC_YEAR) {
		if(id == IDC_YEAR4 && CHECKS(IDC_YEAR4)) {
			CheckRadioButton(hDlg, IDC_YEAR4, IDC_YEAR, IDC_YEAR4);
			CHECKS(IDC_YEAR) = FALSE;
		}
		if(id == IDC_YEAR && CHECKS(IDC_YEAR)) {
			CheckRadioButton(hDlg, IDC_YEAR4, IDC_YEAR, IDC_YEAR);
			CHECKS(IDC_YEAR4) = FALSE;
		}
	}
	
	else if(id == IDC_MONTH || id == IDC_MONTHS) {
		if(id == IDC_MONTH && CHECKS(IDC_MONTH)) {
			CheckRadioButton(hDlg, IDC_MONTH, IDC_MONTHS, IDC_MONTH);
			CHECKS(IDC_MONTHS) = FALSE;
		}
		if(id == IDC_MONTHS && CHECKS(IDC_MONTHS)) {
			CheckRadioButton(hDlg, IDC_MONTH, IDC_MONTHS, IDC_MONTHS);
			CHECKS(IDC_MONTH) = FALSE;
		}
	}
	
	else if(id == IDC_AMPM) {
		CHECKS(IDC_12HOUR) = 1;
		CheckDlgButton(hDlg, IDC_12HOUR, 1);
	}
	else if(id == IDC_12HOUR && !IsDlgButtonChecked(hDlg,IDC_12HOUR)) {
		CHECKS(IDC_AMPM) = 0;
		CheckDlgButton(hDlg, IDC_AMPM, 0);
	}
	
	CreateFormat(fmt, checks);
	SetDlgItemText(hDlg, IDC_FORMAT, fmt);
	m_transition=oldtransition; // end transition lock
	SendPSChanged(hDlg);
}

/*------------------------------------------------
  Initialize a format string. Called from settings.c
--------------------------------------------------*/
void InitFormat()
{
	wchar_t format_old[LRG_BUFF];
	wchar_t format[LRG_BUFF];
	char checks[FORMAT_NUM];
	
	if(api.GetInt(L"Format", ENTRY(IDC_CUSTOM), 0))
		return;
	ChecksLocaleInit(checks, 0);	
	CreateFormat(format, checks);
	api.GetStr(L"Format", L"Format", format_old, _countof(format_old), L"");
	if(wcscmp(format, format_old))
		api.SetStr(L"Format", L"Format", format);
}

/*--------------------------------------------------
=============== Create a format string automatically
--------------------------------------------------*/
void CreateFormat(wchar_t* dst, char* checks)
{
	const wchar_t* spacer = L" ";
	char use_time = 0; ///< bitmask; 1 = date, 2 = time
	int control;
	int creation_bit; ///< date/time bits; &1 = date, !&1 = time
	
	creation_bit = 1<<2 | 0<<1 | 1<<0; // Date+Time; old T-Clock default (0b101)
	
	for(control=IDC_YEAR4; control<=IDC_WEEKDAY; ++control) {
		if(CHECKS(control)) {
			use_time |= 1;
			break;
		}
	}
	
	for(control=IDC_HOUR; control<=IDC_INTERNETTIME; ++control) {
		if(CHECKS(control)) {
			use_time |= 2;
			break;
		}
	}
	
	if(use_time == 3) { // both bits (date&time) set
		if(CHECKS(IDC_LINEFEED)){
			spacer = L"\\n";
			if(CHECKS(IDC_LINEFEED) == BST_INDETERMINATE)
				creation_bit = 1<<2 | 1<<1 | 0<<0; // Time+Date; Vista+ (0b110)
		}
	}
	
	*dst = '\0';
	
	do {
		if(*dst)
			wcscat(dst, spacer);
		if(creation_bit&1) {
			//
			// Date
			//
			if(!m_bDayOfWeekIsLast && CHECKS(IDC_WEEKDAY)) {
				wcscat(dst, L"ddd");
				for(control=IDC_YEAR4; control<=IDC_DAY; ++control) {
					if(CHECKS(control)) {
						if((m_ilang&0x00ff) == LANG_CHINESE) wcscat(dst, L" ");
						else if(m_sMon[0] && m_sMon[ wcslen(m_sMon) - 1 ] == '.')
							wcscat(dst, L" ");
						else wcscat(dst, L", ");
						break;
					}
				}
			}

			switch(m_idate){
			case 0: // m/d/y
				if(CHECKS(IDC_MONTH) || CHECKS(IDC_MONTHS)) {
					if(CHECKS(IDC_MONTH)) wcscat(dst, L"mm");
					if(CHECKS(IDC_MONTHS)) wcscat(dst, L"mmm");
					if(CHECKS(IDC_DAY) || CHECKS(IDC_YEAR4) || CHECKS(IDC_YEAR)) {
						if(CHECKS(IDC_MONTH)) wcscat(dst, m_sep_date);
						else wcscat(dst, L" ");
					}
				}
				if(CHECKS(IDC_DAY)) {
					wcscat(dst, L"dd");
					if(CHECKS(IDC_YEAR4) || CHECKS(IDC_YEAR)) {
						if(CHECKS(IDC_MONTH)) wcscat(dst, m_sep_date);
						else wcscat(dst, L", ");
					}
				}
				if(CHECKS(IDC_YEAR4)) wcscat(dst, L"yyyy");
				if(CHECKS(IDC_YEAR)) wcscat(dst, L"yy");
				break;
			case 1: // d/m/y
				if(CHECKS(IDC_DAY)) {
					wcscat(dst, L"dd");
					if(CHECKS(IDC_MONTH) || CHECKS(IDC_MONTHS)) {
						if(CHECKS(IDC_MONTH)) wcscat(dst, m_sep_date);
						else wcscat(dst, L" ");
					} else if(CHECKS(IDC_YEAR4) || CHECKS(IDC_YEAR)) wcscat(dst, m_sep_date);
				}
				if(CHECKS(IDC_MONTH) || CHECKS(IDC_MONTHS)) {
					if(CHECKS(IDC_MONTH)) wcscat(dst, L"mm");
					if(CHECKS(IDC_MONTHS)) wcscat(dst, L"mmm");
					if(CHECKS(IDC_YEAR4) || CHECKS(IDC_YEAR)) {
						if(CHECKS(IDC_MONTH)) wcscat(dst, m_sep_date);
						else wcscat(dst, L" ");
					}
				}
				if(CHECKS(IDC_YEAR4)) wcscat(dst, L"yyyy");
				if(CHECKS(IDC_YEAR)) wcscat(dst, L"yy");
				break;
			default:  // y/m/d
				if(CHECKS(IDC_YEAR4) || CHECKS(IDC_YEAR)) {
					if(CHECKS(IDC_YEAR4)) wcscat(dst, L"yyyy");
					if(CHECKS(IDC_YEAR)) wcscat(dst, L"yy");
					if(CHECKS(IDC_MONTH) || CHECKS(IDC_MONTHS)
					   || CHECKS(IDC_DAY)) {
						if(CHECKS(IDC_MONTHS)) wcscat(dst, L" ");
						else wcscat(dst, m_sep_date);
					}
				}
				if(CHECKS(IDC_MONTH) || CHECKS(IDC_MONTHS)) {
					if(CHECKS(IDC_MONTH)) wcscat(dst, L"mm");
					if(CHECKS(IDC_MONTHS)) wcscat(dst, L"mmm");
					if(CHECKS(IDC_DAY)) {
						if(CHECKS(IDC_MONTHS)) wcscat(dst, L" ");
						else wcscat(dst, m_sep_date);
					}
				}
				if(CHECKS(IDC_DAY)) wcscat(dst, L"dd");
			}

			if(m_bDayOfWeekIsLast && CHECKS(IDC_WEEKDAY)) {
				for(control=IDC_YEAR4; control<=IDC_DAY; ++control) {
					if(CHECKS(control)) { wcscat(dst, L" "); break; }
				}
				wcscat(dst, L"ddd");
			}
		} else {
			//
			// Time
			//
			if(m_bTimeMarkerIsFirst && CHECKS(IDC_AMPM)) {
				wcscat(dst, L"tt");
				if(use_time&2) wcscat(dst, L" ");
			}
			use_time = 0;
			
			if(CHECKS(IDC_HOUR)) {
				if(CHECKS(IDC_12HOUR)){
					if(CHECKS(IDC_HOUR) != BST_INDETERMINATE)
						wcscat(dst, L"h");
					else
						wcscat(dst, L"hh");
				}else{
					if(CHECKS(IDC_HOUR) != BST_INDETERMINATE) // reversed logic for compatibility and simpler default values
						wcscat(dst, L"HH");
					else
						wcscat(dst, L"H");
				}
				++use_time;
			}
			if(CHECKS(IDC_MINUTE)) {
				if(use_time++) wcscat(dst, m_sep_time);
				wcscat(dst, L"nn");
			}
			if(CHECKS(IDC_SECOND)) {
				if(use_time++) wcscat(dst, m_sep_time);
				wcscat(dst, L"ss");
			}
			
			if(!m_bTimeMarkerIsFirst && CHECKS(IDC_AMPM)) {
				if(use_time) wcscat(dst, L" ");
				wcscat(dst, L"tt");
			}
			
			if(CHECKS(IDC_INTERNETTIME)){
				if(use_time||CHECKS(IDC_AMPM)) wcscat(dst, L" ");
				wcscat(dst, L"@@@");
				if(CHECKS(IDC_INTERNETTIME) == BST_INDETERMINATE)
					wcscat(dst, L".@@");
			}
		}
		creation_bit >>= 1;
	}while(creation_bit != 1);
}
