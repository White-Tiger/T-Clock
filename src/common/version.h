#ifndef AUTOVERSION_H
#define AUTOVERSION_H
/* Note: to use integer defines as strings, use STR(), eg. STR(VER_REVISION) */
/**** Version ****/
#	define VER_MAJOR 2
#	define VER_MINOR 4
#	define VER_BUILD 4
	/** status values: 0=Alpha(α), 1=Beta(β), 2=RC(гc), 3=Release(г), 4=Maintenance(гm) */
#	define VER_STATUS 2
#	define VER_STATUS_FULL "RC"
#	define VER_STATUS_SHORT "rc"
#	define VER_STATUS_GREEK "\u0433c"
#	define VER_REVISION 531
#	define VER_FULL "2.4.4 RC"
#	define VER_SHORT "2.4rc4"
#	define VER_SHORT_DOTS "2.4.4"
#	define VER_SHORT_GREEK "2.4\u0433c4"
#	define VER_RC_REVISION 2, 4, 4, 531
#	define VER_RC_STATUS 2, 4, 4, 2
/**** Subversion Information ****/
#	define VER_REVISION_URL "https://github.com/drventure/T-Clock.git"
#	define VER_REVISION_DATE "2018-06-08 08:42:06 +0000 (Fri, Jun 08 2018)"
#	define VER_REVISION_HASH "2eb7e2a"
#	define VER_REVISION_TAG "v2.4.4#531-rc"
/**** Date/Time ****/
#	define VER_TIMESTAMP 1604167172
#	define VER_TIME_SEC 32
#	define VER_TIME_MIN 59
#	define VER_TIME_HOUR 17
#	define VER_TIME_DAY 31
#	define VER_TIME_MONTH 10
#	define VER_TIME_YEAR 2020
#	define VER_TIME_WDAY 6
#	define VER_TIME_YDAY 304
#	define VER_TIME_WDAY_SHORT "Sat"
#	define VER_TIME_WDAY_FULL "Saturday"
#	define VER_TIME_MONTH_SHORT "Oct"
#	define VER_TIME_MONTH_FULL "October"
#	define VER_TIME "17:59:32"
#	define VER_DATE "2020-10-31"
#	define VER_DATE_LONG "Sat, Oct 31, 2020 17:59:32 UTC"
#	define VER_DATE_SHORT "2020-10-31 17:59:32 UTC"
#	define VER_DATE_ISO "2020-10-31T17:59:32Z"
/**** Helper 'functions' ****/
#	define VER_IsReleaseOrHigher() ( VER_STATUS >= 3 )
#	define VER_IsAlpha() ( VER_STATUS == 0 )
#	define VER_IsBeta() ( VER_STATUS == 1 )
#	define VER_IsRC() ( VER_STATUS == 2 )
#	define VER_IsRelease() ( VER_STATUS == 3 )
#	define VER_IsMaintenance() ( VER_STATUS == 4 )
#ifndef STR
#	define STR_(x) #x
#	define STR(x) STR_(x)
#endif
#ifndef L
#	define L_(x) L##x
#	define L(x) L_(x)
#endif
#endif
