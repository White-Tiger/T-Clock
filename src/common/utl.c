/*-------------------------------------------
  utl.c - KAZUBON 1997-1999
---------------------------------------------*/
// Modified by Stoic Joker: Monday, 03/22/2010 @ 7:32:29pm
#include "globals.h"
#include "utl.h"
#include "version.h"
#include <tlhelp32.h>

#ifdef LOGGING
#	include <time.h>
#	include <shlobj.h> // SHGetFolderPath()
#endif // LOGGING

int IsRunAsAdmin()
{
	int is_admin = 0;
	char admin_group[SECURITY_MAX_SID_SIZE];
	DWORD cbSize = sizeof(admin_group);
	if(CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, &admin_group, &cbSize)){
		CheckTokenMembership(NULL, admin_group, &is_admin);
	}
	return is_admin;
}
int IsUserInAdminGroup()
{
	int is_admin = 0;
	char admin_group[SECURITY_MAX_SID_SIZE];
	DWORD cbSize;
	HANDLE hToken;
	HANDLE hTokenTest = NULL;
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)){
		// process might run with stripped down rights (that is, if UAC is active)
		if(api.OS >= TOS_VISTA){
			TOKEN_ELEVATION_TYPE elevType;
			if(GetTokenInformation(hToken, TokenElevationType, &elevType, sizeof(elevType), &cbSize)){
				if(elevType == TokenElevationTypeLimited){
					if(!GetTokenInformation(hToken, TokenLinkedToken, &hTokenTest, sizeof(hTokenTest), &cbSize)){
						CloseHandle(hToken);
						return 0;
					}
				}
			}
		}
		// no UAC involved? impersonate our primary token for use by CheckTokenMembership()
		if(!hTokenTest){
			if(!DuplicateToken(hToken, SecurityIdentification, &hTokenTest)){
				hTokenTest = NULL;
			}
		}
		if(hTokenTest){
			cbSize = sizeof(admin_group);
			if(CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, &admin_group, &cbSize)){
				CheckTokenMembership(hTokenTest, admin_group, &is_admin);
			}
			CloseHandle(hTokenTest);
		}
		CloseHandle(hToken);
	}
	return is_admin;
}

unsigned GetParentProcess(unsigned pid) {
	unsigned ppid = 0;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe32 = {sizeof(PROCESSENTRY32)};
	if(Process32First(snap,&pe32)) {
		do {
			if(pe32.th32ProcessID == pid) {
				ppid = pe32.th32ParentProcessID;
				break;
			}
		}while(Process32Next(snap,&pe32));
	}
	CloseHandle(snap);
	return ppid;
}
//===================================================================================
//-------------------------------------+++--> Force a ReDraw of T-Clock & the TaskBar:
void RefreshUs()   //----------------------------------------------------------+++-->
{
	HWND hclock = api.GetClock(0);
	if(hclock){
		SendMessage(hclock,CLOCKM_REFRESHCLOCK,0,0);
		SendMessage(hclock,CLOCKM_REFRESHTASKBAR,0,0);
	}
}

int atox(const char* p)
{
	int r = 0;
	for(; *p; ++p) {
		if('0' <= *p && *p <= '9') r=(r<<4) + *p-'0';
		else if('A' <= *p && *p <= 'F') r=(r<<4) + *p-('A'-10);
		else if('a' <= *p && *p <= 'f') r=(r<<4) + *p-('a'-10);
		else break;
	}
	return r;
}
int wtox(const wchar_t* p)
{
	int r = 0;
	for(; *p; ++p) {
		if('0' <= *p && *p <= '9') r=(r<<4) + *p-'0';
		else if('A' <= *p && *p <= 'F') r=(r<<4) + *p-('A'-10);
		else if('a' <= *p && *p <= 'f') r=(r<<4) + *p-('a'-10);
		else break;
	}
	return r;
}

int _24hTo12h(int hour){
	hour %= 24;
	if(hour > 12)
		hour -= 12;
	else
		hour = (!hour?12:hour);
	return hour;
}
int _12hTo24h(int hour, int pm){
	if(hour >= 12)
		hour = (pm?12:0);
	else if(pm)
		hour += 12;
	return hour;
}

void add_title(wchar_t* path, const wchar_t* title)
{
	wchar_t* p = path;
	if(*path && (!*title || title[1]!=':')){ // not absolute device path
		if(title[0]=='\\' || title[0]=='/') { // absolute path
			if(p[0] && p[1]==':') p += 2;
		}else{ // relative path
			for(; p[0]; ++p) {
				if((p[0]=='\\' || p[0]=='/') && !p[1]) {
					break;
				}
			}
			*p++ = '\\';
		}
	}
	while(*title) *p++ = *title++;
	*p = '\0';
}

void del_title(wchar_t* path)
{
	wchar_t* p,* ep;
	
	for(p=ep=path; p[0]; ++p) {
		if(p[0]=='\\' || p[0]=='/') {
			if(p>path && p[-1]==':') ep = p+1;
			else ep = p;
		}
	}
	*ep = '\0';
}

void get_title(wchar_t* dst, const wchar_t* path)
{
	const wchar_t* p,* ep;
	
	for(p=ep=path; p[0]; ++p) {
		if(p[0]=='\\' || p[0]=='/') {
			if(!*++p) break;
			if(p>path && p[-1]==':') ep = p+1;
			else ep = p;
		}
	}
	
	if(ep[0]=='\\' || ep[0]=='/') ++ep;
	
	while(*ep) *dst++ = *ep++;
	if(dst[-1]=='\\' || dst[-1]=='/')
		--dst;
	*dst = '\0';
}

int ext_cmp(const wchar_t* fname, const wchar_t* ext)
{
	const wchar_t* p, *sp;
	
	sp = NULL;
	for(p=fname; p[0]; ++p) {
		if(p[0]=='.') sp = p;
		else if(p[0]=='\\' || p[0]=='/') sp = NULL;
	}
	
	if(!sp) sp = p;
	if(sp[0]=='.') ++sp;
	
	for(; sp[0]||ext[0]; ++sp,++ext) {
		if(toupper(sp[0])!=toupper(ext[0]))
			return (toupper(sp[0]) - toupper(ext[0]));
	}
	return 0;
}
/*
void parse(char* dst, char* src, int n)
{
	char* dp;
	int i;
	
	for(i = 0; i < n; i++) {
		while(*src && *src != ',') src++;
		if(*src == ',') src++;
	}
	if(*src == 0) {
		*dst = 0; return;
	}
	
	while(*src == ' ') src++;
	
	dp = dst;
	while(*src && *src != ',') *dst++ = *src++;
	*dst = 0;
	
	while(dst != dp) {
		dst--;
		if(*dst == ' ') *dst = 0;
		else break;
	}
}// */
/*
void parsechar(char* dst, char* src, char ch, int n)
{
	char* dp;
	int i;
	
	for(i = 0; i < n; i++) {
		while(*src && *src != ch) src++;
		if(*src == ch) src++;
	}
	if(*src == 0) {
		*dst = 0; return;
	}
	
	while(*src == ' ') src++;
	
	dp = dst;
	while(*src && *src != ch) *dst++ = *src++;
	*dst = 0;
	
	while(dst != dp) {
		dst--;
		if(*dst == ' ') *dst = 0;
		else break;
	}
}// */

void str0cat(wchar_t* list, const wchar_t* str)
{
	if(list[0]||list[1]){ // find last string pair
		for(; list[0]||list[1]; ++list);
		++list;
	}
	for(; *str; *list++=*str++); // append new string
	list[0] = list[1] = '\0'; // end string & pair
}

/*---------------------------------------------
--------------------- returns a resource string
---------------------------------------------*/
wchar_t* MyString(UINT id)
{
	static wchar_t buf[80];
	
	*buf = '\0';
	LoadString(GetModuleHandle(NULL), id, buf, _countof(buf));
	return buf;
}
/*
#include <stddef.h>
void ForceForegroundWindow(HWND hwnd)
{
	DWORD fgthread=GetWindowThreadProcessId(GetForegroundWindow(),0);
	if(fgthread && _threadid^fgthread && AttachThreadInput(_threadid,fgthread,1)){
//		AllowSetForegroundWindow(ASFW_ANY);//does nothing... we become foreground, but won't receive window messages
//		SetFocus(hwnd);// "
		SetForegroundWindow(hwnd);
		BringWindowToTop(hwnd);
		AttachThreadInput(_threadid,fgthread,0);
		return;
	}
	SetForegroundWindow(hwnd);
	BringWindowToTop(hwnd);
}// */
//===============================================================================


/*
void Pause(HWND hWnd, LPCTSTR pszArgs)
{
	LONG lInterval = atoi(pszArgs);
	LONG lTime = GetTickCount();
	MSG msg;
	
	if(lInterval > 0) {
		while((LONG)(GetTickCount() - lTime) < lInterval) {
			if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
}// */
/// currently unused drawing stuff
/*
// tile an image vertically
void VerticalTileBlt(HDC hdcDest, int xDest, int yDest, int cxDest, int cyDest,
					 HDC hdcSrc, int xSrc, int ySrc, int cxSrc, int cySrc,
					 BOOL ReverseBlt, BOOL useTrans)
{
	int y;
	
	if(ReverseBlt) {
		for(y = cyDest - cySrc; y > yDest - cySrc; y -= cySrc) {
			TC2DrawBlt(hdcDest,
					   xDest,
					   y,
					   cxDest,
					   cySrc,
					   hdcSrc,
					   xSrc,
					   ySrc,
					   cxSrc,
					   cySrc,
					   useTrans);
		}
	} else {
		for(y = 0; y < cyDest; y += cySrc) {
			TC2DrawBlt(hdcDest,
					   xDest,
					   yDest + y,
					   cxDest,
					   __min(cyDest - y, cySrc),
					   hdcSrc,
					   xSrc,
					   ySrc,
					   cxSrc,
					   __min(cyDest - y, cySrc),
					   useTrans);
		}
	}
}
// tile an image horizontally and vertically
void FillTileBlt(HDC hdcDest, int xDest, int yDest, int cxDest, int cyDest, HDC hdcSrc, int xSrc, int ySrc, int cxSrc, int cySrc, DWORD rasterOp)
{
	int x, y;
	
	for(y = 0; y < cyDest; y += cySrc) {
		for(x = 0; x < cxDest; x += cxSrc) {
			BitBlt(hdcDest,
				   xDest + x,
				   yDest + y,
				   cxSrc,
				   cySrc,
				   hdcSrc,
				   xSrc,
				   ySrc,
				   rasterOp);
		}
	}
}
void TileBlt(HDC hdcDest, int xDest, int yDest, int cxDest, int cyDest, HDC hdcSrc,
			 int xSrc, int ySrc, int cxSrc, int cySrc, BOOL useTrans)
{
	int y, x;
	
	for(y = yDest; y < cyDest; y = y + cySrc) {
		for(x = xDest; x < cxDest; x = x + cxSrc) {
			TC2DrawBlt(hdcDest, x, y, cxSrc, cySrc,
					   hdcSrc, xSrc, ySrc, cxSrc, cySrc, useTrans);
		}
	}
}// */

HWND CreateDialogParamOnce(HWND* hwnd, HINSTANCE hInstance, const wchar_t* lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
	const HWND pending = (HWND)(intptr_t)1;
	HWND hwnd_ = *hwnd;
	if(!hwnd_ || (hwnd_ != pending && !IsWindow(hwnd_))){
		*hwnd = pending;
		*hwnd = CreateDialogParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam);
	} else if(hwnd_ != pending) {
		SetActiveWindow(hwnd_);
	}
	return *hwnd;
}

HBITMAP CreateBitmapWithAlpha(HDC hdc, int width, int height) {
	BITMAPINFO bmi = {{sizeof(bmi.bmiHeader)}};
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biBitCount = 32;
	return CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
}

HBITMAP GetBitmapFromIcon(HICON icon, int size) {
	HDC screen = GetDC(NULL);
	HDC dc = CreateCompatibleDC(screen);
	HBITMAP bitmap;
	HGDIOBJ old;
	int width, height;
	if(size == 0) {
		width = GetSystemMetrics(SM_CXICON);
		height = GetSystemMetrics(SM_CYICON);
	/* } else if(size == -2) {
		// returned values are wrong, eg. 15px instead of 16px at 100% scaling (Win10)
		// see: http://trac.wxwidgets.org/ticket/17290
		width = GetSystemMetrics(SM_CXMENUCHECK);
		height = GetSystemMetrics(SM_CYMENUCHECK);// */
	} else if(size < 0) {
		width = GetSystemMetrics(SM_CXSMICON);
		height = GetSystemMetrics(SM_CYSMICON);
	} else {
		width = height = size;
	}
	bitmap = CreateBitmapWithAlpha(dc, width, height);
	old = SelectObject(dc, bitmap);
	
	ReleaseDC(NULL, screen);
	DrawIconEx(dc, 0, 0, icon, width, height, 0, NULL, DI_NORMAL);
	SelectObject(dc, old);
	DeleteDC(dc);

	return bitmap;
}

#ifdef LOGGING
EXTERN_C IMAGE_DOS_HEADER __ImageBase; ///< own dll/exe handle
typedef struct LOG_SHARE {
	FILE* fp;
	int indent;
} LOG_SHARE;
static HANDLE s_log_share = NULL;
static LOG_SHARE* s_log = NULL;
#define kLogDisabled ((LOG_SHARE*)(intptr_t)-1)
static const char* find_titleA(const char* path) {
	const char* file = NULL;
	for(; path[0]; ++path) {
		if(path[0] == '\\' || path[0] == '/')
			file = path + 1;
	}
	return file ? file : path;
}
void DebugLog(int indent, const char* format, ...) {
	va_list args;
	union u {
		wchar_t path[MAX_PATH];
		char buf[MAX_PATH * sizeof(wchar_t)];
	} u;
	SYSTEMTIME now;
	
	if(s_log == kLogDisabled)
		return;
	if(!s_log) {
		int bfirst;
		if(api.GetInt(L"", L"NoLog", 0) == VER_REVISION) {
			s_log = kLogDisabled;
			return;
		}
		sprintf(u.buf, "TClLg%lx", GetCurrentProcessId());
		s_log_share = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LOG_SHARE), u.buf);
		if(!s_log_share)
			return;
		bfirst = (GetLastError() != ERROR_ALREADY_EXISTS);
		s_log = (LOG_SHARE*)MapViewOfFile(s_log_share, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if(!s_log) {
			DebugLogFree();
			return;
		}
		if(bfirst) {
			s_log->fp = NULL;
			s_log->indent = 0;
			if(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, u.path) == S_OK) {
				add_title(u.path, L"T-Clock.log");
				s_log->fp = _wfopen(u.path, L"ab");
				if(s_log->fp) { // don't really care if the log couldn't be made
					fputs("------ T-Clock: ", s_log->fp);
					DebugLog(0, "- logging started -");
				}
			}
		}
	}
	s_log->indent += indent;
	if(!s_log->fp)
		return;
	if(*format) {
		GetSystemTime(&now);
		GetModuleFileNameA(NULL, u.buf, _countof(u.buf));
		for(indent=s_log->indent-(indent>0 ? indent : 0); indent > 0; --indent)
			fputc('	', s_log->fp);
		fprintf(s_log->fp, "%04hu/%02hu/%02hu %02hu:%02hu:%02hu.%03hu UTC [%s", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, find_titleA(u.buf));
		if((HINSTANCE)&__ImageBase != GetModuleHandle(NULL)) {
			GetModuleFileNameA((HINSTANCE)&__ImageBase, u.buf, _countof(u.buf));
			fprintf(s_log->fp, ",%s", find_titleA(u.buf));
		}
		fputs("] ", s_log->fp);
		va_start(args, format);
		vfprintf(s_log->fp, format, args);
		va_end(args);
		fputc('\n', s_log->fp);
	}
	//fflush(s_log->fp);
}
void DebugLogFree() {
	if(s_log == kLogDisabled)
		s_log = NULL;
	if(s_log_share) {
		//if(s_log) {
		//	if(s_log->fp)
		//		fclose(s_log->fp), s_log->fp = NULL;
		//	UnmapViewOfFile(s_log), s_log = NULL;
		//}
		//CloseHandle(s_log_share), s_log_share = NULL;
	}
}
#endif // LOGGING
