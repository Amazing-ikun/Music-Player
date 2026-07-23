#pragma once

// Control IDs
constexpr int IDC_PLAYLIST         = 1001;
constexpr int IDC_SEARCH_EDIT      = 1011;
constexpr int IDC_BTN_PREV         = 1002;
constexpr int IDC_BTN_PLAY         = 1003;
constexpr int IDC_BTN_NEXT         = 1004;
constexpr int IDC_BTN_MODE         = 1005;
constexpr int IDC_SLIDER_VOL       = 1006;
constexpr int IDC_TRACK_SEEK       = 1007;
constexpr int IDC_STAT_TIME        = 1008;
constexpr int IDC_STAT_SONG        = 1009;
constexpr int IDC_STAT_VOL         = 1010;
constexpr int IDC_CTRL_PANEL       = 1012;
constexpr int IDC_BTN_LOCATE       = 1013;

// Menu Command IDs
constexpr int ID_FILE_OPENFOLDER   = 2001;
constexpr int ID_FILE_EXIT         = 2002;
constexpr int ID_PLAY_SEQUENTIAL   = 2003;
constexpr int ID_PLAY_REPEATONE    = 2004;
constexpr int ID_PLAY_SHUFFLE      = 2005;
constexpr int ID_SETTINGS_AUTOPLAY = 2006;
constexpr int ID_SETTINGS_REMEMBER = 2007;
constexpr int ID_FILE_ADDFILES     = 2008;
constexpr int ID_SETTINGS_TRAY     = 2009;
constexpr int ID_TRAY_EXIT         = 2010;
constexpr int ID_TRAY_RESTORE      = 2011;
constexpr int ID_SETTINGS_HOTKEYS  = 2012;
constexpr int ID_SETTINGS_STATS    = 2013;
constexpr int ID_FILE_EXPORT_PLAYLIST = 2014;
constexpr int ID_TRAY_PLAYPAUSE    = 2015;
constexpr int ID_TRAY_PREV         = 2016;
constexpr int ID_TRAY_NEXT         = 2017;
constexpr int ID_TRAY_MINIMIZE     = 2018;
constexpr int ID_SPEED_025         = 2019;
constexpr int ID_SPEED_050         = 2020;
constexpr int ID_SPEED_075         = 2021;
constexpr int ID_SPEED_100         = 2022;
constexpr int ID_SPEED_125         = 2023;
constexpr int ID_SPEED_150         = 2024;
constexpr int ID_SPEED_200         = 2025;
constexpr int ID_SPEED_CUSTOM      = 2026;
constexpr int ID_STARTUP_NOTHING   = 2027;
constexpr int ID_STARTUP_AUTOPLAY  = 2028;
constexpr int ID_UNDO_REMOVE       = 2029;

// Custom Window Messages
constexpr UINT WM_USER_SONG_END    = WM_USER + 100;
constexpr UINT WM_APP_TRAY         = WM_USER + 101;
constexpr UINT WM_APP_FADE_DONE    = WM_USER + 102;
constexpr UINT WM_APP_BRING_TO_TOP = WM_USER + 103;

// Timer IDs
constexpr UINT_PTR TIMER_ID_SEEK   = 3001;

// Resource ID (must be #define — RC compiler doesn't understand constexpr)
#define IDI_APP_ICON 101

// Hotkey IDs
constexpr int HKID_PLAYPAUSE       = 4001;
constexpr int HKID_PREV            = 4002;
constexpr int HKID_NEXT            = 4003;
constexpr int HKID_VOLUP           = 4004;
constexpr int HKID_VOLDN           = 4005;
constexpr int HKID_RESTORE         = 4006;
constexpr int HKID_MINIMIZE        = 4007;
