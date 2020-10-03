#include "../common/globals.h"
//#include "../common/newapi.h"
#include "../common/getopt_tools.h"
#include "../common/version.h"
#include <time.h>
#include <commctrl.h>
//#include <io.h> // _open_osfhandle
//#include <fcntl.h> // _O_TEXT

#include "update.h"

//other
HINSTANCE g_instance;
TClockAPI api;


int main(int argc, char* argv[]) {
	static const char* short_options = "hu::";
	static struct option long_options[] = {
		// basic
		{"help",       no_argument,       0, 'h'},
		{"version",    no_argument,       0, '1'},
		{"update",     optional_argument, 0, 'u'},
		{0}
	};
	const struct help help_info[] = {
		{0,DH_ARGV_SHORT,"[option] ..."},
		{'h',0,""},
		{'1',0,""},
		{'u',"check|notify|silent","check for updates\nreturns IDYES(6), IDNO(7) or IDCANCEL(2)"},
		{0}
	};
	INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_STANDARD_CLASSES|ICC_WIN95_CLASSES};
	
	OpportunisticConsole();
	// https://support.microsoft.com/en-us/kb/105305
	/* if it were a GUI app, we could do something like this:
	if(stdout->_file < 0) {
		HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if(hstdout && hstdout != INVALID_HANDLE_VALUE) {
			int hCrt = _open_osfhandle((intptr_t)hstdout, _O_TEXT);
			FILE* hf = _fdopen(hCrt, "w");
			setvbuf(hf, NULL, _IONBF, 0);
			*stdout = *hf;
		}
	}
	if(stderr->_file < 0) {
		HANDLE hstderr = GetStdHandle(STD_ERROR_HANDLE);
		if(hstderr && hstderr != INVALID_HANDLE_VALUE) {
			int hCrt = _open_osfhandle((intptr_t)hstderr, _O_TEXT);
			FILE* hf = _fdopen(hCrt, "w");
			setvbuf(hf, NULL, _IONBF, 0);
			*stderr = *hf;
		}
	}// */
	
	g_instance = GetModuleHandle(NULL);
	InitCommonControlsEx(&icex);
	if(LoadClockAPI(L"T-Clock" ARCH_SUFFIX, &api)) {
		puts("failed to load T-Clock.dll");
		return 2;
	}
	
	for(;;) {
		int opt;
		int option_index = 0;
		opt = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (opt == -1)
			break;
		switch(opt){
		case 0: case 1:
			break;
		case '?': /*case ':':*/
			break;
		case 'h':
			DisplayHelp(argv[0], short_options, long_options, help_info, 80);
			/* fall through */
		case '1':
			puts("T-Clock Options " VER_REVISION_TAG);
			return 1;
		case 'u':{
			int updates = IDCANCEL;
			if(!optarg || !strcasecmp(optarg,"check")) {
				updates = UpdateCheck(UPDATE_SHOW);
			} else if(!strcasecmp(optarg,"notify")) {
				updates = UpdateCheck(UPDATE_NOTIFY);
			} else if(!strcasecmp(optarg,"silent")) {
				updates = UpdateCheck(UPDATE_SILENT);
			}
			if(updates == IDYES)
				puts("updates found");
			else if(updates == IDNO)
				puts("no updates");
			else
				puts("update error");
			return updates;}
		default:
			;
		}
	}
	if(optind < argc || argc == 1) {
		DisplayHelp(argv[0], short_options, long_options, help_info, 80);
		return 1;
	}
	return 0;
}

//Base class 
class Parent 
{ 
	public: 
	int id_p; 
}; 

// Sub class inheriting from Base Class(Parent) 
class Child : public Parent 
{ 
	public: 
	int id_c; 
}; 

//main function 
int main() 
{ 
	
		Child obj1; 
		
		// An object of class child has all data members 
		// and member functions of class parent 
		obj1.id_c = 7; 
		obj1.id_p = 91; 
		cout << "Child id is " << obj1.id_c << endl; 
		cout << "Parent id is " << obj1.id_p << endl; 
		
		return 0; 
} 

