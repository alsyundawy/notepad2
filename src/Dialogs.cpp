/******************************************************************************
*
*
* Notepad4
*
* Dialogs.cpp
*   Notepad4 dialog boxes implementation
*
* See Readme.txt for more information about this source code.
* Please send me your comments to this work.
*
* See License.txt for details about distribution and modification.
*
*                                              (c) Florian Balmer 1996-2011
*                                                  florian.balmer@gmail.com
*                                              https://www.flos-freeware.ch
*
*
******************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include "config.h"
#include "SciCall.h"
#include "Helpers.h"
#include "Notepad4.h"
#include "Edit.h"
#include "Styles.h"
#include "Dlapi.h"
#include "Dialogs.h"
#include "resource.h"
#include "Version.h"

extern HWND		hwndMain;
extern DWORD	dwLastIOError;
extern int		iCurrentEncoding;
extern bool		bSkipUnicodeDetection;
extern bool		bLoadANSIasUTF8;
extern bool		bLoadASCIIasUTF8;
extern bool		bLoadNFOasOEM;
extern bool		fNoFileVariables;
extern bool		bNoEncodingTags;
extern bool		bWarnLineEndings;
extern bool		bFixLineEndings;
extern bool		bAutoStripBlanks;
#if NP2_ENABLE_APP_LOCALIZATION_DLL
extern LANGID uiLanguage;
#endif
extern int iWrapColumn;
extern bool bUseXPFileDialog;

static inline HWND GetMsgBoxParent() noexcept {
	HWND hwnd = GetActiveWindow();
	return (hwnd == nullptr) ? hwndMain : hwnd;
}

//=============================================================================
//
// MsgBox()
//
int MsgBox(UINT uType, UINT uIdMsg, ...) noexcept {
	WCHAR szBuf[1024];
	WCHAR szText[1024];

	GetString(uIdMsg, szBuf, COUNTOF(szBuf));

	va_list va;
	va_start(va, uIdMsg);
	wvsprintf(szText, szBuf, va);
	va_end(va);

#if NP2_ENABLE_APP_LOCALIZATION_DLL
	const LANGID lang = uiLanguage;
#else
	constexpr LANGID lang = LANG_USER_DEFAULT;
#endif

	if (uType & MB_SERVICE_NOTIFICATION) {
		uType &= ~MB_SERVICE_NOTIFICATION;
		LPWSTR lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			dwLastIOError,
			lang,
			reinterpret_cast<LPWSTR>(&lpMsgBuf),
			0,
			nullptr);
		StrTrim(lpMsgBuf, L" \a\b\f\n\r\t\v");
		StrCatBuff(szText, L"\r\n", COUNTOF(szText));
		StrCatBuff(szText, lpMsgBuf, COUNTOF(szText));
		LocalFree(lpMsgBuf);
		const WCHAR wcht = szText[lstrlen(szText) - 1];
		if (IsCharAlphaNumeric(wcht) || wcht == L'"' || wcht == L'\'') {
			StrCatBuff(szText, L".", COUNTOF(szText));
		}
	}

	WCHAR szTitle[128];
	GetString(IDS_APPTITLE, szTitle, COUNTOF(szTitle));

	uType |= MB_SETFOREGROUND;
	if (bWindowLayoutRTL) {
		uType |= MB_RTLREADING;
	}

	HWND hwnd = GetMsgBoxParent();
	PostMessage(hwndMain, APPM_CENTER_MESSAGE_BOX, AsInteger<WPARAM>(hwnd), 0);
	return MessageBoxEx(hwnd, szText, szTitle, uType, lang);
}

//=============================================================================
//
// DisplayCmdLineHelp()
//
void DisplayCmdLineHelp(HWND hwnd) noexcept {
	WCHAR szTitle[32];
	WCHAR szText[2048];

	GetString(IDS_APPTITLE, szTitle, COUNTOF(szTitle));
	GetString(IDS_CMDLINEHELP, szText, COUNTOF(szText));

	MSGBOXPARAMS mbp;
	mbp.cbSize = sizeof(MSGBOXPARAMS);
	mbp.hwndOwner = hwnd;
	mbp.hInstance = g_hInstance;
	mbp.lpszText = szText;
	mbp.lpszCaption = szTitle;
	mbp.dwStyle = MB_OK | MB_USERICON | MB_SETFOREGROUND;
	mbp.lpszIcon = MAKEINTRESOURCE(IDR_MAINWND);
	mbp.dwContextHelpId = 0;
	mbp.lpfnMsgBoxCallback = nullptr;
#if NP2_ENABLE_APP_LOCALIZATION_DLL
	mbp.dwLanguageId = uiLanguage;
#else
	mbp.dwLanguageId = LANG_USER_DEFAULT;
#endif
	if (bWindowLayoutRTL) {
		mbp.dwStyle |= MB_RTLREADING;
	}

	if (hwnd != nullptr) {
		PostMessage(hwndMain, APPM_CENTER_MESSAGE_BOX, AsInteger<WPARAM>(hwnd), 0);
	}
	MessageBoxIndirect(&mbp);
}

void OpenHelpLink(HWND hwnd, int cmd) noexcept {
	LPCWSTR link = nullptr;
	switch (cmd) {
	case IDC_WEBPAGE_LINK:
		link = L"https://www.flos-freeware.ch";
		break;
	case IDC_EMAIL_LINK:
		link = L"mailto:florian.balmer@gmail.com";
		break;
	case IDC_MOD_PAGE_LINK:
		link = VERSION_MODPAGE_DISPLAY;
		break;
	case IDC_SCI_PAGE_LINK:
		link = VERSION_SCIPAGE_DISPLAY;
		break;
	case IDC_NEW_PAGE_LINK:
	case IDM_HELP_PROJECT_HOME:
		link = VERSION_NEWPAGE_DISPLAY;
		break;
	case IDM_HELP_LATEST_RELEASE:
		link = HELP_LINK_LATEST_RELEASE;
		break;
	case IDM_HELP_LATEST_BUILD:
		link = HELP_LINK_LATEST_BUILD;
		break;
	case IDM_HELP_REPORT_ISSUE:
		link = HELP_LINK_REPORT_ISSUE;
		break;
	case IDM_HELP_FEATURE_REQUEST:
		link = HELP_LINK_FEATURE_REQUEST;
		break;
	case IDM_HELP_ONLINE_WIKI:
		link = HELP_LINK_ONLINE_WIKI;
		break;
	}

	if (StrNotEmpty(link)) {
		ShellExecute(hwnd, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);
	}
}

static inline LPCWSTR GetProcessorArchitecture() noexcept {
	SYSTEM_INFO info;
	GetNativeSystemInfo(&info);
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64	12
#endif
	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		return L"x64";
	case PROCESSOR_ARCHITECTURE_ARM:
		return L"ARM";
	case PROCESSOR_ARCHITECTURE_ARM64:
		return L"ARM64";
	case PROCESSOR_ARCHITECTURE_IA64:
		return L"IA64";
	case PROCESSOR_ARCHITECTURE_INTEL:
		return L"x86";
	default:
		return L"Unknown";
	}
}

//=============================================================================
//
// BFFCallBack()
//
static int CALLBACK BFFCallBack(HWND hwnd, UINT umsg, LPARAM lParam, LPARAM lpData) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	if (umsg == BFFM_INITIALIZED) {
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}

//=============================================================================
//
// GetDirectory()
//
bool GetDirectory(HWND hwndParent, int iTitle, LPWSTR pszFolder, LPCWSTR pszBase) noexcept {
	WCHAR szTitle[256];
	StrCpyEx(szTitle, L"");
	GetString(iTitle, szTitle, COUNTOF(szTitle));

	WCHAR szBase[MAX_PATH];
	if (StrIsEmpty(pszBase)) {
		GetCurrentDirectory(MAX_PATH, szBase);
	} else {
		lstrcpy(szBase, pszBase);
	}

	BROWSEINFO bi;
	bi.hwndOwner = hwndParent;
	bi.pidlRoot = nullptr;
	bi.pszDisplayName = pszFolder;
	bi.lpszTitle = szTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = &BFFCallBack;
	bi.lParam = AsInteger<LPARAM>(szBase);
	bi.iImage = 0;

	PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
	if (pidl) {
		SHGetPathFromIDList(pidl, pszFolder);
		CoTaskMemFree(pidl);
		return true;
	}

	return false;
}

//=============================================================================
//
// AboutDlgProc()
//
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		WCHAR wch[128];
#if defined(VERSION_BUILD_TOOL_BUILD)
		wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
			VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH, VERSION_BUILD_TOOL_BUILD);
#else
		wsprintf(wch, VERSION_BUILD_INFO_FORMAT, VERSION_BUILD_TOOL_NAME,
			VERSION_BUILD_TOOL_MAJOR, VERSION_BUILD_TOOL_MINOR, VERSION_BUILD_TOOL_PATCH);
#endif

		SetDlgItemText(hwnd, IDC_VERSION, VERSION_FILEVERSION_LONG);
		SetDlgItemText(hwnd, IDC_BUILD_INFO, wch);

		HFONT hFontTitle = AsPointer<HFONT>(SendDlgItemMessage(hwnd, IDC_VERSION, WM_GETFONT, 0, 0));
		if (hFontTitle == nullptr) {
			hFontTitle = GetStockFont(DEFAULT_GUI_FONT);
		}

		LOGFONT lf;
		GetObject(hFontTitle, sizeof(LOGFONT), &lf);
		lf.lfWeight = FW_BOLD;
		hFontTitle = CreateFontIndirect(&lf);
		SendDlgItemMessage(hwnd, IDC_VERSION, WM_SETFONT, AsInteger<WPARAM>(hFontTitle), TRUE);
		SetWindowLongPtr(hwnd, DWLP_USER, AsInteger<LONG_PTR>(hFontTitle));

		if (GetDlgItem(hwnd, IDC_WEBPAGE_LINK) == nullptr) {
			SetDlgItemText(hwnd, IDC_WEBPAGE_TEXT, VERSION_WEBPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_WEBPAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_WEBPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_WEBPAGE_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_EMAIL_LINK) == nullptr) {
			SetDlgItemText(hwnd, IDC_EMAIL_TEXT, VERSION_EMAIL_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_EMAIL_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_EMAIL_DISPLAY);
			SetDlgItemText(hwnd, IDC_EMAIL_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_MOD_PAGE_LINK) == nullptr) {
			SetDlgItemText(hwnd, IDC_MOD_PAGE_LINK, VERSION_MODPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_MOD_PAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_MODPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_MOD_PAGE_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_NEW_PAGE_LINK) == nullptr) {
			SetDlgItemText(hwnd, IDC_NEW_PAGE_TEXT, VERSION_NEWPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_NEW_PAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_NEWPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_NEW_PAGE_LINK, wch);
		}

		if (GetDlgItem(hwnd, IDC_SCI_PAGE_LINK) == nullptr) {
			SetDlgItemText(hwnd, IDC_SCI_PAGE_TEXT, VERSION_SCIPAGE_DISPLAY);
			ShowWindow(GetDlgItem(hwnd, IDC_SCI_PAGE_TEXT), SW_SHOWNORMAL);
		} else {
			wsprintf(wch, L"<A>%s</A>", VERSION_SCIPAGE_DISPLAY);
			SetDlgItemText(hwnd, IDC_SCI_PAGE_LINK, wch);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmhdr = AsPointer<LPNMHDR>(lParam);
		switch (pnmhdr->code) {
		case NM_CLICK:
		case NM_RETURN:
			OpenHelpLink(hwnd, static_cast<int>(pnmhdr->idFrom));
			break;
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
		case IDC_COPY_BUILD_INFO:
			if (LOWORD(wParam) == IDC_COPY_BUILD_INFO) {
				OSVERSIONINFOW version;
				memset(&version, 0, sizeof(version));
				version.dwOSVersionInfoSize = sizeof(version);
				NP2_COMPILER_WARNING_PUSH
				NP2_IGNORE_WARNING_DEPRECATED_DECLARATIONS
				GetVersionEx(&version);
				NP2_COMPILER_WARNING_POP

				WCHAR wch[128];
				WCHAR tch[512];
				LPCWSTR arch = GetProcessorArchitecture();
				const int iEncoding = Encoding_GetIndex(mEncoding[CPI_DEFAULT].uCodePage);
				Encoding_GetLabel(iEncoding);
				GetDlgItemText(hwnd, IDC_BUILD_INFO, wch, COUNTOF(wch));
				wsprintf(tch, L"%s\n%s\nEncoding: %s, %s\nScheme: %s, %s\nSystem: %u.%u.%u %s %s\n",
					VERSION_FILEVERSION_LONG, wch,
					mEncoding[iCurrentEncoding].wchLabel, mEncoding[iEncoding].wchLabel,
					PathFindExtension(szCurFile), pLexCurrent->pszName,
					version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber,
					version.szCSDVersion, arch);
				SetClipData(hwnd, tch);
			}
			EndDialog(hwnd, IDOK);
			break;
		}
		return TRUE;

	case WM_DESTROY: {
		HFONT hFontTitle = AsPointer<HFONT>(GetWindowLongPtr(hwnd, DWLP_USER));
		DeleteObject(hFontTitle);
	}
	return FALSE;
	}
	return FALSE;
}

//=============================================================================
//
// RunDlgProc()
//
extern int cxRunDlg;
static INT_PTR CALLBACK RunDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		ResizeDlg_InitX(hwnd, cxRunDlg, IDC_RESIZEGRIP3);
		MakeBitmapButton(hwnd, IDC_SEARCHEXE, g_exeInstance, IDB_OPEN_FOLDER16);

		HWND hwndCtl = GetDlgItem(hwnd, IDC_COMMANDLINE);
		Edit_LimitText(hwndCtl, MAX_PATH - 1);
		Edit_SetText(hwndCtl, AsPointer<LPCWSTR>(lParam));
		SHAutoComplete(hwndCtl, SHACF_FILESYSTEM);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxRunDlg, nullptr);
		DeleteBitmapButton(hwnd, IDC_SEARCHEXE);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, nullptr);
		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RUNDESC, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_SEARCHEXE, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_COMMANDLINE, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
		InvalidateRect(GetDlgItem(hwnd, IDC_RUNDESC), nullptr, TRUE);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SEARCHEXE: {
			WCHAR szArgs[MAX_PATH];
			WCHAR szArg2[MAX_PATH];
			WCHAR szFile[MAX_PATH * 2];

			GetDlgItemText(hwnd, IDC_COMMANDLINE, szArgs, COUNTOF(szArgs));
			ExtractFirstArgument(szArgs, szFile, szArg2);
			ExpandEnvironmentStringsEx(szFile, COUNTOF(szFile));
			ExpandEnvironmentStringsEx(szArg2, COUNTOF(szArg2));

			WCHAR szFilter[256];
			GetString(IDS_FILTER_EXE, szFilter, COUNTOF(szFilter));
			PrepareFilterStr(szFilter);

			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = szFilter;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = COUNTOF(szFile);
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
						| OFN_PATHMUSTEXIST | OFN_SHAREAWARE | OFN_NODEREFERENCELINKS;
			if (bUseXPFileDialog) {
				ofn.Flags |= OFN_EXPLORER | OFN_ENABLESIZING | OFN_ENABLEHOOK;
				ofn.lpfnHook = OpenSaveFileDlgHookProc;
			}

			if (GetOpenFileName(&ofn)) {
				PathQuoteSpaces(szFile);
				if (StrNotEmpty(szArg2)) {
					lstrcat(szFile, L" ");
					lstrcat(szFile, szArg2);
				}
				SetDlgItemText(hwnd, IDC_COMMANDLINE, szFile);
			}

			PostMessage(hwnd, WM_NEXTDLGCTL, TRUE, FALSE);
		}
		break;

		case IDC_COMMANDLINE: {
			bool bEnableOK = false;
			WCHAR args[MAX_PATH];

			if (GetDlgItemText(hwnd, IDC_COMMANDLINE, args, MAX_PATH)) {
				if (ExtractFirstArgument(args, args, nullptr)) {
					if (StrNotEmpty(args)) {
						bEnableOK = true;
					}
				}
			}

			EnableWindow(GetDlgItem(hwnd, IDOK), bEnableOK);
		}
		break;

		case IDOK: {
			WCHAR arg1[MAX_PATH];
			if (GetDlgItemText(hwnd, IDC_COMMANDLINE, arg1, MAX_PATH)) {
				bool bQuickExit = false;
				WCHAR arg2[MAX_PATH];

				ExtractFirstArgument(arg1, arg1, arg2);
				ExpandEnvironmentStringsEx(arg2, COUNTOF(arg2));

				if (StrCaseEqual(arg1, L"Notepad4") || StrCaseEqual(arg1, L"Notepad4.exe")) {
					GetModuleFileName(nullptr, arg1, COUNTOF(arg1));
					bQuickExit = true;
				}

				WCHAR wchDirectory[MAX_PATH] = L"";
				if (StrNotEmpty(szCurFile)) {
					lstrcpy(wchDirectory, szCurFile);
					PathRemoveFileSpec(wchDirectory);
				}

				SHELLEXECUTEINFO sei;
				memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
				sei.cbSize = sizeof(SHELLEXECUTEINFO);
				sei.fMask = SEE_MASK_DOENVSUBST;
				sei.hwnd = hwnd;
				sei.lpVerb = nullptr;
				sei.lpFile = arg1;
				sei.lpParameters = arg2;
				sei.lpDirectory = wchDirectory;
				sei.nShow = SW_SHOWNORMAL;

				if (bQuickExit) {
					sei.fMask |= SEE_MASK_NOZONECHECKS;
					EndDialog(hwnd, IDOK);
					ShellExecuteEx(&sei);
				} else {
					if (ShellExecuteEx(&sei)) {
						EndDialog(hwnd, IDOK);
					} else {
						PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, IDC_COMMANDLINE)), TRUE);
					}
				}
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// RunDlg()
//
void RunDlg(HWND hwnd, LPCWSTR lpstrDefault) noexcept {
	ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_RUN), hwnd, RunDlgProc, AsInteger<LPARAM>(lpstrDefault));
}

//=============================================================================
//
// OpenWithDlgProc()
//
extern WCHAR tchOpenWithDir[MAX_PATH];
extern bool flagNoFadeHidden;

extern int cxOpenWithDlg;
extern int cyOpenWithDlg;

static INT_PTR CALLBACK OpenWithDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_Init(hwnd, cxOpenWithDlg, cyOpenWithDlg, IDC_RESIZEGRIP3);

		HWND hwndLV = GetDlgItem(hwnd, IDC_OPENWITHDIR);
		InitWindowCommon(hwndLV);
		//SetExplorerTheme(hwndLV);
		ListView_SetExtendedListViewStyle(hwndLV, /*LVS_EX_FULLROWSELECT|*/LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

		const LVCOLUMN lvc = { LVCF_FMT | LVCF_TEXT, LVCFMT_LEFT, 0, nullptr, -1, 0, 0, 0
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
			, 0, 0, 0
#endif
		};
		ListView_InsertColumn(hwndLV, 0, &lvc);
		DirList_Init(hwndLV);
		DirList_Fill(hwndLV, tchOpenWithDir, DL_ALLOBJECTS, nullptr, false, flagNoFadeHidden, DS_NAME, false);
		DirList_StartIconThread(hwndLV);
		ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);

		MakeBitmapButton(hwnd, IDC_GETOPENWITHDIR, g_exeInstance, IDB_OPEN_FOLDER16);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		DirList_Destroy(GetDlgItem(hwnd, IDC_OPENWITHDIR));
		DeleteBitmapButton(hwnd, IDC_GETOPENWITHDIR);
		ResizeDlg_Destroy(hwnd, &cxOpenWithDlg, &cyOpenWithDlg);
		return FALSE;

	case WM_SIZE: {
		int dx;
		int dy;

		ResizeDlg_Size(hwnd, lParam, &dx, &dy);

		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_OPENWITHDIR, dx, dy, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_GETOPENWITHDIR, 0, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_OPENWITHDESCR, 0, dy, SWP_NOSIZE);
		EndDeferWindowPos(hdwp);

		ResizeDlgCtl(hwnd, IDC_OPENWITHDESCR, dx, 0);
		ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_OPENWITHDIR), 0, LVSCW_AUTOSIZE_USEHEADER);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmh = AsPointer<LPNMHDR>(lParam);

		if (pnmh->idFrom == IDC_OPENWITHDIR) {
			switch (pnmh->code) {
			case LVN_GETDISPINFO:
				DirList_GetDispInfo(GetDlgItem(hwnd, IDC_OPENWITHDIR), lParam);
				break;

			case LVN_DELETEITEM:
				DirList_DeleteItem(GetDlgItem(hwnd, IDC_OPENWITHDIR), lParam);
				break;

			case LVN_ITEMCHANGED: {
				const NM_LISTVIEW *pnmlv = AsPointer<NM_LISTVIEW *>(lParam);
				EnableWindow(GetDlgItem(hwnd, IDOK), (pnmlv->uNewState & LVIS_SELECTED));
			}
			break;

			case NM_DBLCLK:
				if (ListView_GetSelectedCount(GetDlgItem(hwnd, IDC_OPENWITHDIR))) {
					SendWMCommand(hwnd, IDOK);
				}
				break;
			}
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_GETOPENWITHDIR: {
			HWND hwndLV = GetDlgItem(hwnd, IDC_OPENWITHDIR);
			if (GetDirectory(hwnd, IDS_OPENWITH, tchOpenWithDir, tchOpenWithDir)) {
				DirList_Fill(hwndLV, tchOpenWithDir, DL_ALLOBJECTS, nullptr, false, flagNoFadeHidden, DS_NAME, false);
				DirList_StartIconThread(hwndLV);
				ListView_EnsureVisible(hwndLV, 0, FALSE);
				ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);
			}
			PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(hwndLV), TRUE);
		}
		break;

		case IDOK: {
			DirListItem *lpdli = AsPointer<DirListItem *>(GetWindowLongPtr(hwnd, DWLP_USER));
			lpdli->mask = DLI_FILENAME | DLI_TYPE;
			lpdli->ntype = DLE_NONE;
			DirList_GetItem(GetDlgItem(hwnd, IDC_OPENWITHDIR), (-1), lpdli);

			if (lpdli->ntype != DLE_NONE) {
				EndDialog(hwnd, IDOK);
			} else {
				MessageBeep(MB_OK);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// OpenWithDlg()
//
bool OpenWithDlg(HWND hwnd, LPCWSTR lpstrFile) {
	DirListItem dliOpenWith;
	dliOpenWith.mask = DLI_FILENAME;

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_OPENWITH), hwnd, OpenWithDlgProc, AsInteger<LPARAM>(&dliOpenWith))) {
		WCHAR szParam[MAX_PATH];
		WCHAR wchDirectory[MAX_PATH] = L"";

		if (StrNotEmpty(szCurFile)) {
			lstrcpy(wchDirectory, szCurFile);
			PathRemoveFileSpec(wchDirectory);
		}

		SHELLEXECUTEINFO sei;
		memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.fMask = 0;
		sei.hwnd = hwnd;
		sei.lpVerb = nullptr;
		sei.lpFile = dliOpenWith.szFileName;
		sei.lpParameters = szParam;
		sei.lpDirectory = wchDirectory;
		sei.nShow = SW_SHOWNORMAL;

		// resolve links and get short path name
		if (!PathGetLnkPath(lpstrFile, szParam)) {
			lstrcpy(szParam, lpstrFile);
		}
		PathQuoteSpaces(szParam);

		ShellExecuteEx(&sei);

		return true;
	}
	return false;
}

//=============================================================================
//
// FavoritesDlgProc()
//
extern WCHAR tchFavoritesDir[MAX_PATH];

extern int cxFavoritesDlg;
extern int cyFavoritesDlg;
extern int cxAddFavoritesDlg;

static INT_PTR CALLBACK FavoritesDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_Init(hwnd, cxFavoritesDlg, cyFavoritesDlg, IDC_RESIZEGRIP3);

		HWND hwndLV = GetDlgItem(hwnd, IDC_FAVORITESDIR);
		InitWindowCommon(hwndLV);
		//SetExplorerTheme(hwndLV);
		ListView_SetExtendedListViewStyle(hwndLV, /*LVS_EX_FULLROWSELECT|*/LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
		const LVCOLUMN lvc = { LVCF_FMT | LVCF_TEXT, LVCFMT_LEFT, 0, nullptr, -1, 0, 0, 0
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
			, 0, 0, 0
#endif
		};
		ListView_InsertColumn(hwndLV, 0, &lvc);
		DirList_Init(hwndLV);
		DirList_Fill(hwndLV, tchFavoritesDir, DL_ALLOBJECTS, nullptr, false, flagNoFadeHidden, DS_NAME, false);
		DirList_StartIconThread(hwndLV);
		ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);

		MakeBitmapButton(hwnd, IDC_GETFAVORITESDIR, g_exeInstance, IDB_OPEN_FOLDER16);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		DirList_Destroy(GetDlgItem(hwnd, IDC_FAVORITESDIR));
		DeleteBitmapButton(hwnd, IDC_GETFAVORITESDIR);
		ResizeDlg_Destroy(hwnd, &cxFavoritesDlg, &cyFavoritesDlg);
		return FALSE;

	case WM_SIZE: {
		int dx;
		int dy;

		ResizeDlg_Size(hwnd, lParam, &dx, &dy);

		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP3, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FAVORITESDIR, dx, dy, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_GETFAVORITESDIR, 0, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FAVORITESDESCR, 0, dy, SWP_NOSIZE);
		EndDeferWindowPos(hdwp);

		ResizeDlgCtl(hwnd, IDC_FAVORITESDESCR, dx, 0);
		ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_FAVORITESDIR), 0, LVSCW_AUTOSIZE_USEHEADER);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmh = AsPointer<LPNMHDR>(lParam);

		if (pnmh->idFrom == IDC_FAVORITESDIR) {
			switch (pnmh->code) {
			case LVN_GETDISPINFO:
				DirList_GetDispInfo(GetDlgItem(hwnd, IDC_OPENWITHDIR), lParam);
				break;

			case LVN_DELETEITEM:
				DirList_DeleteItem(GetDlgItem(hwnd, IDC_FAVORITESDIR), lParam);
				break;

			case LVN_ITEMCHANGED: {
				const NM_LISTVIEW *pnmlv = AsPointer<NM_LISTVIEW *>(lParam);
				EnableWindow(GetDlgItem(hwnd, IDOK), (pnmlv->uNewState & LVIS_SELECTED));
			}
			break;

			case NM_DBLCLK:
				if (ListView_GetSelectedCount(GetDlgItem(hwnd, IDC_FAVORITESDIR))) {
					SendWMCommand(hwnd, IDOK);
				}
				break;
			}
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_GETFAVORITESDIR: {
			HWND hwndLV = GetDlgItem(hwnd, IDC_FAVORITESDIR);
			if (GetDirectory(hwnd, IDS_FAVORITES, tchFavoritesDir, tchFavoritesDir)) {
				DirList_Fill(hwndLV, tchFavoritesDir, DL_ALLOBJECTS, nullptr, false, flagNoFadeHidden, DS_NAME, false);
				DirList_StartIconThread(hwndLV);
				ListView_EnsureVisible(hwndLV, 0, FALSE);
				ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);
			}
			PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(hwndLV), TRUE);
		}
		break;

		case IDOK: {
			DirListItem *lpdli = AsPointer<DirListItem *>(GetWindowLongPtr(hwnd, DWLP_USER));
			lpdli->mask = DLI_FILENAME | DLI_TYPE;
			lpdli->ntype = DLE_NONE;
			DirList_GetItem(GetDlgItem(hwnd, IDC_FAVORITESDIR), (-1), lpdli);

			if (lpdli->ntype != DLE_NONE) {
				EndDialog(hwnd, IDOK);
			} else {
				MessageBeep(MB_OK);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// FavoritesDlg()
//
bool FavoritesDlg(HWND hwnd, LPWSTR lpstrFile) noexcept {
	DirListItem dliFavorite;
	dliFavorite.mask = DLI_FILENAME;

	if (IDOK == ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_FAVORITES), hwnd, FavoritesDlgProc, AsInteger<LPARAM>(&dliFavorite))) {
		lstrcpyn(lpstrFile, dliFavorite.szFileName, MAX_PATH);
		return true;
	}

	return false;
}

//=============================================================================
//
// AddToFavDlgProc()
//
//
static INT_PTR CALLBACK AddToFavDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		ResizeDlg_InitX(hwnd, cxAddFavoritesDlg, IDC_RESIZEGRIP);

		HWND hwndCtl = GetDlgItem(hwnd, IDC_FAVORITESFILE);
		Edit_LimitText(hwndCtl, MAX_PATH - 1);
		Edit_SetText(hwndCtl, AsPointer<LPCWSTR>(lParam));

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY:
		ResizeDlg_Destroy(hwnd, &cxAddFavoritesDlg, nullptr);
		return FALSE;

	case WM_SIZE: {
		int dx;

		ResizeDlg_Size(hwnd, lParam, &dx, nullptr);
		HDWP hdwp = BeginDeferWindowPos(5);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, 0, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FAVORITESDESCR, dx, 0, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FAVORITESFILE, dx, 0, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
		InvalidateRect(GetDlgItem(hwnd, IDC_FAVORITESDESCR), nullptr, TRUE);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_FAVORITESFILE:
			EnableWindow(GetDlgItem(hwnd, IDOK), GetWindowTextLength(GetDlgItem(hwnd, IDC_FAVORITESFILE)));
			break;

		case IDOK: {
			LPWSTR pszName = AsPointer<LPWSTR>(GetWindowLongPtr(hwnd, DWLP_USER));
			GetDlgItemText(hwnd, IDC_FAVORITESFILE, pszName, MAX_PATH - 1);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// AddToFavDlg()
//
bool AddToFavDlg(HWND hwnd, LPCWSTR lpszName, LPCWSTR lpszTarget) {
	WCHAR pszName[MAX_PATH];
	lstrcpy(pszName, lpszName);

	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_ADDTOFAV), hwnd, AddToFavDlgProc, AsInteger<LPARAM>(pszName));

	if (iResult == IDOK) {
		if (PathCreateFavLnk(pszName, lpszTarget, tchFavoritesDir)) {
			MsgBoxInfo(MB_OK, IDS_FAV_SUCCESS);
			return true;
		}
		MsgBoxWarn(MB_OK, IDS_FAV_FAILURE);
	}

	return false;
}

//=============================================================================
//
// FileMRUDlgProc()
//
//
extern MRUList mruFile;
extern bool bSaveRecentFiles;
extern int iMaxRecentFiles;
extern int cxFileMRUDlg;
extern int cyFileMRUDlg;

static DWORD WINAPI FileMRUIconThread(LPVOID lpParam) noexcept {
	const BackgroundWorker * const worker = static_cast<const BackgroundWorker *>(lpParam);

	WCHAR tch[MAX_PATH] = L"";
	DWORD dwFlags = SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_ATTRIBUTES | SHGFI_ATTR_SPECIFIED;

	HWND hwnd = worker->hwnd;
	const int iMaxItem = ListView_GetItemCount(hwnd);
	int iItem = 0;

	LV_ITEM lvi;
	memset(&lvi, 0, sizeof(LV_ITEM));

	while (iItem < iMaxItem && worker->Continue()) {
		lvi.mask = LVIF_TEXT;
		lvi.pszText = tch;
		lvi.cchTextMax = COUNTOF(tch);
		lvi.iItem = iItem;
		if (ListView_GetItem(hwnd, &lvi)) {
			SHFILEINFO shfi;
			DWORD dwAttr = 0;
			if (PathIsUNC(tch) || !PathIsFile(tch)) {
				dwFlags |= SHGFI_USEFILEATTRIBUTES;
				dwAttr = FILE_ATTRIBUTE_NORMAL;
				shfi.dwAttributes = 0;
				SHGetFileInfo(PathFindFileName(tch), dwAttr, &shfi, sizeof(SHFILEINFO), dwFlags);
			} else {
				shfi.dwAttributes = SFGAO_LINK | SFGAO_SHARE;
				SHGetFileInfo(tch, dwAttr, &shfi, sizeof(SHFILEINFO), dwFlags);
			}

			lvi.mask = LVIF_IMAGE;
			lvi.iImage = shfi.iIcon;
			lvi.stateMask = 0;
			lvi.state = 0;

			if (shfi.dwAttributes & SFGAO_LINK) {
				lvi.mask |= LVIF_STATE;
				lvi.stateMask |= LVIS_OVERLAYMASK;
				lvi.state |= INDEXTOOVERLAYMASK(2);
			}

			if (shfi.dwAttributes & SFGAO_SHARE) {
				lvi.mask |= LVIF_STATE;
				lvi.stateMask |= LVIS_OVERLAYMASK;
				lvi.state |= INDEXTOOVERLAYMASK(1);
			}

			if (PathIsUNC(tch)) {
				dwAttr = FILE_ATTRIBUTE_NORMAL;
			} else {
				dwAttr = GetFileAttributes(tch);
			}

			if (!flagNoFadeHidden &&
					dwAttr != INVALID_FILE_ATTRIBUTES &&
					dwAttr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
				lvi.mask |= LVIF_STATE;
				lvi.stateMask |= LVIS_CUT;
				lvi.state |= LVIS_CUT;
			}

			lvi.iSubItem = 0;
			ListView_SetItem(hwnd, &lvi);
		}
		iItem++;
	}

	return 0;
}

static INT_PTR CALLBACK FileMRUDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);

		HWND hwndLV = GetDlgItem(hwnd, IDC_FILEMRU);
		InitWindowCommon(hwndLV);

		BackgroundWorker *worker = static_cast<BackgroundWorker *>(GlobalAlloc(GPTR, sizeof(BackgroundWorker)));
		SetProp(hwnd, L"it", worker);
		worker->Init(hwndLV);

		ResizeDlg_Init(hwnd, cxFileMRUDlg, cyFileMRUDlg, IDC_RESIZEGRIP);

		SHFILEINFO shfi;
		ListView_SetImageList(hwndLV,
							  AsPointer<HIMAGELIST>(SHGetFileInfo(L"C:\\", 0, &shfi, sizeof(SHFILEINFO), SHGFI_SMALLICON | SHGFI_SYSICONINDEX)),
							  LVSIL_SMALL);

		ListView_SetImageList(hwndLV,
							  AsPointer<HIMAGELIST>(SHGetFileInfo(L"C:\\", 0, &shfi, sizeof(SHFILEINFO), SHGFI_LARGEICON | SHGFI_SYSICONINDEX)),
							  LVSIL_NORMAL);

		//SetExplorerTheme(hwndLV);
		ListView_SetExtendedListViewStyle(hwndLV, /*LVS_EX_FULLROWSELECT|*/LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
		const LVCOLUMN lvc = { LVCF_FMT | LVCF_TEXT, LVCFMT_LEFT, 0, nullptr, -1, 0, 0, 0
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
			, 0, 0, 0
#endif
		};
		ListView_InsertColumn(hwndLV, 0, &lvc);

		// Update view
		SendWMCommand(hwnd, IDC_FILEMRU_UPDATE_VIEW);
		SetDlgItemInt(hwnd, IDC_MRU_COUNT_VALUE, iMaxRecentFiles, FALSE);

		if (bSaveRecentFiles) {
			CheckDlgButton(hwnd, IDC_SAVEMRU, BST_CHECKED);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY: {
		BackgroundWorker *worker = static_cast<BackgroundWorker *>(GetProp(hwnd, L"it"));
		worker->Destroy();
		RemoveProp(hwnd, L"it");
		GlobalFree(worker);

		bSaveRecentFiles = IsButtonChecked(hwnd, IDC_SAVEMRU);
		iMaxRecentFiles = GetDlgItemInt(hwnd, IDC_MRU_COUNT_VALUE, nullptr, FALSE);
		iMaxRecentFiles = max(iMaxRecentFiles, MRU_MAXITEMS);

		ResizeDlg_Destroy(hwnd, &cxFileMRUDlg, &cyFileMRUDlg);
	}
	return FALSE;

	case WM_SIZE: {
		int dx;
		int dy;

		ResizeDlg_Size(hwnd, lParam, &dx, &dy);

		HDWP hdwp = BeginDeferWindowPos(6);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_FILEMRU, dx, dy, SWP_NOMOVE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_EMPTY_MRU, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_SAVEMRU, 0, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_MRU_COUNT_LABEL, 0, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_MRU_COUNT_VALUE, 0, dy, SWP_NOSIZE);
		EndDeferWindowPos(hdwp);
		ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_FILEMRU), 0, LVSCW_AUTOSIZE_USEHEADER);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmhdr = AsPointer<LPNMHDR>(lParam);
		if (pnmhdr->idFrom == IDC_FILEMRU) {
			switch (pnmhdr->code) {
			case NM_DBLCLK:
				SendWMCommand(hwnd, IDOK);
				break;

			case LVN_GETDISPINFO: {
				/*
				LV_DISPINFO *lpdi = AsPointer<LV_DISPINFO *>(lParam);

				if (lpdi->item.mask & LVIF_IMAGE) {
					WCHAR tch[MAX_PATH];

					LV_ITEM lvi;
					memset(&lvi, 0, sizeof(LV_ITEM));
					lvi.mask = LVIF_TEXT;
					lvi.pszText = tch;
					lvi.cchTextMax = COUNTOF(tch);
					lvi.iItem = lpdi->item.iItem;

					ListView_GetItem(GetDlgItem(hwnd, IDC_FILEMRU), &lvi);

					DWORD dwFlags = SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_ATTRIBUTES | SHGFI_ATTR_SPECIFIED;
					DWORD dwAttr = 0;
					SHFILEINFO shfi;
					if (!PathIsFile(tch)) {
						dwFlags |= SHGFI_USEFILEATTRIBUTES;
						dwAttr = FILE_ATTRIBUTE_NORMAL;
						shfi.dwAttributes = 0;
						SHGetFileInfo(PathFindFileName(tch), dwAttr, &shfi, sizeof(SHFILEINFO), dwFlags);
					} else {
						shfi.dwAttributes = SFGAO_LINK | SFGAO_SHARE;
						SHGetFileInfo(tch, dwAttr, &shfi, sizeof(SHFILEINFO), dwFlags);
					}

					lpdi->item.iImage = shfi.iIcon;
					lpdi->item.mask |= LVIF_DI_SETITEM;
					lpdi->item.stateMask = 0;
					lpdi->item.state = 0;

					if (shfi.dwAttributes & SFGAO_LINK) {
						lpdi->item.mask |= LVIF_STATE;
						lpdi->item.stateMask |= LVIS_OVERLAYMASK;
						lpdi->item.state |= INDEXTOOVERLAYMASK(2);
					}

					if (shfi.dwAttributes & SFGAO_SHARE) {
						lpdi->item.mask |= LVIF_STATE;
						lpdi->item.stateMask |= LVIS_OVERLAYMASK;
						lpdi->item.state |= INDEXTOOVERLAYMASK(1);
					}

					dwAttr = GetFileAttributes(tch);

					if (!flagNoFadeHidden &&
							dwAttr != INVALID_FILE_ATTRIBUTES &&
							dwAttr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
						lpdi->item.mask |= LVIF_STATE;
						lpdi->item.stateMask |= LVIS_CUT;
						lpdi->item.state |= LVIS_CUT;
					}
				}
				*/
			}
			break;

			case LVN_ITEMCHANGED:
			case LVN_DELETEITEM:
				EnableWindow(GetDlgItem(hwnd, IDOK), ListView_GetSelectedCount(GetDlgItem(hwnd, IDC_FILEMRU)));
				break;
			}
		} else if (pnmhdr->idFrom == IDC_EMPTY_MRU) {
			if ((pnmhdr->code == NM_CLICK || pnmhdr->code == NM_RETURN)) {
				mruFile.Empty(false);
				if (StrNotEmpty(szCurFile)) {
					mruFile.Add(szCurFile);
				}
				mruFile.Save();
				SendWMCommand(hwnd, IDC_FILEMRU_UPDATE_VIEW);
			}
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_FILEMRU_UPDATE_VIEW: {
			BackgroundWorker *worker = static_cast<BackgroundWorker *>(GetProp(hwnd, L"it"));
			worker->Cancel();

			HWND hwndLV = GetDlgItem(hwnd, IDC_FILEMRU);
			ListView_DeleteAllItems(hwndLV);

			LV_ITEM lvi;
			memset(&lvi, 0, sizeof(LV_ITEM));
			lvi.mask = LVIF_TEXT | LVIF_IMAGE;

			SHFILEINFO shfi;
			SHGetFileInfo(L"Icon", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(SHFILEINFO),
						  SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_SYSICONINDEX);
			lvi.iImage = shfi.iIcon;

			for (int i = 0; i < mruFile.iSize; i++) {
				LPWSTR path = mruFile.pszItems[i];
				lvi.iItem = i;
				lvi.pszText = path;
				ListView_InsertItem(hwndLV, &lvi);
			}

			ListView_SetItemState(hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);
			ListView_SetColumnWidth(hwndLV, 0, LVSCW_AUTOSIZE_USEHEADER);

			worker->workerThread = CreateThread(nullptr, 0, FileMRUIconThread, worker, 0, nullptr);
		}
		break;

		case IDC_FILEMRU:
			break;

		case IDOK: {
			WCHAR tch[MAX_PATH];
			HWND hwndLV = GetDlgItem(hwnd, IDC_FILEMRU);

			if (ListView_GetSelectedCount(hwndLV)) {
				LV_ITEM lvi;
				memset(&lvi, 0, sizeof(LV_ITEM));

				lvi.mask = LVIF_TEXT;
				lvi.pszText = tch;
				lvi.cchTextMax = COUNTOF(tch);
				lvi.iItem = ListView_GetNextItem(hwndLV, -1, LVNI_ALL | LVNI_SELECTED);

				ListView_GetItem(hwndLV, &lvi);

				PathUnquoteSpaces(tch);

				if (!PathIsFile(tch)) {
					// Ask...
					if (IDYES == MsgBoxWarn(MB_YESNO, IDS_ERR_MRUDLG)) {
						mruFile.DeleteFileFromStore(tch);
						mruFile.Delete(lvi.iItem);

						// must use recreate the list, index might change...
						//ListView_DeleteItem(hwndLV, lvi.iItem);
						SendWMCommand(hwnd, IDC_FILEMRU_UPDATE_VIEW);

						EnableWindow(GetDlgItem(hwnd, IDOK), ListView_GetSelectedCount(hwndLV));
					}
				} else {
					lstrcpy(AsPointer<LPWSTR>(GetWindowLongPtr(hwnd, DWLP_USER)), tch);
					EndDialog(hwnd, IDOK);
				}
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;

		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// FileMRUDlg()
//
//
bool FileMRUDlg(HWND hwnd, LPWSTR lpstrFile) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_FILEMRU), hwnd, FileMRUDlgProc, AsInteger<LPARAM>(lpstrFile));
	return iResult == IDOK;
}

//=============================================================================
//
// ChangeNotifyDlgProc()
//
//
extern FileWatchingMode iFileWatchingMode;
extern int iFileWatchingOption;
extern bool bResetFileWatching;

static INT_PTR CALLBACK ChangeNotifyDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG:
		CheckRadioButton(hwnd, IDC_CHANGENOTIFY_NONE, IDC_CHANGENOTIFY_AUTO_RELOAD, IDC_CHANGENOTIFY_NONE + static_cast<int>(iFileWatchingMode));
		if (iFileWatchingOption & FileWatchingOption_LogFile) {
			CheckDlgButton(hwnd, IDC_CHANGENOTIFY_LOG_FILE, BST_CHECKED);
		}
		if (iFileWatchingOption & FileWatchingOption_KeepAtEnd) {
			CheckDlgButton(hwnd, IDC_CHANGENOTIFY_KEEP_AT_END, BST_CHECKED);
		}
		if (bResetFileWatching) {
			CheckDlgButton(hwnd, IDC_CHANGENOTIFY_RESET_WATCH, BST_CHECKED);
		}
		CenterDlgInParent(hwnd);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			int value = GetCheckedRadioButton(hwnd, IDC_CHANGENOTIFY_NONE, IDC_CHANGENOTIFY_AUTO_RELOAD);
			iFileWatchingMode = static_cast<FileWatchingMode>(value - IDC_CHANGENOTIFY_NONE);
			value = FileWatchingOption_None;
			if (IsButtonChecked(hwnd, IDC_CHANGENOTIFY_LOG_FILE)) {
				value |= FileWatchingOption_LogFile;
			}
			if (IsButtonChecked(hwnd, IDC_CHANGENOTIFY_KEEP_AT_END)) {
				value |= FileWatchingOption_KeepAtEnd;
			}
			iFileWatchingOption = value;
			bResetFileWatching = IsButtonChecked(hwnd, IDC_CHANGENOTIFY_RESET_WATCH);
			EndDialog(hwnd, IDOK);
		} break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//=============================================================================
//
// ChangeNotifyDlg()
//
bool ChangeNotifyDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CHANGENOTIFY), hwnd, ChangeNotifyDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// ColumnWrapDlgProc()
//
//
static INT_PTR CALLBACK ColumnWrapDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		const int column = iWrapColumn ? iWrapColumn : fvCurFile.iLongLinesLimit;

		SetDlgItemInt(hwnd, IDC_COLUMNWRAP, column, FALSE);
		SendDlgItemMessage(hwnd, IDC_COLUMNWRAP, EM_LIMITTEXT, 15, 0);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			BOOL fTranslated;
			const int column = GetDlgItemInt(hwnd, IDC_COLUMNWRAP, &fTranslated, FALSE);

			if (fTranslated) {
				iWrapColumn = clamp(column, 1, 512);

				EndDialog(hwnd, IDOK);
			} else {
				PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, IDC_COLUMNWRAP)), 1);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// ColumnWrapDlg()
//
bool ColumnWrapDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_COLUMNWRAP), hwnd, ColumnWrapDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// WordWrapSettingsDlgProc()
//
//
extern bool fWordWrapG;
extern int iWordWrapMode;
extern int iWordWrapIndent;
extern int iWordWrapSymbols;
extern bool bShowWordWrapSymbols;
extern bool bWordWrapSelectSubLine;
extern bool bHighlightCurrentSubLine;

static INT_PTR CALLBACK WordWrapSettingsDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		WCHAR tch[512];
		for (int i = 0; i < 4; i++) {
			HWND hwndCtl = GetDlgItem(hwnd, IDC_WRAP_INDENT + i);
			GetString(IDS_WRAP_INDENT_OPTIONS + i, tch, COUNTOF(tch));
			lstrcat(tch, L"|");
			LPWSTR p1 = tch;
			LPWSTR p2;
			while ((p2 = StrChr(p1, L'|')) != nullptr) {
				*p2++ = L'\0';
				if (*p1) {
					ComboBox_AddString(hwndCtl, p1);
				}
				p1 = p2;
			}

			ComboBox_SetExtendedUI(hwndCtl, TRUE);
		}

		SendDlgItemMessage(hwnd, IDC_WRAP_INDENT, CB_SETCURSEL, iWordWrapIndent, 0);
		SendDlgItemMessage(hwnd, IDC_WRAP_SYMBOL_BEFORE, CB_SETCURSEL, bShowWordWrapSymbols ? (iWordWrapSymbols % 10) : 0, 0);
		SendDlgItemMessage(hwnd, IDC_WRAP_SYMBOL_AFTER, CB_SETCURSEL, bShowWordWrapSymbols ? (iWordWrapSymbols / 10) : 0, 0);
		SendDlgItemMessage(hwnd, IDC_WRAP_MODE, CB_SETCURSEL, iWordWrapMode, 0);

		if (bWordWrapSelectSubLine) {
			CheckDlgButton(hwnd, IDC_WRAP_SELECT_SUBLINE, BST_CHECKED);
		}
		if (bHighlightCurrentSubLine) {
			CheckDlgButton(hwnd, IDC_WRAP_HIGHLIGHT_SUBLINE, BST_CHECKED);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			iWordWrapIndent = static_cast<int>(SendDlgItemMessage(hwnd, IDC_WRAP_INDENT, CB_GETCURSEL, 0, 0));
			bShowWordWrapSymbols = false;
			int iSel = static_cast<int>(SendDlgItemMessage(hwnd, IDC_WRAP_SYMBOL_BEFORE, CB_GETCURSEL, 0, 0));
			const int iSel2 = static_cast<int>(SendDlgItemMessage(hwnd, IDC_WRAP_SYMBOL_AFTER, CB_GETCURSEL, 0, 0));
			if (iSel > 0 || iSel2 > 0) {
				bShowWordWrapSymbols = true;
				iWordWrapSymbols = iSel + iSel2 * 10;
			}

			iSel = static_cast<int>(SendDlgItemMessage(hwnd, IDC_WRAP_MODE, CB_GETCURSEL, 0, 0));
			if (iSel != SC_WRAP_NONE) {
				iWordWrapMode = iSel;
			}

			fWordWrapG = fvCurFile.fWordWrap = iSel != SC_WRAP_NONE;
			bWordWrapSelectSubLine = IsButtonChecked(hwnd, IDC_WRAP_SELECT_SUBLINE);
			bHighlightCurrentSubLine = IsButtonChecked(hwnd, IDC_WRAP_HIGHLIGHT_SUBLINE);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// WordWrapSettingsDlg()
//
bool WordWrapSettingsDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_WORDWRAP), hwnd, WordWrapSettingsDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// LongLineSettingsDlgProc()
//
//
extern int iLongLineMode;
extern int iLongLinesLimitG;

static INT_PTR CALLBACK LongLineSettingsDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		SetDlgItemInt(hwnd, IDC_LONGLINE_LIMIT, fvCurFile.iLongLinesLimit, FALSE);
		SendDlgItemMessage(hwnd, IDC_LONGLINE_LIMIT, EM_LIMITTEXT, 15, 0);

		if (iLongLineMode == EDGE_LINE) {
			CheckRadioButton(hwnd, IDC_LONGLINE_EDGE_LINE, IDC_LONGLINE_BACK_COLOR, IDC_LONGLINE_EDGE_LINE);
		} else {
			CheckRadioButton(hwnd, IDC_LONGLINE_EDGE_LINE, IDC_LONGLINE_BACK_COLOR, IDC_LONGLINE_BACK_COLOR);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			BOOL fTranslated;
			int longLinesLimit = GetDlgItemInt(hwnd, IDC_LONGLINE_LIMIT, &fTranslated, FALSE);

			if (fTranslated) {
				longLinesLimit = clamp(longLinesLimit, 0, NP2_LONG_LINE_LIMIT);
				fvCurFile.iLongLinesLimit = longLinesLimit;
				iLongLinesLimitG = longLinesLimit;
				iLongLineMode = IsButtonChecked(hwnd, IDC_LONGLINE_EDGE_LINE) ? EDGE_LINE : EDGE_BACKGROUND;

				EndDialog(hwnd, IDOK);
			} else {
				PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, IDC_LONGLINE_LIMIT)), TRUE);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// LongLineSettingsDlg()
//
bool LongLineSettingsDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_LONGLINES), hwnd, LongLineSettingsDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// TabSettingsDlgProc()
//
//
static void SyncGlobalTabSettings(HWND hwnd) noexcept {
	const bool useGlobal = IsButtonChecked(hwnd, IDC_SCHEME_USE_GLOBAL_TAB);
	if (useGlobal) {
		WCHAR wch[16];
		GetDlgItemText(hwnd, IDC_GLOBAL_TAB_WIDTH, wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_SCHEME_TAB_WIDTH, wch);
		GetDlgItemText(hwnd, IDC_GLOBAL_INDENT_WIDTH, wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_SCHEME_INDENT_WIDTH, wch);
		CheckDlgButton(hwnd, IDC_SCHEME_TAB_AS_SPACE, IsDlgButtonChecked(hwnd, IDC_GLOBAL_TAB_AS_SPACE));
	} else {
		SetDlgItemInt(hwnd, IDC_SCHEME_TAB_WIDTH, tabSettings.schemeTabWidth, FALSE);
		SetDlgItemInt(hwnd, IDC_SCHEME_INDENT_WIDTH, tabSettings.schemeIndentWidth, FALSE);
		CheckDlgButton(hwnd, IDC_SCHEME_TAB_AS_SPACE, tabSettings.schemeTabsAsSpaces ? BST_CHECKED : BST_UNCHECKED);
	}
	EnableWindow(GetDlgItem(hwnd, IDC_SCHEME_TAB_WIDTH), !useGlobal);
	EnableWindow(GetDlgItem(hwnd, IDC_SCHEME_INDENT_WIDTH), !useGlobal);
	EnableWindow(GetDlgItem(hwnd, IDC_SCHEME_TAB_AS_SPACE), !useGlobal);
}

static void SyncSchemeTabSettings(HWND hwnd) noexcept {
	const bool useScheme = IsButtonChecked(hwnd, IDC_FILE_USE_SCHEME_TAB);
	if (useScheme) {
		WCHAR wch[16];
		GetDlgItemText(hwnd, IDC_SCHEME_TAB_WIDTH, wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_FILE_TAB_WIDTH, wch);
		GetDlgItemText(hwnd, IDC_SCHEME_INDENT_WIDTH, wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_FILE_INDENT_WIDTH, wch);
		CheckDlgButton(hwnd, IDC_FILE_TAB_AS_SPACE, IsDlgButtonChecked(hwnd, IDC_SCHEME_TAB_AS_SPACE));
	} else {
		SetDlgItemInt(hwnd, IDC_FILE_TAB_WIDTH, fvCurFile.iTabWidth, FALSE);
		SetDlgItemInt(hwnd, IDC_FILE_INDENT_WIDTH, fvCurFile.iIndentWidth, FALSE);
		CheckDlgButton(hwnd, IDC_FILE_TAB_AS_SPACE, fvCurFile.bTabsAsSpaces ? BST_CHECKED : BST_UNCHECKED);
	}
	EnableWindow(GetDlgItem(hwnd, IDC_FILE_TAB_WIDTH), !useScheme);
	EnableWindow(GetDlgItem(hwnd, IDC_FILE_INDENT_WIDTH), !useScheme);
	EnableWindow(GetDlgItem(hwnd, IDC_FILE_TAB_AS_SPACE), !useScheme);
}

static INT_PTR CALLBACK TabSettingsDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		WCHAR wch[MAX_EDITLEXER_NAME_SIZE];
		Style_LoadTabSettings(pLexCurrent);
		LPCWSTR pszName = Style_GetCurrentLexerName(wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_SCHEME_TAB_GROUPBOX, pszName);
		if (StrIsEmpty(szCurFile)) {
			GetString(IDS_UNTITLED, wch, COUNTOF(wch));
			pszName = wch;
		} else {
			pszName = PathFindFileName(szCurFile);
		}
		SetDlgItemText(hwnd, IDC_FILE_TAB_GROUPBOX, pszName);

		SetDlgItemInt(hwnd, IDC_GLOBAL_TAB_WIDTH, tabSettings.globalTabWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_GLOBAL_TAB_WIDTH, EM_LIMITTEXT, 15, 0);
		SetDlgItemInt(hwnd, IDC_GLOBAL_INDENT_WIDTH, tabSettings.globalIndentWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_GLOBAL_INDENT_WIDTH, EM_LIMITTEXT, 15, 0);
		if (tabSettings.globalTabsAsSpaces) {
			CheckDlgButton(hwnd, IDC_GLOBAL_TAB_AS_SPACE, BST_CHECKED);
		}

		SetDlgItemInt(hwnd, IDC_SCHEME_TAB_WIDTH, tabSettings.schemeTabWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_SCHEME_TAB_WIDTH, EM_LIMITTEXT, 15, 0);
		SetDlgItemInt(hwnd, IDC_SCHEME_INDENT_WIDTH, tabSettings.schemeIndentWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_SCHEME_INDENT_WIDTH, EM_LIMITTEXT, 15, 0);
		if (tabSettings.schemeTabsAsSpaces) {
			CheckDlgButton(hwnd, IDC_SCHEME_TAB_AS_SPACE, BST_CHECKED);
		}
		if (tabSettings.schemeUseGlobalTabSettings) {
			CheckDlgButton(hwnd, IDC_SCHEME_USE_GLOBAL_TAB, BST_CHECKED);
			SyncGlobalTabSettings(hwnd);
		}

		SetDlgItemInt(hwnd, IDC_FILE_TAB_WIDTH, fvCurFile.iTabWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_FILE_TAB_WIDTH, EM_LIMITTEXT, 15, 0);
		SetDlgItemInt(hwnd, IDC_FILE_INDENT_WIDTH, fvCurFile.iIndentWidth, FALSE);
		SendDlgItemMessage(hwnd, IDC_FILE_INDENT_WIDTH, EM_LIMITTEXT, 15, 0);
		if (fvCurFile.bTabsAsSpaces) {
			CheckDlgButton(hwnd, IDC_FILE_TAB_AS_SPACE, BST_CHECKED);
		}
		const BOOL hasFileTabSettings = fvCurFile.mask & FV_MaskHasFileTabSettings;
		if (!hasFileTabSettings) {
			CheckDlgButton(hwnd, IDC_FILE_USE_SCHEME_TAB, BST_CHECKED);
			SyncSchemeTabSettings(hwnd);
		}

		if (fvCurFile.bTabIndents) {
			CheckDlgButton(hwnd, IDC_TAB_INDENT, BST_CHECKED);
		}
		if (tabSettings.bBackspaceUnindents & 2) {
			CheckDlgButton(hwnd, IDC_BACKSPACE_SMARTDEL, BST_CHECKED);
		}
		if (tabSettings.bBackspaceUnindents & 1) {
			CheckDlgButton(hwnd, IDC_BACKSPACE_UNINDENT, BST_CHECKED);
		}
		if (tabSettings.bDetectIndentation) {
			CheckDlgButton(hwnd, IDC_DETECT_INDENTATION, BST_CHECKED);
		}

		CenterDlgInParent(hwnd);
		if (hasFileTabSettings || !tabSettings.schemeUseGlobalTabSettings) {
			HWND hwndCtl = GetDlgItem(hwnd, hasFileTabSettings ? IDC_FILE_TAB_WIDTH : IDC_SCHEME_TAB_WIDTH);
			SetFocus(hwndCtl);
			PostMessage(hwndCtl, EM_SETSEL, 0, -1);
			return FALSE;
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			BOOL fTranslated1;
			BOOL fTranslated2;

			int iNewTabWidth = GetDlgItemInt(hwnd, IDC_GLOBAL_TAB_WIDTH, &fTranslated1, FALSE);
			int iNewIndentWidth = GetDlgItemInt(hwnd, IDC_GLOBAL_INDENT_WIDTH, &fTranslated2, FALSE);
			if (fTranslated1 && fTranslated2) {
				tabSettings.globalTabWidth = clamp(iNewTabWidth, TAB_WIDTH_MIN, TAB_WIDTH_MAX);
				tabSettings.globalIndentWidth = clamp(iNewIndentWidth, INDENT_WIDTH_MIN, INDENT_WIDTH_MAX);
				tabSettings.globalTabsAsSpaces = IsButtonChecked(hwnd, IDC_GLOBAL_TAB_AS_SPACE);
			} else {
				PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, fTranslated1 ? IDC_GLOBAL_INDENT_WIDTH : IDC_GLOBAL_TAB_WIDTH)), TRUE);
				break;
			}

			const bool useGlobal = IsButtonChecked(hwnd, IDC_SCHEME_USE_GLOBAL_TAB);
			tabSettings.schemeUseGlobalTabSettings = useGlobal;
			if (useGlobal) {
				tabSettings.schemeTabWidth = tabSettings.globalTabWidth;
				tabSettings.schemeIndentWidth = tabSettings.globalIndentWidth;
				tabSettings.schemeTabsAsSpaces = tabSettings.globalTabsAsSpaces;
			} else {
				iNewTabWidth = GetDlgItemInt(hwnd, IDC_SCHEME_TAB_WIDTH, &fTranslated1, FALSE);
				iNewIndentWidth = GetDlgItemInt(hwnd, IDC_SCHEME_INDENT_WIDTH, &fTranslated2, FALSE);
				if (fTranslated1 && fTranslated2) {
					tabSettings.schemeTabWidth = clamp(iNewTabWidth, TAB_WIDTH_MIN, TAB_WIDTH_MAX);
					tabSettings.schemeIndentWidth = clamp(iNewIndentWidth, INDENT_WIDTH_MIN, INDENT_WIDTH_MAX);
					tabSettings.schemeTabsAsSpaces = IsButtonChecked(hwnd, IDC_SCHEME_TAB_AS_SPACE);
				} else {
					PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, fTranslated1 ? IDC_SCHEME_INDENT_WIDTH : IDC_SCHEME_TAB_WIDTH)), TRUE);
					break;
				}
			}

			if (IsButtonChecked(hwnd, IDC_FILE_USE_SCHEME_TAB)) {
				fvCurFile.mask &= ~FV_MaskHasFileTabSettings;
				fvCurFile.iTabWidth = tabSettings.schemeTabWidth;
				fvCurFile.iIndentWidth = tabSettings.schemeIndentWidth;
				fvCurFile.bTabsAsSpaces = tabSettings.schemeTabsAsSpaces;
			} else {
				iNewTabWidth = GetDlgItemInt(hwnd, IDC_FILE_TAB_WIDTH, &fTranslated1, FALSE);
				iNewIndentWidth = GetDlgItemInt(hwnd, IDC_FILE_INDENT_WIDTH, &fTranslated2, FALSE);
				if (fTranslated1 && fTranslated2) {
					fvCurFile.mask |= FV_MaskHasFileTabSettings;
					fvCurFile.iTabWidth = clamp(iNewTabWidth, TAB_WIDTH_MIN, TAB_WIDTH_MAX);
					fvCurFile.iIndentWidth = clamp(iNewIndentWidth, INDENT_WIDTH_MIN, INDENT_WIDTH_MAX);
					fvCurFile.bTabsAsSpaces = IsButtonChecked(hwnd, IDC_FILE_TAB_AS_SPACE);
				} else {
					PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(GetDlgItem(hwnd, fTranslated1 ? IDC_FILE_INDENT_WIDTH : IDC_FILE_TAB_WIDTH)), TRUE);
					break;
				}
			}

			fvCurFile.bTabIndents = IsButtonChecked(hwnd, IDC_TAB_INDENT);
			tabSettings.bTabIndents = fvCurFile.bTabIndents;
			tabSettings.bBackspaceUnindents = IsButtonChecked(hwnd, IDC_BACKSPACE_UNINDENT);
			if (IsButtonChecked(hwnd, IDC_BACKSPACE_SMARTDEL)) {
				tabSettings.bBackspaceUnindents |= 2;
			}
			tabSettings.bDetectIndentation = IsButtonChecked(hwnd, IDC_DETECT_INDENTATION);
			Style_SaveTabSettings(pLexCurrent);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDC_GLOBAL_TAB_WIDTH:
		case IDC_GLOBAL_INDENT_WIDTH:
			if (HIWORD(wParam) == EN_CHANGE) {
				SyncGlobalTabSettings(hwnd);
				SyncSchemeTabSettings(hwnd);
			}
			break;

		case IDC_GLOBAL_TAB_AS_SPACE:
		case IDC_SCHEME_USE_GLOBAL_TAB:
			SyncGlobalTabSettings(hwnd);
			SyncSchemeTabSettings(hwnd);
			break;

		case IDC_SCHEME_TAB_WIDTH:
		case IDC_SCHEME_INDENT_WIDTH:
			if (HIWORD(wParam) == EN_CHANGE) {
				SyncSchemeTabSettings(hwnd);
			}
			break;

		case IDC_SCHEME_TAB_AS_SPACE:
		case IDC_FILE_USE_SCHEME_TAB:
			SyncSchemeTabSettings(hwnd);
			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// TabSettingsDlg()
//
bool TabSettingsDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_TABSETTINGS), hwnd, TabSettingsDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// SelectDefEncodingDlgProc()
//
//
struct ENCODEDLG {
	bool bRecodeOnly;
	int  idEncoding;
	int  cxDlg;
	int  cyDlg;
	UINT uidLabel;
};

static INT_PTR CALLBACK SelectDefEncodingDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);

		const int iEncoding = *(AsPointer<int *>(lParam));
		Encoding_GetLabel(iEncoding);
		SetDlgItemText(hwnd, IDC_ENCODING_LABEL, mEncoding[iEncoding].wchLabel);

		if (bSkipUnicodeDetection) {
			CheckDlgButton(hwnd, IDC_NOUNICODEDETECTION, BST_CHECKED);
		}

		if (bLoadANSIasUTF8) {
			CheckDlgButton(hwnd, IDC_ANSIASUTF8, BST_CHECKED);
		}
		if (bLoadASCIIasUTF8) {
			CheckDlgButton(hwnd, IDC_ASCIIASUTF8, BST_CHECKED);
		}

		if (bLoadNFOasOEM) {
			CheckDlgButton(hwnd, IDC_NFOASOEM, BST_CHECKED);
		}

		if (bNoEncodingTags) {
			CheckDlgButton(hwnd, IDC_ENCODINGFROMFILEVARS, BST_CHECKED);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_NOTIFY: {
		LPNMHDR pnmhdr = AsPointer<LPNMHDR>(lParam);
		switch (pnmhdr->code) {
		case NM_CLICK:
		case NM_RETURN:
			if (pnmhdr->idFrom == IDC_ENCODING_LINK) {
				int *pidREncoding = AsPointer<int *>(GetWindowLongPtr(hwnd, DWLP_USER));
				if (SelectEncodingDlg(hwndMain, pidREncoding, IDS_SELRECT_DEFAULT_ENCODING)) {
					Encoding_GetLabel(*pidREncoding);
					SetDlgItemText(hwnd, IDC_ENCODING_LABEL, mEncoding[*pidREncoding].wchLabel);
				}
			}
			break;
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			bSkipUnicodeDetection = IsButtonChecked(hwnd, IDC_NOUNICODEDETECTION);
			bLoadANSIasUTF8 = IsButtonChecked(hwnd, IDC_ANSIASUTF8);
			bLoadASCIIasUTF8 = IsButtonChecked(hwnd, IDC_ASCIIASUTF8);
			bLoadNFOasOEM = IsButtonChecked(hwnd, IDC_NFOASOEM);
			bNoEncodingTags = IsButtonChecked(hwnd, IDC_ENCODINGFROMFILEVARS);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//=============================================================================
//
// SelectDefEncodingDlg()
//
bool SelectDefEncodingDlg(HWND hwnd, int *pidREncoding) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DEFENCODING), hwnd, SelectDefEncodingDlgProc, AsInteger<LPARAM>(pidREncoding));
	return iResult == IDOK;
}

//=============================================================================
//
// SelectEncodingDlgProc()
//
//
static INT_PTR CALLBACK SelectEncodingDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		const ENCODEDLG * const pdd = AsPointer<const ENCODEDLG*>(lParam);
		ResizeDlg_Init(hwnd, pdd->cxDlg, pdd->cyDlg, IDC_RESIZEGRIP);

		WCHAR wch[256];
		GetString(pdd->uidLabel, wch, COUNTOF(wch));
		SetDlgItemText(hwnd, IDC_ENCODING_LABEL, wch);

		// TODO: following code is buggy when icon size for shfi.hIcon is larger than bmp.bmHeight,
		// we need to determine icon size first, then resize the encoding mask bitmap accordingly.

		const int resource = GetBitmapResourceIdForCurrentDPI(IDB_ENCODING16);
		HBITMAP hbmp = static_cast<HBITMAP>(LoadImage(g_exeInstance, MAKEINTRESOURCE(resource), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
		hbmp = ResizeImageForCurrentDPI(hbmp);
		BITMAP bmp;
		GetObject(hbmp, sizeof(BITMAP), &bmp);
		HIMAGELIST himl = ImageList_Create(bmp.bmHeight, bmp.bmHeight, ILC_COLOR32 | ILC_MASK, 0, 0);
		ImageList_AddMasked(himl, hbmp, CLR_DEFAULT);
		DeleteObject(hbmp);

		// folder icon
		const DWORD iconFlags = GetCurrentIconHandleFlags();
		SHFILEINFO shfi;
		SHGetFileInfo(L"Icon", FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(SHFILEINFO), iconFlags);
		ImageList_AddIcon(himl, shfi.hIcon);

		HWND hwndTV = GetDlgItem(hwnd, IDC_ENCODINGLIST);
		InitWindowCommon(hwndTV);
		TreeView_SetExtendedStyle(hwndTV, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
		SetExplorerTheme(hwndTV);

		TreeView_SetImageList(hwndTV, himl, TVSIL_NORMAL);
		Encoding_AddToTreeView(hwndTV, pdd->idEncoding, pdd->bRecodeOnly);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_DESTROY: {
		ENCODEDLG * pdd = AsPointer<ENCODEDLG *>(GetWindowLongPtr(hwnd, DWLP_USER));
		ResizeDlg_Destroy(hwnd, &pdd->cxDlg, &pdd->cyDlg);
		HIMAGELIST himl = TreeView_GetImageList(GetDlgItem(hwnd, IDC_ENCODINGLIST), TVSIL_NORMAL);
		ImageList_Destroy(himl);
	}
	return FALSE;

	case WM_SIZE: {
		int dx;
		int dy;

		ResizeDlg_Size(hwnd, lParam, &dx, &dy);

		HDWP hdwp = BeginDeferWindowPos(4);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_RESIZEGRIP, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDOK, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDCANCEL, dx, dy, SWP_NOSIZE);
		hdwp = DeferCtlPos(hdwp, hwnd, IDC_ENCODINGLIST, dx, dy, SWP_NOMOVE);
		EndDeferWindowPos(hdwp);
	}
	return TRUE;

	case WM_GETMINMAXINFO:
		ResizeDlg_GetMinMaxInfo(hwnd, lParam);
		return TRUE;

	case WM_NOTIFY: {
		LPNMHDR lpnmh = AsPointer<LPNMHDR>(lParam);
		if (lpnmh->idFrom == IDC_ENCODINGLIST) {
			switch (lpnmh->code) {
			case NM_DBLCLK: {
				int temp = -1;
				if (Encoding_GetFromTreeView(GetDlgItem(hwnd, IDC_ENCODINGLIST), &temp, true)) {
					SendWMCommand(hwnd, IDOK);
				}
			}
			break;

			case TVN_SELCHANGED: {
				LPNMTREEVIEW lpnmtv = AsPointer<LPNMTREEVIEW>(lParam);
				EnableWindow(GetDlgItem(hwnd, IDOK), lpnmtv->itemNew.lParam != 0);
				if (lpnmtv->itemNew.lParam == 0) {
					TreeView_Expand(GetDlgItem(hwnd, IDC_ENCODINGLIST), lpnmtv->itemNew.hItem, TVE_EXPAND);
				}
			}
			break;
			}
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			HWND hwndTV = GetDlgItem(hwnd, IDC_ENCODINGLIST);
			ENCODEDLG * pdd = AsPointer<ENCODEDLG *>(GetWindowLongPtr(hwnd, DWLP_USER));
			if (Encoding_GetFromTreeView(hwndTV, &pdd->idEncoding, false)) {
				EndDialog(hwnd, IDOK);
			} else {
				PostMessage(hwnd, WM_NEXTDLGCTL, AsInteger<WPARAM>(hwndTV), TRUE);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=============================================================================
//
// SelectEncodingDlg()
//
extern int cxEncodingDlg;
extern int cyEncodingDlg;

bool SelectEncodingDlg(HWND hwnd, int *pidREncoding, UINT uidLabel) noexcept {
	ENCODEDLG dd;

	dd.bRecodeOnly = (uidLabel == IDS_SELRECT_RELOAD_ENCODING);
	dd.idEncoding = *pidREncoding;
	dd.cxDlg = cxEncodingDlg;
	dd.cyDlg = cyEncodingDlg;
	dd.uidLabel = uidLabel;

	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SELECT_ENCODING), hwnd, SelectEncodingDlgProc, AsInteger<LPARAM>(&dd));

	cxEncodingDlg = dd.cxDlg;
	cyEncodingDlg = dd.cyDlg;

	if (iResult == IDOK) {
		*pidREncoding = dd.idEncoding;
		return true;
	}
	return false;
}

//=============================================================================
//
// SelectDefLineEndingDlgProc()
//
static INT_PTR CALLBACK SelectDefLineEndingDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		const int iOption = *(AsPointer<int *>(lParam));

		// Load options
		HWND hwndCtl = GetDlgItem(hwnd, IDC_EOLMODELIST);
		WCHAR wch[128];
		for (int i = 0; i < 3; i++) {
			GetString(IDS_EOLMODENAME_CRLF + i, wch, COUNTOF(wch));
			ComboBox_AddString(hwndCtl, wch);
		}

		ComboBox_SetCurSel(hwndCtl, iOption);
		ComboBox_SetExtendedUI(hwndCtl, TRUE);

		if (bWarnLineEndings) {
			CheckDlgButton(hwnd, IDC_WARNINCONSISTENTEOLS, BST_CHECKED);
		}

		if (bFixLineEndings) {
			CheckDlgButton(hwnd, IDC_CONSISTENTEOLS, BST_CHECKED);
		}

		if (bAutoStripBlanks) {
			CheckDlgButton(hwnd, IDC_AUTOSTRIPBLANKS, BST_CHECKED);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			int *piOption = AsPointer<int *>(GetWindowLongPtr(hwnd, DWLP_USER));
			*piOption = static_cast<int>(SendDlgItemMessage(hwnd, IDC_EOLMODELIST, CB_GETCURSEL, 0, 0));
			bWarnLineEndings = IsButtonChecked(hwnd, IDC_WARNINCONSISTENTEOLS);
			bFixLineEndings = IsButtonChecked(hwnd, IDC_CONSISTENTEOLS);
			bAutoStripBlanks = IsButtonChecked(hwnd, IDC_AUTOSTRIPBLANKS);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//=============================================================================
//
// SelectDefLineEndingDlg()
//
bool SelectDefLineEndingDlg(HWND hwnd, int *iOption) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DEFEOLMODE), hwnd, SelectDefLineEndingDlgProc, AsInteger<LPARAM>(iOption));
	return iResult == IDOK;
}

static INT_PTR CALLBACK WarnLineEndingDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		const EditFileIOStatus * const status = AsPointer<EditFileIOStatus *>(lParam);
		const int iEOLMode = GetSettingsEOLMode(status->iEOLMode);

		// Load options
		HWND hwndCtl = GetDlgItem(hwnd, IDC_EOLMODELIST);
		WCHAR wch[128];
		for (int i = 0; i < 3; i++) {
			GetString(IDS_EOLMODENAME_CRLF + i, wch, COUNTOF(wch));
			ComboBox_AddString(hwndCtl, wch);
		}

		ComboBox_SetCurSel(hwndCtl, iEOLMode);
		ComboBox_SetExtendedUI(hwndCtl, TRUE);

		WCHAR tchFmt[128];
		for (int i = 0; i < 3; i++) {
			WCHAR tchLn[32];
			FormatNumber(tchLn, status->linesCount[i]);
			GetDlgItemText(hwnd, IDC_EOL_SUM_CRLF + i, tchFmt, COUNTOF(tchFmt));
			wsprintf(wch, tchFmt, tchLn);
			SetDlgItemText(hwnd, IDC_EOL_SUM_CRLF + i, wch);
		}

		if (bWarnLineEndings) {
			CheckDlgButton(hwnd, IDC_WARNINCONSISTENTEOLS, BST_CHECKED);
		}
		if (status->bLineEndingsDefaultNo) {
			SendMessage(hwnd, DM_SETDEFID, IDCANCEL, 0);
		}
		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			EditFileIOStatus *status = AsPointer<EditFileIOStatus *>(GetWindowLongPtr(hwnd, DWLP_USER));
			const int iEOLMode = static_cast<int>(SendDlgItemMessage(hwnd, IDC_EOLMODELIST, CB_GETCURSEL, 0, 0));
			status->iEOLMode = iEOLMode;
			bWarnLineEndings = IsButtonChecked(hwnd, IDC_WARNINCONSISTENTEOLS);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			bWarnLineEndings = IsButtonChecked(hwnd, IDC_WARNINCONSISTENTEOLS);
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

bool WarnLineEndingDlg(HWND hwnd, EditFileIOStatus *status) noexcept {
	MessageBeep(MB_ICONEXCLAMATION);
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_WARNLINEENDS), hwnd, WarnLineEndingDlgProc, AsInteger<LPARAM>(status));
	return iResult == IDOK;
}

void InitZoomLevelComboBox(HWND hwnd, int nCtlId, int zoomLevel) noexcept {
	WCHAR tch[16];
	int selIndex = -1;
	static const short levelList[] = {
		500, 450, 400, 350, 300, 250,
		200, 175, 150, 125, 100, 75, 50, 25,
	};

	HWND hwndCtl = GetDlgItem(hwnd, nCtlId);
	ComboBox_LimitText(hwndCtl, 8);
	for (UINT i = 0; i < COUNTOF(levelList); i++) {
		const int level = levelList[i];
		if (zoomLevel == level) {
			selIndex = i;
		}
		wsprintf(tch, L"%d%%", level);
		ComboBox_AddString(hwndCtl, tch);
	}

	ComboBox_SetExtendedUI(hwndCtl, TRUE);
	ComboBox_SetCurSel(hwndCtl, selIndex);
	if (selIndex < 0) {
		wsprintf(tch, L"%d%%", zoomLevel);
		SetWindowText(hwndCtl, tch);
	}
}

bool GetZoomLevelComboBoxValue(HWND hwnd, int nCtrId, int *zoomLevel) noexcept {
	WCHAR tch[16];
	GetDlgItemText(hwnd, nCtrId, tch, COUNTOF(tch));
	return CRTStrToInt(tch, zoomLevel) && *zoomLevel >= SC_MIN_ZOOM_LEVEL && *zoomLevel <= SC_MAX_ZOOM_LEVEL;
}

static INT_PTR CALLBACK ZoomLevelDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		const int zoomLevel = SciCall_GetZoom();
		InitZoomLevelComboBox(hwnd, IDC_ZOOMLEVEL, zoomLevel);
		if (lParam) {
			SetToRightBottom(hwnd);
		} else {
			CenterDlgInParent(hwnd);
		}
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDYES: {
			int zoomLevel;
			if (GetZoomLevelComboBoxValue(hwnd, IDC_ZOOMLEVEL, &zoomLevel)) {
				SciCall_SetZoom(zoomLevel);
			}
			if (LOWORD(wParam) == IDOK) {
				EndDialog(hwnd, IDOK);
			}
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

void ZoomLevelDlg(HWND hwnd, bool bBottom) noexcept {
	ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_ZOOMLEVEL), hwnd, ZoomLevelDlgProc, bBottom);
}

extern EditAutoCompletionConfig autoCompletionConfig;

static INT_PTR CALLBACK AutoCompletionSettingsDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		int mask = autoCompletionConfig.iCompleteOption;
		if (autoCompletionConfig.bIndentText) {
			CheckDlgButton(hwnd, IDC_AUTO_INDENT_TEXT, BST_CHECKED);
		}
		if (mask & AutoCompletionOption_CloseTags) {
			CheckDlgButton(hwnd, IDC_AUTO_CLOSE_TAGS, BST_CHECKED);
		}
		if (mask & AutoCompletionOption_CompleteWord) {
			CheckDlgButton(hwnd, IDC_AUTO_COMPLETE_WORD, BST_CHECKED);
		}
		if (mask & AutoCompletionOption_ScanWordsInDocument) {
			CheckDlgButton(hwnd, IDC_AUTOC_SCAN_DOCUMENT_WORDS, BST_CHECKED);
		}
		if (mask & AutoCompletionOption_OnlyWordsInDocument) {
			CheckDlgButton(hwnd, IDC_AUTOC_ONLY_DOCUMENT_WORDS, BST_CHECKED);
		}
		if (mask & AutoCompletionOption_EnglishIMEModeOnly) {
			CheckDlgButton(hwnd, IDC_AUTOC_ENGLISH_IME_ONLY, BST_CHECKED);
		}

		SetDlgItemInt(hwnd, IDC_AUTOC_VISIBLE_ITEM_COUNT, autoCompletionConfig.iVisibleItemCount, FALSE);
		SendDlgItemMessage(hwnd, IDC_AUTOC_VISIBLE_ITEM_COUNT, EM_LIMITTEXT, 8, 0);

		SetDlgItemInt(hwnd, IDC_AUTOC_MIN_WORD_LENGTH, autoCompletionConfig.iMinWordLength, FALSE);
		SendDlgItemMessage(hwnd, IDC_AUTOC_MIN_WORD_LENGTH, EM_LIMITTEXT, 8, 0);

		SetDlgItemInt(hwnd, IDC_AUTOC_MIN_NUMBER_LENGTH, autoCompletionConfig.iMinNumberLength, FALSE);
		SendDlgItemMessage(hwnd, IDC_AUTOC_MIN_NUMBER_LENGTH, EM_LIMITTEXT, 8, 0);

		WCHAR wch[32];
		wsprintf(wch, L"%u ms", autoCompletionConfig.dwScanWordsTimeout);
		SetDlgItemText(hwnd, IDC_AUTOC_SCAN_WORDS_TIMEOUT, wch);

		mask = autoCompletionConfig.fCompleteScope;
		if (mask & AutoCompleteScope_Commont) {
			CheckDlgButton(hwnd, IDC_AUTO_COMPLETE_INSIDE_COMMONT, BST_CHECKED);
		}
		if (mask & AutoCompleteScope_String) {
			CheckDlgButton(hwnd, IDC_AUTO_COMPLETE_INSIDE_STRING, BST_CHECKED);
		}
		if (mask & AutoCompleteScope_PlainText) {
			CheckDlgButton(hwnd, IDC_AUTO_COMPLETE_INSIDE_PLAINTEXT, BST_CHECKED);
		}

		mask = autoCompletionConfig.fScanWordScope;
		if (mask & AutoCompleteScope_Commont) {
			CheckDlgButton(hwnd, IDC_SCAN_WORD_INSIDE_COMMONT, BST_CHECKED);
		}
		if (mask & AutoCompleteScope_String) {
			CheckDlgButton(hwnd, IDC_SCAN_WORD_INSIDE_STRING, BST_CHECKED);
		}
		if (mask & AutoCompleteScope_PlainText) {
			CheckDlgButton(hwnd, IDC_SCAN_WORD_INSIDE_PLAINTEXT, BST_CHECKED);
		}

		mask = autoCompletionConfig.fAutoCompleteFillUpMask;
		if (mask & AutoCompleteFillUpMask_Enter) {
			CheckDlgButton(hwnd, IDC_AUTOC_FILLUP_ENTER, BST_CHECKED);
		}
		if (mask & AutoCompleteFillUpMask_Tab) {
			CheckDlgButton(hwnd, IDC_AUTOC_FILLUP_TAB, BST_CHECKED);
		}
		if (mask & AutoCompleteFillUpMask_Space) {
			CheckDlgButton(hwnd, IDC_AUTOC_FILLUP_SPACE, BST_CHECKED);
		}
		if (mask & AutoCompleteFillUpMask_Punctuation) {
			CheckDlgButton(hwnd, IDC_AUTOC_FILLUP_PUNCTUATION, BST_CHECKED);
		}

		SetDlgItemText(hwnd, IDC_AUTOC_FILLUP_PUNCTUATION_LIST, autoCompletionConfig.wszAutoCompleteFillUp);
		SendDlgItemMessage(hwnd, IDC_AUTOC_FILLUP_PUNCTUATION_LIST, EM_LIMITTEXT, MAX_AUTO_COMPLETION_FILLUP_LENGTH, 0);

		mask = autoCompletionConfig.fAutoInsertMask;
		if (mask & AutoInsertMask_Parenthesis) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_PARENTHESIS, BST_CHECKED);
		}
		if (mask & AutoInsertMask_Brace) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_BRACE, BST_CHECKED);
		}
		if (mask & AutoInsertMask_SquareBracket) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_SQUARE_BRACKET, BST_CHECKED);
		}
		if (mask & AutoInsertMask_AngleBracket) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_ANGLE_BRACKET, BST_CHECKED);
		}
		if (mask & AutoInsertMask_DoubleQuote) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_DOUBLE_QUOTE, BST_CHECKED);
		}
		if (mask & AutoInsertMask_SingleQuote) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_SINGLE_QUOTE, BST_CHECKED);
		}
		if (mask & AutoInsertMask_Backtick) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_BACKTICK, BST_CHECKED);
		}
		if (mask & AutoInsertMask_SpaceAfterComma) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_SPACE_COMMA, BST_CHECKED);
		}
		if (mask & AutoInsertMask_SpaceAfterComment) {
			CheckDlgButton(hwnd, IDC_AUTO_INSERT_SPACE_COMMENT, BST_CHECKED);
		}

		mask = autoCompletionConfig.iAsmLineCommentChar;
		CheckRadioButton(hwnd, IDC_ASM_LINE_COMMENT_SEMICOLON, IDC_ASM_LINE_COMMENT_AT, IDC_ASM_LINE_COMMENT_SEMICOLON + mask);

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			autoCompletionConfig.bIndentText = IsButtonChecked(hwnd, IDC_AUTO_INDENT_TEXT);

			int mask = AutoCompletionOption_None;
			if (IsButtonChecked(hwnd, IDC_AUTO_CLOSE_TAGS)) {
				mask |= AutoCompletionOption_CloseTags;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_COMPLETE_WORD)) {
				mask |= AutoCompletionOption_CompleteWord;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_SCAN_DOCUMENT_WORDS)) {
				mask |= AutoCompletionOption_ScanWordsInDocument;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_ONLY_DOCUMENT_WORDS)) {
				mask |= AutoCompletionOption_OnlyWordsInDocument;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_ENGLISH_IME_ONLY)) {
				mask |= AutoCompletionOption_EnglishIMEModeOnly;
			}
			autoCompletionConfig.iCompleteOption = mask;

			mask = GetDlgItemInt(hwnd, IDC_AUTOC_VISIBLE_ITEM_COUNT, nullptr, FALSE);
			autoCompletionConfig.iVisibleItemCount = max(mask, MIN_AUTO_COMPLETION_VISIBLE_ITEM_COUNT);

			mask = GetDlgItemInt(hwnd, IDC_AUTOC_MIN_WORD_LENGTH, nullptr, FALSE);
			autoCompletionConfig.iMinWordLength = max(mask, MIN_AUTO_COMPLETION_WORD_LENGTH);

			mask = GetDlgItemInt(hwnd, IDC_AUTOC_MIN_NUMBER_LENGTH, nullptr, FALSE);
			autoCompletionConfig.iMinNumberLength = max(mask, MIN_AUTO_COMPLETION_NUMBER_LENGTH);

			WCHAR wch[32];
			GetDlgItemText(hwnd, IDC_AUTOC_SCAN_WORDS_TIMEOUT, wch, COUNTOF(wch));
			if (CRTStrToInt(wch, &mask)) {
				autoCompletionConfig.dwScanWordsTimeout = max(mask, AUTOC_SCAN_WORDS_MIN_TIMEOUT);
			}

			mask = AutoCompleteScope_Other;
			if (IsButtonChecked(hwnd, IDC_AUTO_COMPLETE_INSIDE_COMMONT)) {
				mask |= AutoCompleteScope_Commont;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_COMPLETE_INSIDE_STRING)) {
				mask |= AutoCompleteScope_String;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_COMPLETE_INSIDE_PLAINTEXT)) {
				mask |= AutoCompleteScope_PlainText;
			}
			autoCompletionConfig.fCompleteScope = mask;

			mask = AutoCompleteScope_Other;
			if (IsButtonChecked(hwnd, IDC_SCAN_WORD_INSIDE_COMMONT)) {
				mask |= AutoCompleteScope_Commont;
			}
			if (IsButtonChecked(hwnd, IDC_SCAN_WORD_INSIDE_STRING)) {
				mask |= AutoCompleteScope_String;
			}
			if (IsButtonChecked(hwnd, IDC_SCAN_WORD_INSIDE_PLAINTEXT)) {
				mask |= AutoCompleteScope_PlainText;
			}
			autoCompletionConfig.fScanWordScope = mask;

			mask = AutoCompleteFillUpMask_None;
			if (IsButtonChecked(hwnd, IDC_AUTOC_FILLUP_ENTER)) {
				mask |= AutoCompleteFillUpMask_Enter;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_FILLUP_TAB)) {
				mask |= AutoCompleteFillUpMask_Tab;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_FILLUP_SPACE)) {
				mask |= AutoCompleteFillUpMask_Space;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOC_FILLUP_PUNCTUATION)) {
				mask |= AutoCompleteFillUpMask_Punctuation;
			}

			autoCompletionConfig.fAutoCompleteFillUpMask = mask;
			GetDlgItemText(hwnd, IDC_AUTOC_FILLUP_PUNCTUATION_LIST, autoCompletionConfig.wszAutoCompleteFillUp, COUNTOF(autoCompletionConfig.wszAutoCompleteFillUp));

			mask = AutoInsertMask_None;
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_PARENTHESIS)) {
				mask |= AutoInsertMask_Parenthesis;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_BRACE)) {
				mask |= AutoInsertMask_Brace;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_SQUARE_BRACKET)) {
				mask |= AutoInsertMask_SquareBracket;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_ANGLE_BRACKET)) {
				mask |= AutoInsertMask_AngleBracket;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_DOUBLE_QUOTE)) {
				mask |= AutoInsertMask_DoubleQuote;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_SINGLE_QUOTE)) {
				mask |= AutoInsertMask_SingleQuote;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_BACKTICK)) {
				mask |= AutoInsertMask_Backtick;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_SPACE_COMMA)) {
				mask |= AutoInsertMask_SpaceAfterComma;
			}
			if (IsButtonChecked(hwnd, IDC_AUTO_INSERT_SPACE_COMMENT)) {
				mask |= AutoInsertMask_SpaceAfterComment;
			}

			autoCompletionConfig.fAutoInsertMask = mask;
			autoCompletionConfig.iAsmLineCommentChar = GetCheckedRadioButton(hwnd, IDC_ASM_LINE_COMMENT_SEMICOLON, IDC_ASM_LINE_COMMENT_AT) - IDC_ASM_LINE_COMMENT_SEMICOLON;
			EditCompleteUpdateConfig();
			SciCall_SetAutoInsertMask(mask);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

bool AutoCompletionSettingsDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_AUTOCOMPLETION), hwnd, AutoCompletionSettingsDlgProc, 0);
	return iResult == IDOK;
}

extern int iAutoSaveOption;
extern DWORD dwAutoSavePeriod;

static INT_PTR CALLBACK AutoSaveSettingsDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		if (iAutoSaveOption & AutoSaveOption_Periodic) {
			CheckDlgButton(hwnd, IDC_AUTOSAVE_ENABLE, BST_CHECKED);
		}
		if (iAutoSaveOption & AutoSaveOption_Suspend) {
			CheckDlgButton(hwnd, IDC_AUTOSAVE_SUSPEND, BST_CHECKED);
		}
		if (iAutoSaveOption & AutoSaveOption_Shutdown) {
			CheckDlgButton(hwnd, IDC_AUTOSAVE_SHUTDOWN, BST_CHECKED);
		}
		if (iAutoSaveOption & AutoSaveOption_ManuallyDelete) {
			CheckDlgButton(hwnd, IDC_AUTOSAVE_MANUALLYDELETE, BST_CHECKED);
		}
		if (iAutoSaveOption & AutoSaveOption_OverwriteCurrent) {
			CheckDlgButton(hwnd, IDC_AUTOSAVE_OVERWRITECURRENT, BST_CHECKED);
		}

		WCHAR tch[32];
		const UINT seconds = dwAutoSavePeriod / 1000;
		const UINT milliseconds = dwAutoSavePeriod % 1000;
		if (milliseconds) {
			wsprintf(tch, L"%u.%03u", seconds, milliseconds);
		} else {
			wsprintf(tch, L"%u.0", seconds);
		}
		SetDlgItemText(hwnd, IDC_AUTOSAVE_PERIOD, tch);

		CenterDlgInParent(hwnd);
	};
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			int option = 0;
			if (IsButtonChecked(hwnd, IDC_AUTOSAVE_ENABLE)) {
				option |= AutoSaveOption_Periodic;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOSAVE_SUSPEND)) {
				option |= AutoSaveOption_Suspend;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOSAVE_SHUTDOWN)) {
				option |= AutoSaveOption_Shutdown;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOSAVE_MANUALLYDELETE)) {
				option |= AutoSaveOption_ManuallyDelete;
			}
			if (IsButtonChecked(hwnd, IDC_AUTOSAVE_OVERWRITECURRENT)) {
				option |= AutoSaveOption_OverwriteCurrent;
			}
			iAutoSaveOption = option;

			WCHAR tch[32] = L"";
			GetDlgItemText(hwnd, IDC_AUTOSAVE_PERIOD, tch, COUNTOF(tch));
			float period = 0;
			StrToFloat(tch, &period);
			dwAutoSavePeriod = static_cast<int>(period * 1000);
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDC_AUTOSAVE_OPENFOLDER: {
			LPCWSTR szFolder = AutoSave_GetDefaultFolder();
			OpenContainingFolder(hwnd, szFolder, false);
		} break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

bool AutoSaveSettingsDlg(HWND hwnd) noexcept {
	const INT_PTR iResult = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_AUTOSAVE), hwnd, AutoSaveSettingsDlgProc, 0);
	return iResult == IDOK;
}

//=============================================================================
//
// InfoBoxDlgProc()
//
//
namespace {

struct INFOBOX {
	LPWSTR lpstrMessage;
	LPCWSTR lpstrSetting;
	HICON hIcon;
	bool   bDisableCheckBox;
};

enum SuppressMmessage {
	SuppressMmessage_None = 0,
	SuppressMmessage_Suppress,
	SuppressMmessage_Never,
};

INT_PTR CALLBACK InfoBoxDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	switch (umsg) {
	case WM_INITDIALOG: {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		const INFOBOX * const lpib = AsPointer<const INFOBOX *>(lParam);

		SendDlgItemMessage(hwnd, IDC_INFOBOXICON, STM_SETICON, AsInteger<WPARAM>(lpib->hIcon), 0);
		SetDlgItemText(hwnd, IDC_INFOBOXTEXT, lpib->lpstrMessage);
		if (lpib->bDisableCheckBox) {
			EnableWindow(GetDlgItem(hwnd, IDC_INFOBOXCHECK), FALSE);
		}
		NP2HeapFree(lpib->lpstrMessage);
		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_CTLCOLORSTATIC: {
		const DWORD dwId = GetWindowLong(AsPointer<HWND>(lParam), GWL_ID);

		if (dwId >= IDC_INFOBOXRECT && dwId <= IDC_INFOBOXTEXT) {
			HDC hdc = AsPointer<HDC>(wParam);
			SetBkMode(hdc, TRANSPARENT);
			return AsInteger<LONG_PTR>(GetSysColorBrush(COLOR_WINDOW));
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
		case IDYES:
		case IDNO:
			if (IsButtonChecked(hwnd, IDC_INFOBOXCHECK)) {
				const INFOBOX * const lpib = AsPointer<const INFOBOX *>(GetWindowLongPtr(hwnd, DWLP_USER));
				IniSetBool(INI_SECTION_NAME_SUPPRESSED_MESSAGES, lpib->lpstrSetting, true);
			}
			EndDialog(hwnd, LOWORD(wParam));
			break;
		}
		return TRUE;
	}
	return FALSE;
}

}

//=============================================================================
//
// InfoBox()
//
//
INT_PTR InfoBox(UINT uType, LPCWSTR lpstrSetting, UINT uidMessage, ...) noexcept {
	const UINT icon = uType & MB_ICONMASK;
	uType &= MB_TYPEMASK;
	const SuppressMmessage iMode = static_cast<SuppressMmessage>(IniGetInt(INI_SECTION_NAME_SUPPRESSED_MESSAGES, lpstrSetting, SuppressMmessage_None));
	if (StrNotEmpty(lpstrSetting) && iMode == SuppressMmessage_Suppress) {
		return (uType == MB_YESNO) ? IDYES : IDOK;
	}

	WCHAR wchFormat[512];
	GetString(uidMessage, wchFormat, COUNTOF(wchFormat));

	INFOBOX ib;
	ib.lpstrMessage = static_cast<LPWSTR>(NP2HeapAlloc(1024 * sizeof(WCHAR)));

	va_list va;
	va_start(va, uidMessage);
	wvsprintf(ib.lpstrMessage, wchFormat, va);
	va_end(va);

	ib.lpstrSetting = lpstrSetting;

#if 0//_WIN32_WINNT >= _WIN32_WINNT_VISTA
	SHSTOCKICONINFO sii;
	sii.cbSize = sizeof(SHSTOCKICONINFO);
	sii.hIcon = nullptr;
	const SHSTOCKICONID siid = (icon == MB_ICONINFORMATION) ? SIID_INFO : ((icon == MB_ICONQUESTION) ? SIID_HELP : SIID_WARNING);
	SHGetStockIconInfo(siid, SHGSI_ICON, &sii); //! not implemented in Wine
	ib.hIcon = sii.hIcon;
#else
	LPCWSTR lpszIcon = (icon == MB_ICONINFORMATION) ? IDI_INFORMATION : ((icon == MB_ICONQUESTION) ? IDI_QUESTION : IDI_EXCLAMATION);
	ib.hIcon = LoadIcon(nullptr, lpszIcon);
#endif

	ib.bDisableCheckBox = StrIsEmpty(szIniFile) || StrIsEmpty(lpstrSetting) || iMode == SuppressMmessage_Never;

	const WORD idDlg = (uType == MB_YESNO) ? IDD_INFOBOX_YESNO : ((uType == MB_OKCANCEL) ? IDD_INFOBOX_OKCANCEL : IDD_INFOBOX_OK);
	HWND hwnd = GetMsgBoxParent();
	MessageBeep(MB_ICONEXCLAMATION);
	const INT_PTR result = ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(idDlg), hwnd, InfoBoxDlgProc, AsInteger<LPARAM>(&ib));
#if 0//_WIN32_WINNT >= _WIN32_WINNT_VISTA
	DestroyIcon(sii.hIcon);
#endif
	return result;
}

/*
HKEY_CLASSES_ROOT\*\shell\Notepad4
	(Default)				REG_SZ		Edit with Notepad4
	icon					REG_SZ		Notepad4.exe
	command
		(Default)			REG_SZ		"Notepad4.exe" "%1"

HKEY_CLASSES_ROOT\Applications\Notepad4.exe
	AppUserModelID			REG_SZ		Notepad4 Text Editor
	FriendlyAppName			REG_SZ		Notepad4 Text Editor
	shell\open\command
		(Default)			REG_SZ		"Notepad4.exe" "%1"

HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe
	(Default)				REG_SZ			Notepad4.exe
	Debugger								REG_SZ		"Notepad4.exe" /z
	UseFilter								REG_DWORD	0
*/
extern bool fIsElevated;
extern TripleBoolean flagUseSystemMRU;
extern WCHAR g_wchAppUserModelID[64];

namespace {

enum {
	SystemIntegration_ContextMenu = 1,
	SystemIntegration_JumpList = 2,
	SystemIntegration_ReplaceNotepad = 4,
	SystemIntegration_RestoreNotepad = 8,
};

struct SystemIntegrationInfo {
	LPWSTR lpszText;
	LPWSTR lpszName;
};

#define NP2RegSubKey_ReplaceNotepad	L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\notepad.exe"
#if defined(_WIN64)
#define samDesired_WOW64_64KEY	0
#else
// uses 64-bit registry on 64-bit Windows prior Windows 7
#define samDesired_WOW64_64KEY	KEY_WOW64_64KEY
#endif

int GetSystemIntegrationStatus(SystemIntegrationInfo &info) noexcept {
	int mask = 0;
	WCHAR tchModule[MAX_PATH];
	GetModuleFileName(nullptr, tchModule, COUNTOF(tchModule));

	// context menu
	HKEY hKey;
	LSTATUS status = RegOpenKeyEx(HKEY_CLASSES_ROOT, NP2RegSubKey_ContextMenu, 0, KEY_READ, &hKey);
	if (status == ERROR_SUCCESS) {
		info.lpszText = Registry_GetDefaultString(hKey);
		HKEY hSubKey;
		status = RegOpenKeyEx(hKey, L"command", 0, KEY_READ, &hSubKey);
		if (status == ERROR_SUCCESS) {
			LPWSTR command = Registry_GetDefaultString(hSubKey);
			if (command != nullptr) {
				if (StrStrI(command, tchModule) != nullptr) {
					mask |= SystemIntegration_ContextMenu;
				}
				NP2HeapFree(command);
			}
			RegCloseKey(hSubKey);
		}
		RegCloseKey(hKey);
	}

	// jump list
	status = RegOpenKeyEx(HKEY_CLASSES_ROOT, NP2RegSubKey_JumpList, 0, KEY_READ, &hKey);
	if (status == ERROR_SUCCESS) {
		info.lpszName = Registry_GetString(hKey, L"FriendlyAppName");
		HKEY hSubKey;
		status = RegOpenKeyEx(hKey, L"shell\\open\\command", 0, KEY_READ, &hSubKey);
		if (status == ERROR_SUCCESS) {
			LPWSTR command = Registry_GetDefaultString(hSubKey);
			if (command != nullptr) {
				LPWSTR userId = Registry_GetString(hKey, L"AppUserModelID");
				if (userId != nullptr && StrEqual(userId, g_wchAppUserModelID) && StrStrI(command, tchModule) != nullptr) {
					mask |= SystemIntegration_JumpList;
				}
				if (userId != nullptr) {
					NP2HeapFree(userId);
				}
				NP2HeapFree(command);
			}
			RegCloseKey(hSubKey);
		}
		RegCloseKey(hKey);
	}

	// replace Windows Notepad
	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NP2RegSubKey_ReplaceNotepad, 0, KEY_QUERY_VALUE | samDesired_WOW64_64KEY, &hKey);
	if (status == ERROR_SUCCESS) {
		LPWSTR command = Registry_GetString(hKey, L"Debugger");
		if (command != nullptr) {
			if (StrStrI(command, tchModule) != nullptr) {
				mask |= SystemIntegration_ReplaceNotepad;
			}
			NP2HeapFree(command);
		}
		RegCloseKey(hKey);
	}

	return mask;
}

void UpdateSystemIntegrationStatus(int mask, LPCWSTR lpszText, LPCWSTR lpszName) noexcept {
	WCHAR tchModule[MAX_PATH];
	GetModuleFileName(nullptr, tchModule, COUNTOF(tchModule));
	WCHAR command[300];
	wsprintf(command, L"\"%s\" \"%%1\"", tchModule);

	// context menu
	// delete the old one: HKEY_CLASSES_ROOT\*\shell\Notepad4.exe
	//Registry_DeleteTree(HKEY_CLASSES_ROOT, NP2RegSubKey_ContextMenu L".exe");
	if (mask & SystemIntegration_ContextMenu) {
		HKEY hSubKey;
		const LSTATUS status = Registry_CreateKey(HKEY_CLASSES_ROOT, NP2RegSubKey_ContextMenu L"\\command", &hSubKey);
		if (status == ERROR_SUCCESS) {
			HKEY hKey;
			RegOpenKeyEx(HKEY_CLASSES_ROOT, NP2RegSubKey_ContextMenu, 0, KEY_WRITE, &hKey);
			Registry_SetDefaultString(hKey, lpszText);
			Registry_SetString(hKey, L"icon", tchModule);
			Registry_SetDefaultString(hSubKey, command);
			RegCloseKey(hKey);
			RegCloseKey(hSubKey);
		}
	} else {
		Registry_DeleteTree(HKEY_CLASSES_ROOT, NP2RegSubKey_ContextMenu);
	}

	// jump list
	if (mask & SystemIntegration_JumpList) {
		HKEY hSubKey;
		const LSTATUS status = Registry_CreateKey(HKEY_CLASSES_ROOT, NP2RegSubKey_JumpList L"\\shell\\open\\command", &hSubKey);
		if (status == ERROR_SUCCESS) {
			HKEY hKey;
			RegOpenKeyEx(HKEY_CLASSES_ROOT, NP2RegSubKey_JumpList, 0, KEY_WRITE, &hKey);
			Registry_SetString(hKey, L"AppUserModelID", g_wchAppUserModelID);
			Registry_SetString(hKey, L"FriendlyAppName", lpszName);
			Registry_SetDefaultString(hSubKey, command);
			RegCloseKey(hKey);
			RegCloseKey(hSubKey);

			if (flagUseSystemMRU != TripleBoolean_True) {
				flagUseSystemMRU = TripleBoolean_True;
				IniSetBoolEx(INI_SECTION_NAME_FLAGS, L"ShellUseSystemMRU", true, true);
			}
		}
	} else {
		Registry_DeleteTree(HKEY_CLASSES_ROOT, NP2RegSubKey_JumpList);
	}

	// replace Windows Notepad
	if (mask & SystemIntegration_ReplaceNotepad) {
		HKEY hKey;
		const LSTATUS status = Registry_CreateKey(HKEY_LOCAL_MACHINE, NP2RegSubKey_ReplaceNotepad, &hKey, samDesired_WOW64_64KEY);
		if (status == ERROR_SUCCESS) {
			wsprintf(command, L"\"%s\" /z", tchModule);
			Registry_SetDefaultString(hKey, tchModule);
			Registry_SetString(hKey, L"Debugger", command);
			Registry_SetInt(hKey, L"UseFilter", 0);
			RegDeleteValue(hKey, L"AppExecutionAliasRedirectPackages");
			RegDeleteValue(hKey, L"AppExecutionAliasRedirect");
#if 0
			WCHAR num[2] = { L'0', L'\0' };
			for (int index = 0; index < 3; index++, num[0]++) {
#if 1
				Registry_DeleteTree(hKey, num);
#else
				HKEY hSubKey;
				status = Registry_CreateKey(hKey, num, &hSubKey);
				if (status == ERROR_SUCCESS) {
					Registry_SetInt(hSubKey, L"AppExecutionAliasRedirect", 1);
					Registry_SetString(hSubKey, L"AppExecutionAliasRedirectPackages", L"*");
					Registry_SetString(hSubKey, L"FilterFullPath", tchModule);
					RegCloseKey(hSubKey);
				}
#endif
			}
#endif
			RegCloseKey(hKey);
		}
	} else if (mask & SystemIntegration_RestoreNotepad) {
#if 0
		Registry_DeleteTree(HKEY_LOCAL_MACHINE, NP2RegSubKey_ReplaceNotepad);
#else
		// on Windows 11, all keys were created by the system, we should not delete them.
		HKEY hKey;
		const LSTATUS status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NP2RegSubKey_ReplaceNotepad, 0, KEY_WRITE | samDesired_WOW64_64KEY, &hKey);
		if (status == ERROR_SUCCESS) {
			RegDeleteValue(hKey, nullptr);
			RegDeleteValue(hKey, L"Debugger");
			Registry_SetInt(hKey, L"UseFilter", 1);
			Registry_SetInt(hKey, L"AppExecutionAliasRedirect", 1);
			Registry_SetString(hKey, L"AppExecutionAliasRedirectPackages", L"*");
#if 0
			GetWindowsDirectory(tchModule, COUNTOF(tchModule));
			LPCWSTR const suffix[] = {
				L"System32\\notepad.exe",
				L"SysWOW64\\notepad.exe",
				L"notepad.exe",
			};
			WCHAR num[2] = { L'0', L'\0' };
			for (int index = 0; index < 3; index++, num[0]++) {
				HKEY hSubKey;
				status = RegOpenKeyEx(hKey, num, 0, KEY_WRITE, &hSubKey);
				if (status == ERROR_SUCCESS) {
					PathCombine(command, tchModule, suffix[index]);
					Registry_SetInt(hSubKey, L"AppExecutionAliasRedirect", 1);
					Registry_SetString(hSubKey, L"AppExecutionAliasRedirectPackages", L"*");
					Registry_SetString(hSubKey, L"FilterFullPath", command);
					RegCloseKey(hSubKey);
				}
			}
#endif
			RegCloseKey(hKey);
		}
#endif
	}
}

INT_PTR CALLBACK SystemIntegrationDlgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) noexcept {
	UNREFERENCED_PARAMETER(lParam);

	switch (umsg) {
	case WM_INITDIALOG: {
		SystemIntegrationInfo info{};
		const int mask = GetSystemIntegrationStatus(info);
		SetWindowLongPtr(hwnd, DWLP_USER, mask);

		HWND hwndCtl = GetDlgItem(hwnd, IDC_CONTEXT_MENU_TEXT);
		if (StrIsEmpty(info.lpszText)) {
			WCHAR wch[128];
			GetString(IDS_LINKDESCRIPTION, wch, COUNTOF(wch));
			Edit_SetText(hwndCtl, wch);
		} else {
			Edit_SetText(hwndCtl, info.lpszText);
		}

		HWND hwndName = GetDlgItem(hwnd, IDC_APPLICATION_NAME);
		Edit_SetText(hwndName, StrIsEmpty(info.lpszName)? g_wchAppUserModelID : info.lpszName);
		if (info.lpszText) {
			NP2HeapFree(info.lpszText);
		}
		if (info.lpszName) {
			NP2HeapFree(info.lpszName);
		}

		if (mask & SystemIntegration_ContextMenu) {
			CheckDlgButton(hwnd, IDC_ENABLE_CONTEXT_MENU, BST_CHECKED);
		}
		if (mask & SystemIntegration_JumpList) {
			CheckDlgButton(hwnd, IDC_ENABLE_JUMP_LIST, BST_CHECKED);
		}
		if (mask & SystemIntegration_ReplaceNotepad) {
			CheckDlgButton(hwnd, IDC_REPLACE_WINDOWS_NOTEPAD, BST_CHECKED);
		}

		if (IsVistaAndAbove() && !fIsElevated) {
			EnableWindow(GetDlgItem(hwnd, IDC_ENABLE_CONTEXT_MENU), FALSE);
			Edit_SetReadOnly(hwndCtl, TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_ENABLE_JUMP_LIST), FALSE);
			Edit_SetReadOnly(hwndName, TRUE);
			EnableWindow(GetDlgItem(hwnd, IDC_REPLACE_WINDOWS_NOTEPAD), FALSE);
		}

		CenterDlgInParent(hwnd);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			if (IsWindowEnabled(GetDlgItem(hwnd, IDC_ENABLE_CONTEXT_MENU))) {
				int mask = 0;
				if (IsButtonChecked(hwnd, IDC_ENABLE_CONTEXT_MENU)) {
					mask |= SystemIntegration_ContextMenu;
				}
				if (IsButtonChecked(hwnd, IDC_ENABLE_JUMP_LIST)) {
					mask |= SystemIntegration_JumpList;
				}
				if (IsButtonChecked(hwnd, IDC_REPLACE_WINDOWS_NOTEPAD)) {
					mask |= SystemIntegration_ReplaceNotepad;
				} else {
					// don't remove third party Notepad replacement.
					const LONG_PTR prev = GetWindowLongPtr(hwnd, DWLP_USER);
					if (prev & SystemIntegration_ReplaceNotepad) {
						mask |= SystemIntegration_RestoreNotepad;
					}
				}

				WCHAR wchText[128];
				GetDlgItemText(hwnd, IDC_CONTEXT_MENU_TEXT, wchText, COUNTOF(wchText));
				TrimString(wchText);

				WCHAR wchName[128];
				GetDlgItemText(hwnd, IDC_APPLICATION_NAME, wchName, COUNTOF(wchName));
				TrimString(wchName);

				UpdateSystemIntegrationStatus(mask, wchText, wchName);
			}
			EndDialog(hwnd, IDOK);
		}
		break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

}

void SystemIntegrationDlg(HWND hwnd) noexcept {
	ThemedDialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SYSTEM_INTEGRATION), hwnd, SystemIntegrationDlgProc, 0);
}
