//---[s]--- For InternetTime 99/03/16@211 M.Takemura -----

/*------------------------------------------------------------------------
// format.c : to make a string to display in the clock -> KAZUBON 1997-1998
//-----------------------------------------------------------------------*/
// Last Modified by Stoic Joker: Friday, 12/16/2011 @ 3:36:00pm
#include "tcdll.h"

static DWORD m_codepage = CP_ACP;
static wchar_t m_MonthShort[11], m_MonthLong[31];
static wchar_t m_DayOfWeekShort[11], m_DayOfWeekLong[31];
static wchar_t* m_DayOfWeekEng[7] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
static wchar_t* m_MonthEng[12] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
static wchar_t m_AM[10], m_PM[10];
static wchar_t m_EraStr[11];
static int m_AltYear;

extern char g_bHourZero;

//================================================================================================
//---------------------------------//+++--> load Localized Strings for Month, Day, & AM/PM Symbols:
void InitFormat(const wchar_t* section, SYSTEMTIME* lt)   //--------------------------------------------------------+++-->
{
	wchar_t year[6];
	int i, ilang, ioptcal;
	
	ilang = api.GetInt(section, L"Locale", GetUserDefaultLangID());
	
	GetLocaleInfo(ilang, LOCALE_IDEFAULTANSICODEPAGE|LOCALE_RETURN_NUMBER, (wchar_t*)&m_codepage, sizeof(m_codepage));
	if(!IsValidCodePage(m_codepage)) m_codepage=CP_ACP;
	
	i = lt->wDayOfWeek - 1;
	if(i < 0) i = 6;
	
	GetLocaleInfo(ilang, LOCALE_SABBREVDAYNAME1 + i, m_DayOfWeekShort, _countof(m_DayOfWeekShort));
//	GetLocaleInfo(ilang, LOCALE_SSHORTESTDAYNAME1 + i, DayOfWeekShort, _countof(DayOfWeekShort)); // Vista+
	GetLocaleInfo(ilang, LOCALE_SDAYNAME1 + i, m_DayOfWeekLong, _countof(m_DayOfWeekLong));
	
	i = lt->wMonth; i--;
	GetLocaleInfo(ilang, LOCALE_SABBREVMONTHNAME1 + i, m_MonthShort, _countof(m_MonthShort));
	GetLocaleInfo(ilang, LOCALE_SMONTHNAME1 + i, m_MonthLong, _countof(m_MonthLong));
	
	api.GetStr(section, L"AMsymbol", m_AM, _countof(m_AM), L"");
	if(!*m_AM){
		GetLocaleInfo(ilang, LOCALE_S1159, m_AM, _countof(m_AM));
		if(!m_AM[0]) wcscpy(m_AM, L"AM");
	}
	api.GetStr(section, L"PMsymbol", m_PM, _countof(m_PM), L"");
	if(!*m_PM){
		GetLocaleInfo(ilang, LOCALE_S2359, m_PM, _countof(m_PM));
		if(!m_PM[0]) wcscpy(m_PM, L"PM");
	}
	
	m_AltYear = -1;
	
	if(!GetLocaleInfo(ilang, LOCALE_IOPTIONALCALENDAR|LOCALE_RETURN_NUMBER, (wchar_t*)&ioptcal, sizeof(ioptcal)))
		ioptcal = 0;
	
	if(ioptcal < 3) ilang = LANG_USER_DEFAULT;
	
	if(!GetDateFormat(ilang, DATE_USE_ALT_CALENDAR, lt, L"gg", m_EraStr, _countof(m_EraStr)))
		*m_EraStr = '\0';
	
	if(GetDateFormat(ilang, DATE_USE_ALT_CALENDAR, lt, L"yyyy", year, _countof(year)))
		m_AltYear = _wtoi(year);
}

__pragma(warning(push))
__pragma(warning(disable:4701)) // MSVC is confused with our S(..) format (uptime) about "num" being "uninitialized"
//================================================================================================
//-------------+++--> Format T-Clock's OutPut String From Current Date, Time, & System Information:
unsigned MakeFormat(wchar_t buf[FORMAT_MAX_SIZE], const wchar_t* fmt, SYSTEMTIME* pt, int beat100)   //------------------+++-->
{
	const wchar_t* bufend = buf+FORMAT_MAX_SIZE;
	const wchar_t* pos;
	wchar_t* out = buf;
	ULONGLONG TickCount = 0;
	//used to adjust the time in the case of a fractional 'w' timezone modifier
	int minute = 0;
	int hour = pt->wHour;

	while(*fmt) {
		if(*fmt == '"') {
			for(++fmt; *fmt&&*fmt!='"'; )
				*out++ = *fmt++;
			if(*fmt) ++fmt;
			continue;
		}
		if(*fmt=='\\' && fmt[1]=='n') {
			fmt+=2;
			*out++='\n';
		}
		/// for testing
		else if(*fmt == 'S' && fmt[1] == 'S' && (fmt[2] == 'S' || fmt[2] == 's')) {
			fmt += 3;
			out += api.WriteFormatNum(out, (int)pt->wSecond, 2, 0);
			*out++ = '.';
			out += api.WriteFormatNum(out, (int)pt->wMilliseconds, 3, 0);
		}
		
		else if(*fmt == 'y' && fmt[1] == 'y') {
			int len;
			len = 2;
			if(*(fmt + 2) == 'y' && *(fmt + 3) == 'y') len = 4;
			
			out += api.WriteFormatNum(out, (len==2)?(int)pt->wYear%100:(int)pt->wYear, len, 0);
			fmt += len;
			hour = pt->wHour;
		} else if(*fmt == 'm') {
			if(*(fmt + 1) == 'm' && *(fmt + 2) == 'e') {
				*out++ = m_MonthEng[pt->wMonth-1][0];
				*out++ = m_MonthEng[pt->wMonth-1][1];
				*out++ = m_MonthEng[pt->wMonth-1][2];
				fmt += 3;
			} else if(fmt[1] == 'm' && fmt[2] == 'm') {
				if(*(fmt + 3) == 'm') {
					fmt += 4;
					for(pos=m_MonthLong; *pos; ) *out++=*pos++;
				} else {
					fmt += 3;
					for(pos=m_MonthShort; *pos; ) *out++=*pos++;
				}
			} else {
				if(fmt[1] == 'm') {
					fmt += 2;
					*out++ = (wchar_t)((int)pt->wMonth / 10) + '0';
				} else {
					++fmt;
					if(pt->wMonth > 9)
						*out++ = (wchar_t)((int)pt->wMonth / 10) + '0';
				}
				*out++ = (wchar_t)((int)pt->wMonth % 10) + '0';
			}
			hour = pt->wHour;
		} else if(*fmt == 'a' && fmt[1] == 'a' && fmt[2] == 'a') {
			fmt += 3;
			if(*fmt == 'a') {
				++fmt;
				pos = m_DayOfWeekLong;
			} else {
				pos = m_DayOfWeekShort;
			}
			for(; *pos; *out++ = *pos++);
			hour = pt->wHour;
		} else if(*fmt=='d') {
			if(fmt[1]=='d' && fmt[2]=='e'){
				fmt+=3;
				for(pos=m_DayOfWeekEng[pt->wDayOfWeek]; *pos; ) *out++=*pos++;
			}else if(fmt[1]=='d' && fmt[2]=='d') {
				fmt += 3;
				if(*fmt == 'd'){
					++fmt;
					pos = m_DayOfWeekLong;
				}else{
					pos = m_DayOfWeekShort;
				}
				for(; *pos; *out++ = *pos++);
			}else{
				if(fmt[1]=='d') {
					fmt+=2;
					*out++ = (wchar_t)((int)pt->wDay / 10) + '0';
				}else{
					++fmt;
					if(pt->wDay > 9)
						*out++ = (wchar_t)((int)pt->wDay / 10) + '0';
				}
				*out++ = (wchar_t)((int)pt->wDay % 10) + '0';
			}
			hour = pt->wHour;
		} else if(*fmt=='h') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			hour = pt->wHour;
			while(hour >= 12) // faster than mod 12 if "hour" <= 24
				hour -= 12;
			if(!hour && !g_bHourZero)
				hour = 12;
			if(fmt[1] == 'h') {
				fmt += 2;
				*out++ = (wchar_t)(hour / 10) + '0';
			} else {
				++fmt;
				if(hour > 9)
					*out++ = (wchar_t)(hour / 10) + '0';
			}
			*out++ = (wchar_t)(hour % 10) + '0';
			hour = pt->wHour;
		} else if(*fmt=='H') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			hour = pt->wHour;
			if(fmt[1] == 'H') {
				fmt += 2;
				*out++ = (wchar_t)(hour / 10) + '0';
			} else {
				++fmt;
				if(hour > 9)
					*out++ = (wchar_t)(hour / 10) + '0';
			}
			*out++ = (wchar_t)(hour % 10) + '0';
		} else if((*fmt=='w'||*fmt=='W') && (fmt[1]=='+'||fmt[1]=='-')) {
			char is_12h = (*fmt == 'w');
			char is_negative = (*++fmt == '-');
			minute = 0;
			hour = 0;
			float frac = 0;
			for(; ((*++fmt >= '0' && *fmt<='9') || *fmt=='.'); ){
				if (*fmt == '.')
				{
					frac = 0.1;
				}
				else if(frac==0)
				{
					//no fractional part yet
					hour *= 10;
					hour += *fmt - '0';
				}
				else 
				{
					//there's a fractional part
					minute += frac * (*fmt - '0') * 60;
					frac *= frac;
				}
			}
			if (is_negative)
			{
				minute = -minute;
				//adjust hour backwards if timezone minute would go negative
				if (((int)pt->wMinute + minute) < 0) hour++;
				hour = -hour;
			}
			hour = (pt->wHour + hour)%24;
			if(hour < 0) hour += 24;
			int adjhour = hour;
			if(is_12h){
				while(hour >= 12) // faster than mod 12 if "hour" <= 24
					hour -= 12;
				if(!hour && !g_bHourZero)
					hour = 12;
			}
			*out++ = (wchar_t)(hour / 10) + '0';
			*out++ = (wchar_t)(hour % 10) + '0';
			hour = adjhour;
		} else if(*fmt == 'n') {
			//adjust for timezone minute offset if any
			minute = minute + (int)pt->wMinute;
			//if minute went negative, make it positive
			if (minute < 0) minute += 60;
			minute = minute % 60;
			if(fmt[1] == 'n') {
				fmt += 2;
				*out++ = (wchar_t)(minute / 10) + '0';
			} else {
				++fmt;
				if(minute > 9)
					*out++ = (wchar_t)(minute / 10) + '0';
			}
			*out++ = (wchar_t)(minute % 10) + '0';
		} else if(*fmt == 's') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			if(fmt[1] == 's') {
				fmt += 2;
				*out++ = (wchar_t)((int)pt->wSecond / 10) + '0';
			} else {
				++fmt;
				if(pt->wSecond > 9)
					*out++ = (wchar_t)((int)pt->wSecond / 10) + '0';
			}
			*out++ = (wchar_t)((int)pt->wSecond % 10) + '0';
		} else if(*fmt == 't' && fmt[1] == 't') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			fmt += 2;
			if(hour < 12) pos = m_AM; else pos = m_PM;
			while(*pos) *out++ = *pos++;
			hour = pt->wHour;
		} else if(*fmt == 'A' && fmt[1] == 'M') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			if(fmt[2] == '/' &&
			   fmt[3] == 'P' && fmt[4] == 'M') {
				if(hour < 12) *out++ = 'A'; //--+++--> 2010 - Noon / MidNight Decided Here!
				else *out++ = 'P';
				*out++ = 'M'; fmt += 5;
			} else if(fmt[2] == 'P' && fmt[3] == 'M') {
				fmt += 4;
				if(hour < 12) pos = m_AM; else pos = m_PM;
				while(*pos) *out++ = *pos++;
			} else *out++ = *fmt++;
			hour = pt->wHour;
		} else if(*fmt == 'a' && fmt[1] == 'm' && fmt[2] == '/' &&
				  fmt[3] == 'p' && fmt[4] == 'm') {
			minute = 0; //once we hit this, reset the timezone minute adjustment
			if(hour < 12) *out++ = 'a';
			else *out++ = 'p';
			*out++ = 'm'; fmt += 5;
			hour = pt->wHour;
		}
		// internet time
		else if(*fmt == '@' && fmt[1] == '@' && fmt[2] == '@') {
			fmt += 3;
			*out++ = '@';
			*out++ = (wchar_t)(beat100 / 10000) + '0';
			*out++ = (wchar_t)((beat100 % 10000) / 1000) + '0';
			*out++ = (wchar_t)((beat100 % 1000) / 100) + '0';
			if(*fmt=='.' && fmt[1]=='@') {
				fmt += 2;
				*out++ = '.';
				*out++ = (wchar_t)((beat100 % 100) / 10) + '0';
				if(*fmt=='@'){
					++fmt;
					*out++ = (wchar_t)((beat100 % 10)) + '0';
				}
			}
			hour = pt->wHour;
		}
		// alternate calendar
		else if(*fmt == 'Y' && m_AltYear > -1) {
			int n = 1;
			while(*fmt == 'Y') { n *= 10; ++fmt; }
			if(n < m_AltYear) {
				n = 1; while(n < m_AltYear) n *= 10;
			}
			for(;;) {
				*out++ = (wchar_t)((m_AltYear % n) / (n/10)) + '0';
				if(n == 10) break;
				n /= 10;
			}
		} else if(*fmt == 'g') {
			for(pos=m_EraStr; *pos&&*fmt=='g'; ){
				*out++ = *pos++;
				++fmt;
			}
			while(*fmt == 'g') fmt++;
		}
		
		else if(*fmt == 'L' && wcsncmp(fmt, L"LDATE", 5) == 0) {
			GetDateFormat(LOCALE_USER_DEFAULT,
						  DATE_LONGDATE, pt, NULL, out, (int)(bufend-out));
			for(; *out; ++out);
			fmt += 5;
		}
		
		else if(*fmt == 'D' && wcsncmp(fmt, L"DATE", 4) == 0) {
			GetDateFormat(LOCALE_USER_DEFAULT,
						  DATE_SHORTDATE, pt, NULL, out, (int)(bufend-out));
			for(; *out; ++out);
			fmt += 4;
		}
		
		else if(*fmt == 'T' && wcsncmp(fmt, L"TIME", 4) == 0) {
			GetTimeFormat(LOCALE_USER_DEFAULT,
						  0, pt, NULL, out, (int)(bufend-out));
			for(; *out; ++out);
			fmt += 4;
			hour = pt->wHour;
		} else if(*fmt == 'S') { // uptime
			int width, padding, num;
			const wchar_t* old_fmt = ++fmt;
			wchar_t specifier = api.GetFormat(&fmt, &width, &padding);
			if(!TickCount) TickCount = api.GetTickCount64();
			switch(specifier){
			case 'd'://days
				num = (int)(TickCount/86400000);
				break;
			case 'a'://hours total
				num = (int)(TickCount/3600000);
				break;
			case 'h'://hours (max 24)
				num = (TickCount/3600000)%24;
				break;
			case 'n'://minutes
				num = (TickCount/60000)%60;
				break;
			case 's'://seconds
				num = (TickCount/1000)%60;
				break;
			case 'T':{// ST, uptime as h:mm:ss
				ULONGLONG past = TickCount/1000;
				int hour, minute;
				num = past%60; past /= 60;
				minute = past%60; past /= 60;
				hour = (int)past;
				
				out += api.WriteFormatNum(out, hour, width, padding);
				*out++ = ':';
				out += api.WriteFormatNum(out, minute, 2, 0);
				*out++ = ':';
				width = 2; padding = 0;
				break;}
			default:
				specifier = '\0';
				fmt = old_fmt;
				*out++ = 'S';
			}
			if(specifier)
				out += api.WriteFormatNum(out, num, width, padding);
			hour = pt->wHour;
		} else if(fmt[0] == 'w') { // numeric Day-of-Week
			int weekday = pt->wDayOfWeek;
			++fmt;
			if(*fmt == 'i' || *fmt == 'u') {
				if(!weekday && *fmt == 'i')
					weekday = 7;
				++fmt;
				*out++ = '0' + (char)weekday;
			} else {
				*out++ = 'w';
			}
			hour = pt->wHour;
		} else if(*fmt == 'W') { // Week-of-Year
			char buf[4];
			int width, padding, num;
			wchar_t specifier;
			struct tm tmnow;
			time_t ts = time(NULL);
			localtime_r(&ts, &tmnow);
			api.GetFormat(&fmt, &width, &padding);
			specifier = *fmt;
			switch(specifier){
			case 's': // Week-Of-Year Starts Sunday
				strftime(buf, _countof(buf), "%U", &tmnow);
				num = atoi(buf);
				break;
			case 'm': // Week-Of-Year Starts Monday
				strftime(buf, _countof(buf), "%W", &tmnow);
				num = atoi(buf);
				break;
			case 'i':{ // ISO-8601 week (1st version by henriko.se, 2nd by White-Tiger)
				int wday, borderdays, week;
				for(;;){
					wday = (!tmnow.tm_wday ? 6 : tmnow.tm_wday - 1); // convert from Sun-Sat to Mon-Sun (0-5)
					borderdays = (1 + tmnow.tm_yday + 6 - wday) % 7;
					week = (1 + tmnow.tm_yday + 6 - wday) / 7;
					if(borderdays >= 4){ // year starts with at least 4 days
						++week;
					} else if(!week){ // we're still in last year's week
						--tmnow.tm_year;
						tmnow.tm_mon = 11;
						tmnow.tm_mday = 31;
						tmnow.tm_isdst = -1;
						if(mktime(&tmnow)==-1){ // mktime magically updates tm_yday, tm_wday
							week = 1;
							break; // fail safe
						}
						tmnow.tm_mon = 0; // just to speed up the "if" below, since we know that it can't be week 1
						continue; // repeat (once)
					}
					if(tmnow.tm_mon==11 && tmnow.tm_mday>=29){ // end of year, could be week 1
						borderdays = 31 - tmnow.tm_mday + wday;
						if(borderdays < 3)
							week = 1;
					}
					break;
				}
				num = week;
				hour = pt->wHour;
				break;}
			case 'u': // U.S. like Week-of-Year - beginning on first day of the year, based on sunday
				num = (1 + (tmnow.tm_yday + 6 - tmnow.tm_wday) / 7);
				hour = pt->wHour;
				break;
			case 'w': // SWN (Simple Week Number)
				num = (1 + tmnow.tm_yday / 7);
				hour = pt->wHour;
				break;
			default:
				specifier = '\0';
				*out++ = 'W';
				hour = pt->wHour;
			}
			if(specifier) {
				++fmt;
				out += api.WriteFormatNum(out, num, width, padding);
			}
		}
//================================================================================================
//======================================= JULIAN DATE Code ========================================
		else if(*fmt == 'J' && *(fmt + 1) == 'D') {
			double y, M, d, h, m, s, bc, JD;
			struct tm Julian;
			int id, is, i;
			char* szJulian;
			time_t UTC = time(NULL);
			
			gmtime_r(&UTC, &Julian);
			
			y = Julian.tm_year +1900;	// Year
			M = Julian.tm_mon +1;		// Month
			d = Julian.tm_mday;			// Day
			h = Julian.tm_hour;			// Hours
			m = Julian.tm_min;			// Minutes
			s = Julian.tm_sec;			// Seconds
			// This Handles the January 1, 4713 B.C up to
			bc = 100.0 * y + M - 190002.5; // Year 0 Part.
			JD = 367.0 * y;
			
			JD -= floor(7.0*(y + floor((M+9.0)/12.0))/4.0);
			JD += floor(275.0*M/9.0);
			JD += d;
			JD += (h + (m + s/60.0)/60.0)/24.0;
			JD += 1721013.5; // BCE 2 November 18 00:00:00.0 UT - Tuesday
			JD -= 0.5*bc/fabs(bc);
			JD += 0.5;
			
			szJulian = _fcvt(JD, 4, &id, &is); // Make it a String
			for(i=0; szJulian[0]; ++i) {
				if(i == id) { //--//-++-> id = Decimal Point Precision/Position
					*out++ = '.'; // ReInsert the Decimal Point Where it Belongs.
				} else {
					*out++ = *szJulian++; //--+++--> Done!
				}
			}
			fmt +=2;
			hour = pt->wHour;
		}
//================================================================================================
//======================================= ORDINAL DATE Code =======================================
		else if(*fmt == 'O' && *(fmt + 1) == 'D') { //--------+++--> Ordinal Date UTC:
			struct tm today;
			time_t UTC = time(NULL);
			
			gmtime_r(&UTC, &today);
			out += wcsftime(out, 16, L"%Y-%j", &today);
			fmt +=2;
			hour = pt->wHour;
		}
		//==========================================================================
		else if(*fmt == 'O' && *(fmt + 1) == 'd') { //------+++--> Ordinal Date Local:
			struct tm today;
			time_t ts = time(NULL);
			
			localtime_r(&ts, &today);
			out += wcsftime(out, 16, L"%Y-%j", &today);
			fmt +=2;
			hour = pt->wHour;
		}
		//==========================================================================
		else if(*fmt == 'D' && wcsncmp(fmt, L"DOY", 3) == 0) { //--+++--> Day-Of-Year:
			struct tm today;
			time_t ts = time(NULL);
			
			localtime_r(&ts, &today);
			out += wcsftime(out, 8, L"%j", &today);
			fmt +=3;
			hour = pt->wHour;
		}
		//==========================================================================
		else if(*fmt == 'P' && wcsncmp(fmt, L"POSIX", 5) == 0) { //-> Posix/Unix Time:
			out += wsprintf(out, FMT("%") FMT(PRIi64), (int64_t)time(NULL));
			fmt +=5;
			hour = pt->wHour;
		}
		//==========================================================================
		else if(*fmt == 'T' && wcsncmp(fmt, L"TZN", 3) == 0) { //--++-> TimeZone Name:
			#ifndef __GNUC__ /* forces us to link with msvcr100 */
			char szTZName[TZNAME_MAX] = {0};
			size_t lRet;
			char* tzn;
			int iDST;
			
			_get_daylight(&iDST);
			if(iDST) {
				_get_tzname(&lRet, szTZName, _countof(szTZName), 1);
			} else {
				_get_tzname(&lRet, szTZName, _countof(szTZName), 0);
			}
			
			tzn = szTZName;
			while(*tzn) *out++ = *tzn++;
			#endif
			fmt +=3;
			hour = pt->wHour;
		}
//=================================================================================================
		else {
			*out++ = *fmt++;
		}
	}
	*out='\0';
	return (unsigned)(out-buf);
}
__pragma(warning(pop))

/*--------------------------------------------------
--------------------------------------- Check Format
--------------------------------------------------*/
DWORD FindFormat(const wchar_t* fmt)
{
	DWORD ret = 0;
	
	while(*fmt) {
		if(*fmt == '"') {
			do{
				for(++fmt; *fmt&&*fmt++!='"'; );
			}while(*fmt == '"');
			if(!*fmt)
				break;
		}
		
		else if(*fmt == 's') {
			fmt++;
			ret |= FORMAT_SECOND;
		}
		else if(*fmt == 'T' && wcsncmp(fmt, L"TIME", 4) == 0) {
			fmt += 4;
			ret |= FORMAT_SECOND;
		}
		
		else if(*fmt == '@' && fmt[1] == '@' && fmt[2] == '@') {
			fmt += 3;
			if(*fmt == '.' && fmt[1] == '@') {
				ret |= FORMAT_BEAT2;
				fmt += 2;
			} else ret |= FORMAT_BEAT1;
		}
		
		else ++fmt;
	}
	return ret;
}
