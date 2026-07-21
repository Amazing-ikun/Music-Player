#pragma once

// Control IDs
#define IDC_PLAYLIST        1001
#define IDC_SEARCH_EDIT     1011
#define IDC_BTN_PREV        1002
#define IDC_BTN_PLAY        1003
#define IDC_BTN_NEXT        1004
#define IDC_BTN_MODE        1005
#define IDC_SLIDER_VOL      1006
#define IDC_TRACK_SEEK      1007
#define IDC_STAT_TIME       1008
#define IDC_STAT_SONG       1009
#define IDC_STAT_VOL        1010
#define IDC_CTRL_PANEL      1012

// Menu Command IDs
#define ID_FILE_OPENFOLDER  2001
#define ID_FILE_EXIT        2002
#define ID_PLAY_SEQUENTIAL  2003
#define ID_PLAY_REPEATONE   2004
#define ID_PLAY_SHUFFLE     2005
#define ID_SETTINGS_AUTOPLAY  2006
#define ID_SETTINGS_REMEMBER  2007
#define ID_FILE_ADDFILES      2008
#define ID_SETTINGS_TRAY      2009
#define ID_TRAY_EXIT          2010
#define ID_TRAY_RESTORE       2011
#define ID_SETTINGS_HOTKEYS   2012
#define ID_SETTINGS_STATS    2013
#define ID_FILE_EXPORT_PLAYLIST 2014

// Custom Window Messages
#define WM_USER_SONG_END    (WM_USER + 100)
#define WM_APP_TRAY         (WM_USER + 101)

// Timer IDs
#define TIMER_ID_SEEK       3001

// Resource IDs
#define IDI_APP_ICON        101

// Hotkey IDs
#define HKID_PLAYPAUSE  4001
#define HKID_PREV       4002
#define HKID_NEXT       4003
#define HKID_VOLUP      4004
#define HKID_VOLDN      4005
#define HKID_RESTORE    4006
#define HKID_MINIMIZE   4007
