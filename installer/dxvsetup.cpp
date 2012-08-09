/***
   * Copyright (c) 2012, Hans-Christian Esperer
   * All rights reserved.
   *  
   * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
   *  
   * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   ***/

#include "stdafx.h"
#include "dxvsetup.h"
#include <stdio.h>
#include <conio.h>
#include <CommCtrl.h>
#include <time.h>
#include <shlobj.h>
#include <objbase.h>

#include "wininet.h"

extern "C" {
#include "sha1.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "bzlib.h"
}

#define MAX_LOADSTRING 100

static const int  SETUP_PORT          = INTERNET_DEFAULT_HTTP_PORT;

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

HINTERNET hInternet        = NULL;

HFONT hFont                = NULL;
HWND hWnd                  = NULL;
HWND lblPAction            = NULL;
HWND lblAction             = NULL;
HWND lblProgress           = NULL;
HWND lblProgressSpinner    = NULL;
HWND lblBytesLeft          = NULL;
HWND txtProtocol           = NULL;
HWND lblFreeSoftware       = NULL;

lua_State* luaState        = NULL;

char *spin[] = {" | ", " / ", " - ", " \\ "};

extern int bspatch(lua_State *L);

INT_PTR CALLBACK TextDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK FileDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK FinishDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

static void CentreWindow(HWND hWnd)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	const int winW = rect.right - rect.left;
	const int winH = rect.bottom - rect.top;
	const int winX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
	const int winY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
	SetWindowPos(hWnd, HWND_TOP, winX, winY, 0, 0, SWP_NOSIZE);
}

static void InitHTTP(void)
{
	hInternet = InternetOpen("HC's dxvsetup",
		INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
}

static void luaWindowsError(lua_State *L, const char* fmtmsg) 
{ 
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

	lua_pushfstring(L, fmtmsg, (char*) lpMsgBuf);
    LocalFree(lpMsgBuf);
}

static int luaFetchHTTP(lua_State* L)
{
	LPCTSTR lpszUserName = NULL;
	LPCTSTR lpszPassword = NULL;
	DWORD dwConnectFlags = 0;
	DWORD dwConnectContext = 0;
	char internalbuf[1024]; /* used only when downloading a file */

	const char* lpszHostName  = lua_tostring(L, 1);
	INTERNET_PORT nServerPort = lua_tointeger(L, 2);
	const char* lpszFinalPath = lua_tostring(L, 3);
	unsigned int maxLength = lua_tointeger(L, 4);


	FILE* outputFile = NULL;
	const char* outFN = NULL;
	if (lua_gettop(L) >= 5) {
		outFN = lua_tostring(L, 5);
		outputFile = fopen(outFN, "wb");
		if (outputFile == NULL) {
			lua_pushfstring(L, "Unable to open output file %s", outFN);
			lua_error(L);
		}
	}

	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE);
	SendMessage(lblProgress, PBM_SETMARQUEE, 1, 30);

	HINTERNET hConnect = InternetConnect(hInternet,
				lpszHostName, nServerPort,
				lpszUserName, lpszPassword,
				INTERNET_SERVICE_HTTP,
				dwConnectFlags, dwConnectContext);

	LPCTSTR lpszVerb = "GET";
	LPCTSTR lpszVersion = NULL;			// Use default.
	LPCTSTR lpszReferrer = NULL;		// No referrer.
	LPCTSTR *lplpszAcceptTypes = NULL;	// Whatever the server wants to give us.
	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
			INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
			INTERNET_FLAG_KEEP_CONNECTION |
			INTERNET_FLAG_NO_AUTH |
			INTERNET_FLAG_NO_AUTO_REDIRECT |
			INTERNET_FLAG_NO_COOKIES |
			INTERNET_FLAG_NO_UI |
			INTERNET_FLAG_RELOAD;
	DWORD dwOpenRequestContext = 0;
	HINTERNET hRequest = HttpOpenRequest(hConnect, lpszVerb, lpszFinalPath, lpszVersion,
			lpszReferrer, lplpszAcceptTypes,
			dwOpenRequestFlags, dwOpenRequestContext);

	if(!HttpSendRequest(hRequest, NULL, 0, NULL, 0)) {
		if (outputFile != NULL) {
			fclose(outputFile);
			DeleteFile(outFN);
		}
		luaWindowsError(L, "Error while downloading file: %s");
		InternetCloseHandle(hConnect);
		lua_error(L);
	}

	DWORD dwInfoLevel = HTTP_QUERY_STATUS_CODE;
	DWORD dwInfoBufferLength = 512;
	BYTE *pInfoBuffer = (BYTE *)malloc(dwInfoBufferLength+1);

	if (!HttpQueryInfo(hRequest, dwInfoLevel, pInfoBuffer, &dwInfoBufferLength, NULL)) {
		if (outputFile != NULL) {
			fclose(outputFile);
			DeleteFile(outFN);
		}
		luaWindowsError(L, "Error while downloading file: %s");
		free(pInfoBuffer);
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		lua_error(L);
	}
	if (strcmp((char*)pInfoBuffer, "200")) {
		if (outputFile != NULL) {
			fclose(outputFile);
			DeleteFile(outFN);
		}
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		lua_pushfstring(L, "Unable to get resource %s (%s)", lpszFinalPath, (char*)pInfoBuffer);
		lua_error(L);
	}

	dwInfoLevel = HTTP_QUERY_CONTENT_LENGTH;
	dwInfoBufferLength = 512;
	if (!HttpQueryInfo(hRequest, dwInfoLevel, pInfoBuffer, &dwInfoBufferLength, NULL)) {
		if (outputFile != NULL) {
			fclose(outputFile);
			DeleteFile(outFN);
		}
		free(pInfoBuffer);
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		lua_pushliteral(L, "Server did not send a correct Content-Length header");
		lua_error(L);
	}

	pInfoBuffer[dwInfoBufferLength] = '\0';
	DWORD contentLength = atoi((const char*) pInfoBuffer);
	//MessageBox(NULL, (char*)pInfoBuffer, "pInfoBuffer", MB_OK | MB_ICONINFORMATION);
	//cprintf("%s", pInfoBuffer);
	free(pInfoBuffer);

	if (contentLength > maxLength) {
		if (outputFile != NULL) {
			fclose(outputFile);
			DeleteFile(outFN);
		}
		lua_pushliteral(L, "Content-Length is bigger than maximum allowed value specified");
		lua_error(L);
	}

	luaL_Buffer buffer;

	char* bufptr;
	if (outputFile == NULL)
		bufptr = luaL_buffinitsize(L, &buffer, contentLength);
	else 
		bufptr = NULL;

	DWORD bytesLeft = contentLength;

	DWORD dwBytesAvailable;
	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);
	SendMessage(lblProgress, PBM_SETPOS, 0, 0);
	long lastProgressPos = 0;
	time_t begintime = time(NULL);
	int invocation = 0;
	while (InternetQueryDataAvailable(hRequest, &dwBytesAvailable, 0, 0)) {
		// BYTE *pMessageBody = (BYTE *)malloc(dwBytesAvailable+1);
	
		if (dwBytesAvailable > bytesLeft) {
			if (outputFile != NULL) {
				fclose(outputFile);
				DeleteFile(outFN);
			}
			lua_pushliteral(L, "Received more data than expected");
			lua_error(L);
		}
		DWORD dwBytesRead;
		BOOL bResult;
		if (bufptr == NULL) {
			if (dwBytesAvailable > 1024) dwBytesAvailable = 1024;
			bResult = InternetReadFile(hRequest, internalbuf,
					dwBytesAvailable, &dwBytesRead);
			if (fwrite(internalbuf, 1, dwBytesRead, outputFile) != dwBytesRead) {
				fclose(outputFile);
				DeleteFile(outFN);
				lua_pushliteral(L, "Unable to write to outputfile");
				lua_error(L);
			}
			bytesLeft -= dwBytesRead;
		} else {
			bResult = InternetReadFile(hRequest, bufptr,
					dwBytesAvailable, &dwBytesRead);
			bufptr += dwBytesRead; bytesLeft -= dwBytesRead;
		}

		if (!bResult) {
			if (outputFile != NULL) {
				fclose(outputFile);
				DeleteFile(outFN);
			}
			lua_pushliteral(L, "Could not read data from the internet. Check if the Aquinas Hub still works!");
			lua_error(L);
		}

		if ((time(NULL) - begintime) > invocation)
			SetWindowText(lblProgressSpinner, spin[(invocation++) % 4]);
	
		if (contentLength != 0) {
			long long progressPos = ((long long)contentLength - (long long)bytesLeft) * 65535 / (long long)contentLength;
			if ((progressPos - lastProgressPos) > 256) {
				SendMessage(lblProgress, PBM_SETPOS, progressPos, 0);
				char bl[128];
				_snprintf(bl, 128, "%d bytes left", bytesLeft);
				SetWindowText(lblBytesLeft, bl);
				lastProgressPos = progressPos;
			}
		}

		if (dwBytesRead == 0)
			break;	// End of File.
	}

	SetWindowText(lblProgressSpinner, "");
	SetWindowText(lblBytesLeft, "");

	SendMessage(lblProgress, PBM_SETPOS, 65535, 0);

	InternetCloseHandle(hConnect);
	InternetCloseHandle(hRequest);

	if (outputFile == NULL) {
		luaL_pushresultsize(&buffer, contentLength);
		return 1;
	} else {
		fclose(outputFile);
		return 0;
	}
}

static int luaGetTempFN(lua_State* L)
{
	const char* prefix = lua_tostring(L, 1);
	char buf[MAX_PATH];
	char tmpfilename[MAX_PATH];
	GetTempPath(MAX_PATH, buf);
	GetTempFileName(buf, prefix, 0, tmpfilename);
	lua_pushstring(L, tmpfilename);
	return 1;
}

static int luaSHA1(lua_State* L)
{
	SHA1Context sha;
	SHA1Reset(&sha);
	size_t inlen;
	const char* inbuf = lua_tolstring(L, 1, &inlen);
	SHA1Input(&sha, (const unsigned char *) inbuf, inlen);
	char outbuf[41];

	if (!SHA1Result(&sha)) {
		lua_pushliteral(L, "SHA1Result failed. Strange!");
		lua_error(L);
		return 0; /* never reached, actually */
	} else {
		_snprintf(outbuf, 41, "%08x%08x%08x%08x%08x",
			 sha.Message_Digest[0], sha.Message_Digest[1],
			 sha.Message_Digest[2],  sha.Message_Digest[3],
			 sha.Message_Digest[4]);
		lua_pushlstring(L, outbuf, 40);
		return 1;
	}
}

static int luaHashFile(lua_State* L)
{
	DWORD bytesleft;
	DWORD readbytes;
	DWORD bytestoread;
	DWORD totalbytes;
	SHA1Context sha;
	char buf[2048];
	char hashbuf[41];

	SHA1Reset(&sha);

	const char* fn = lua_tostring(L, 1);

	SendMessage(lblProgress, PBM_SETPOS, 0, 0);

	HANDLE hFile = CreateFile(fn, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, FALSE);

	if (hFile == INVALID_HANDLE_VALUE) {
		lua_pushliteral(L, "Unable to open file");
		lua_error(L);
	}

	bytesleft = totalbytes = GetFileSize(hFile, NULL);
	
	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);
	while(bytesleft > 0) {
		bytestoread = (bytesleft > 2048) ? 2048 : bytesleft;
		ReadFile(hFile, buf, bytestoread, &readbytes, NULL);
		if (bytestoread != readbytes)
			return false;
		SHA1Input(&sha, (const unsigned char *) buf, readbytes);
		bytesleft -= readbytes;
		long long progressPos = ((long long)totalbytes - (long long)bytesleft) * 65535 / (long long)totalbytes;
		SendMessage(lblProgress, PBM_SETPOS, progressPos, 0);
	}
	SendMessage(lblProgress, PBM_SETPOS, 65535, 0);
	CloseHandle(hFile);

	if (!SHA1Result(&sha)) {
		lua_pushliteral(L, "SHA1Result failed. Strangely enough.");
		lua_error(L);
		return 0; /* never reached */
	} else {
		_snprintf(hashbuf, 41, "%08x%08x%08x%08x%08x",
			sha.Message_Digest[0], sha.Message_Digest[1],
			sha.Message_Digest[2],  sha.Message_Digest[3],
			sha.Message_Digest[4]);
		lua_pushlstring(L, hashbuf, 40);
		return 1;
	}
}

static int luaSleep(lua_State *L)
{
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE);
	SendMessage(lblProgress, PBM_SETMARQUEE, 1, 30);

	Sleep(lua_tointeger(L, 1));

	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);

	return 0;
}

static int luaPatchFile(lua_State *L)
{
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE);
	SendMessage(lblProgress, PBM_SETMARQUEE, 1, 30);

	int res = bspatch(L);

	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);

	return res;
}

static int luaCopyFile(lua_State *L)
{
	const char* srcfn = lua_tostring(L, 1);
	const char* dstfn = lua_tostring(L, 2);

	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE);
	SendMessage(lblProgress, PBM_SETMARQUEE, 1, 30);

	CopyFile(srcfn, dstfn, true);

	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);

	return 0;
}

static int luaCopyDecompress(lua_State *L)
{
	const char* srcfn = lua_tostring(L, 1);
	const char* dstfn = lua_tostring(L, 2);
	char buf[20480];
	char msg[128];

	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE);
	SendMessage(lblProgress, PBM_SETMARQUEE, 1, 30);

	/*
	BZFILE *BZ2_bzReadOpen( int *bzerror, FILE *f, 
                        int verbosity, int small,
                        void *unused, int nUnused );
						*/
	int error;
	FILE* f = fopen(srcfn, "rb");
	if (f == NULL) {
		lua_pushliteral(L, "Unable to open compressed file");
		lua_error(L);
	}
	FILE* fo = fopen(dstfn, "wb");
	if (fo == NULL) {
		fclose(f);
		lua_pushliteral(L, "Unable to open output file");
		lua_error(L);
	}
	BZFILE* compfile = BZ2_bzReadOpen(&error, f, 0, 0, NULL, 0);
	if (error != BZ_OK) {
		fclose(f);
		fclose(fo);
		lua_pushliteral(L, "bzip2 refused to open compressed file");
		lua_error(L);
	}

	DWORD totalbytes = 0;
	while(true) {
		int bytesread = BZ2_bzRead(&error, compfile, buf, 20480);
		if ((error != BZ_STREAM_END) && (error != BZ_OK)) {
			BZ2_bzReadClose(&error, compfile);
			fclose(f);
			fclose(fo);
			SetWindowText(lblBytesLeft, "");
			lua_pushliteral(L, "Error while bzip2 decompressing file");
			lua_error(L);
		}
		if (fwrite(buf, 1, bytesread, fo) != bytesread) {
			BZ2_bzReadClose(&error, compfile);
			fclose(f);
			fclose(fo);
			SetWindowText(lblBytesLeft, "");
			lua_pushliteral(L, "Error while bzip2 decompressing file (cannot write to output file)");
			lua_error(L);
		}
		totalbytes += bytesread;
		_snprintf(msg, 128, "%d bytes decompressed", totalbytes);
		SetWindowText(lblBytesLeft, msg);
		if (error == BZ_STREAM_END)
			break;
	}

	SetWindowText(lblBytesLeft, "");
	fclose(fo);
	BZ2_bzReadClose(&error, compfile);
	fclose(f);

	SendMessage(lblProgress, PBM_SETMARQUEE, 0, 0);
	SetWindowLong(lblProgress, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);

	return 0;
}

static int luaPPrint(lua_State *L)
{
	int numargs = lua_gettop(L);
	for (int i = 0; i < numargs; ++i)
		cprintf("%s%s", i?"\t":"", lua_tostring(L, (i+1)));
	cprintf("\n\r");
	/*
	SendMessage(txtProtocol, EM_SETSEL, -1, -1);
	SendMessage(txtProtocol, EM_REPLACESEL, FALSE, (LPARAM)lua_tostring(L, 1));
	SendMessage(txtProtocol, EM_SETSEL, -1, -1);
	SendMessage(txtProtocol, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
	*/
	SetWindowText(lblPAction, lua_tostring(L, 1));
	return 0;
}

static int luaPrint(lua_State *L)
{
	int numargs = lua_gettop(L);
	for (int i = 0; i < numargs; ++i)
		cprintf("    %s%s", i?"\t":"", lua_tostring(L, (i+1)));
	cprintf("\n\r");
	/*
	SendMessage(txtProtocol, EM_SETSEL, -1, -1);
	SendMessage(txtProtocol, EM_REPLACESEL, FALSE, (LPARAM)lua_tostring(L, 1));
	SendMessage(txtProtocol, EM_SETSEL, -1, -1);
	SendMessage(txtProtocol, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
	*/
	SetWindowText(lblAction, lua_tostring(L, 1));
	return 0;
}

static int luaDeleteDirectory(lua_State *L)
{
	const char* dirname = lua_tostring(L, 1);
	lua_pushboolean(L, RemoveDirectory(dirname));
	return 1;
}

static int luaCreateDirectory(lua_State *L)
{
	const char* dirname = lua_tostring(L, 1);
	lua_pushboolean(L, CreateDirectory(dirname, NULL));
	return 1;
}

DWORD WINAPI InstallerThread(LPVOID Param)
{
	CoInitialize(NULL);
	lua_State *luaState = (lua_State*) Param;
	lua_getglobal(luaState, "doSetup");
	int doSetupRes = lua_pcall(luaState, 0, 0, 0);
	if (doSetupRes != LUA_OK) {
		const char* errormsg;
		switch(doSetupRes) {
		case LUA_ERRRUN:  errormsg = lua_tostring(luaState, 1); break;
		case LUA_ERRMEM:  errormsg = "Out of memory"; break;
		case LUA_ERRERR:  errormsg = "Error that should not have happened!"; break;
		case LUA_ERRGCMM: errormsg = "Error in a garbage collection subroutine"; break;
		default:          errormsg = "Unknown error while running the installation script";
		}
		MessageBox(hWnd, errormsg, "Deus Ex Vetery Setup", MB_OK | MB_ICONEXCLAMATION);
		exit(1);
	}
	HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
	EnableMenuItem(hSysMenu, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
	HMENU hMenu = GetMenu(hWnd);
	EnableMenuItem(hMenu, IDM_EXIT, MF_BYCOMMAND | MF_ENABLED);
	return 0;
}

static int luaShowTextDialog(lua_State *L)
{
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DLGTEXTMESSAGE), hWnd, TextDialog, (LPARAM) L) == IDOK) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	return 1;
}

static int luaShowFileDialog(lua_State *L)
{
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_FILEDIALOG), hWnd, FileDialog, (LPARAM) L) == IDOK) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}

	return 1;
}

static int luaShowFinishDialog(lua_State *L)
{
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INSTALLATIONFINISHED), hWnd, FinishDialog, (LPARAM) L) == IDOK) {
		/* Don't push anything here, as DialogBoxParam has done that for us in this case */
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

static int luaMessageBox(lua_State *L)
{
	const char* msgTitle = lua_tostring(L, 1);
	const char* msgText  = lua_tostring(L, 2);
	const char* msgType  = lua_tostring(L, 3);

	if (!strcmp(msgType, "error")) {
		MessageBox(hWnd, msgText, msgTitle, MB_OK | MB_ICONEXCLAMATION);
		return 0;
	} else if (!strcmp(msgType, "information")) {
		MessageBox(hWnd, msgText, msgTitle, MB_OK | MB_ICONINFORMATION);
		return 0;
	} else if (!strcmp(msgType, "question")) {
		if (MessageBox(hWnd, msgText, msgTitle, MB_YESNO | MB_ICONQUESTION) == IDYES)
			lua_pushboolean(L, 1);
		else
			lua_pushboolean(L, 0);
		return 1;
	}
	
	lua_pushfstring(L, "Unknown MessageBox type %s", msgType);
	lua_error(L);
	return 0; /* not reached */
}

static int luaExists(lua_State *L)
{
	const char* fp = lua_tostring(L, 1);
	DWORD fas = GetFileAttributes(fp);
	if (fas == INVALID_FILE_ATTRIBUTES)
		lua_pushboolean(L, 0);
	else
		lua_pushboolean(L, 1);
	return 1;
}

static int luaIsDir(lua_State *L)
{
	const char* fp = lua_tostring(L, 1);
	DWORD fas = GetFileAttributes(fp);
	if (fas & FILE_ATTRIBUTE_DIRECTORY)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);
	return 1;
}

static int luaGetRegKey(lua_State *L)
{
	const char* rootKey   = lua_tostring(L, 1);
	const char* subKey    = lua_tostring(L, 2);
	const char* valuename = lua_tostring(L, 3);

	HKEY hKey = NULL;
	if (!strcmp(rootKey, "HKEY_CLASSES_ROOT"))
		hKey = HKEY_CLASSES_ROOT;
	else if (!strcmp(rootKey, "HKEY_CURRENT_USER"))
		hKey = HKEY_CURRENT_USER;
	else {
		lua_pushfstring(L, "Unknown registry root %s", rootKey);
		lua_error(L);
	}

	HKEY phkey;
	if (RegOpenKeyEx(hKey, subKey, NULL, KEY_READ, &phkey) != ERROR_SUCCESS) {
		lua_pushfstring(L, "Error opening %s\\%s", rootKey, subKey);
		lua_error(L);
	}
	DWORD valtype;
	DWORD bytes = 1024;
	void* buf = malloc(1024);
	if (buf == NULL) {
		RegCloseKey(phkey);
		lua_pushliteral(L, "malloc failed at luaGetRegKey 1");
		lua_error(L);
	}
	if (RegQueryValueEx(phkey, valuename, NULL, &valtype, (byte*)buf, &bytes) == ERROR_SUCCESS) {
		switch(valtype) {
		case REG_BINARY:
			lua_pushlstring(L, (char*) buf, bytes);
			break;
		case REG_SZ:
			lua_pushlstring(L, (char*) buf, bytes - 1);
			break;
		case REG_DWORD:
			DWORD val;
			memcpy(&val, buf, sizeof(DWORD));
			lua_pushinteger(L, val);
			break;
		default:
			RegCloseKey(phkey);
			lua_pushliteral(L, "Accessed a registry value whose type is not supported");
			lua_error(L);
		}
	} else {
		lua_pushnil(L);
	}
	RegCloseKey(phkey);
	return 1;
}

static int luaGetUserDir(lua_State *L)
{
	const char* dirtype = lua_tostring(L, 1);
	char path[MAX_PATH];
	int csidl;
	if (!strcmp(dirtype, "CSIDL_PROFILE")) {
		csidl = CSIDL_PROFILE;
	} else if (!strcmp(dirtype, "CSIDL_PROGRAMS")) {
		csidl = CSIDL_PROGRAMS;
	} else {
		lua_pushfstring(L, "Unknown CSIDL %s", dirtype);
		lua_error(L);
	}

	SHGetFolderPath(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path);
	lua_pushstring(L, path);
	return 1;
}

static int luaMakeShellLink(lua_State *L)
{
	const char* title    = lua_tostring(L, 1);
	const char* linkpath = lua_tostring(L, 2);
	const char* exepath  = lua_tostring(L, 3);
	const char* runin    = lua_tostring(L, 4);
	IShellLink   *ps;
	IPersistFile *pf;

	if (strlen(linkpath) >= MAX_PATH) {
		lua_pushliteral(L, "linkpath too long for MAX_PATH in luaMakeShellLink");
		lua_error(L);
	}

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*) &ps))) {
		ps->SetDescription(title);
		ps->SetPath(exepath);
		ps->SetWorkingDirectory(runin);
		if (SUCCEEDED(ps->QueryInterface(IID_IPersistFile, (LPVOID*) &pf))) {
			WCHAR wsz[MAX_PATH]; 
            MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wsz, MAX_PATH);
			HRESULT saveRes = pf->Save(wsz, TRUE);
			pf->Release();
			if (SUCCEEDED(saveRes))
				return 0;
		}
	} 
	lua_pushliteral(L, "Error while creating start menu item");
	lua_error(L);
	return 0; /* not reached */
}

static int luaSetWindowTitle(lua_State *L)
{
	const char* newWindowTitle = lua_tostring(L, 1);
	SetWindowText(hWnd, newWindowTitle);
	return 0;
}

static int luaCreateProcess(lua_State *L)
{
	const char* lpApplicationName     = lua_tostring(L, 1);
	const char* lpCommandLine_ro      = lua_tostring(L, 2);	
	const char* lpCurrentDirectory    = lua_tostring(L, 3);	
	char*       lpCommandLine         = (char*) malloc(32768);
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (lpCommandLine == NULL) {
		lua_pushliteral(L, "Out of memory while doing luaCreateProcess!?");
		lua_error(L);
		return 0; /* not reached */
	}

	strncpy(lpCommandLine, lpCommandLine_ro, 32768);
	
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	BOOL res = CreateProcess(lpApplicationName, lpCommandLine, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL, lpCurrentDirectory, &si, &pi);

    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
	free(lpCommandLine);

	if (res) return 0;
	else {
		lua_pushfstring(L, "Unable to launch %s (argv: %s; cwd: %s)",
			lpApplicationName, lpCommandLine_ro, lpCurrentDirectory);
		lua_error(L);
		return 0; /* not reached */
	}
}

static int luaExitSetup(lua_State *L)
{
	int exitCode = lua_tointeger(L, 1);
	/* Can't use PostQuitMessage because we're in a different thread */
	PostMessage(hWnd, WM_QUIT, exitCode, NULL);
	/* I **love** Lua */
	return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
	AllocConsole();
#endif

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;
	
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_DXVSETUP, szWindowClass, MAX_LOADSTRING);
	
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DXVSETUP));

	InternetAttemptConnect(NULL);
	InitHTTP();

	luaState = luaL_newstate();
    luaL_openlibs(luaState);

    lua_register(luaState, "FetchHTTP",			luaFetchHTTP);
	lua_register(luaState, "SHA1",				luaSHA1);
	lua_register(luaState, "HashFile",			luaHashFile);
	lua_register(luaState, "PatchFile",			luaPatchFile);
	lua_register(luaState, "CopyFile",			luaCopyFile);
	lua_register(luaState, "GetTempFN",			luaGetTempFN);
	lua_register(luaState, "CreateDirectory",   luaCreateDirectory);
	lua_register(luaState, "DeleteDirectory",   luaDeleteDirectory);
	lua_register(luaState, "ShowTextDialog",	luaShowTextDialog);
	lua_register(luaState, "ShowFileDialog",	luaShowFileDialog);
	lua_register(luaState, "ShowFinishDialog",	luaShowFinishDialog);
	lua_register(luaState, "MessageBox",		luaMessageBox);
	lua_register(luaState, "Exists",			luaExists);
	lua_register(luaState, "IsDir",				luaIsDir);
	lua_register(luaState, "CopyDecompress",	luaCopyDecompress);
	lua_register(luaState, "GetRegKey",			luaGetRegKey);
	lua_register(luaState, "GetUserDir",		luaGetUserDir);
	lua_register(luaState, "MakeShellLink",		luaMakeShellLink);
	lua_register(luaState, "SetWindowTitle",	luaSetWindowTitle);
	lua_register(luaState, "ExitSetup",			luaExitSetup);
	lua_register(luaState, "CreateProcess",		luaCreateProcess);
	lua_register(luaState, "sleep",				luaSleep);
	lua_register(luaState, "pprint",			luaPPrint);
	lua_register(luaState, "print",				luaPrint);

	HRSRC setupScriptRes   = FindResource(hInstance, MAKEINTRESOURCE(IDR_SETUPSCRIPT), "SCRIPTS");
	HGLOBAL setupScriptGlo = LoadResource(hInstance, setupScriptRes);
	DWORD setupScriptLen = SizeofResource(hInstance, setupScriptRes);
	LPCSTR string = (LPCSTR) LockResource(setupScriptGlo);
	int runRes = (luaL_loadbuffer(luaState, string, setupScriptLen, "dxvsetup.lua")) ||
		(lua_pcall(luaState, 0, LUA_MULTRET, 0));
	if (runRes != 0) {
		MessageBox(hWnd, "Error while loading the setup script!", "Deus Ex Vetery Setup", MB_OK | MB_ICONINFORMATION);
		exit(1);
	}

	DWORD threadID = 0;
	CreateThread(
		NULL,
		0,
		InstallerThread,
		luaState,
		0,
		&threadID);

	//PatchFile(\\System\\DeusEx.u", NULL, "vi/dxv");

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DXVSETUP));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_DXVSETUP);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_DXVSETUP));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	const int winW = 480;
	const int winH = 200;
	const int winX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
	const int winY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

	hFont = CreateFont(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times New Roman"));

	hWnd = CreateWindow(szWindowClass, szTitle, WS_BORDER | WS_CAPTION | WS_SYSMENU,
		winX, winY, winW, winH, NULL, NULL, hInstance, NULL);

	HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
	DeleteMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
	DeleteMenu(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
	DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
	DeleteMenu(hSysMenu, SC_SIZE, MF_BYCOMMAND);
	/* The separator */
	DeleteMenu(hSysMenu, 1, MF_BYPOSITION);
	EnableMenuItem(hSysMenu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED);

	if (!hWnd)
		return FALSE;

	ShowWindow(hWnd, nCmdShow);

	RECT cRect;
	GetClientRect(hWnd, &cRect);
	const int cliW = cRect.right - cRect.left;
	const int cliH = cRect.bottom - cRect.top;

	INITCOMMONCONTROLSEX InitCtrlEx;

	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&InitCtrlEx);


	lblPAction         = CreateWindow(WC_STATIC, "Loading dxvsetup.lua...", WS_CHILD | WS_VISIBLE, (cliW - 420)/2, 42-20, 420, 16, hWnd, NULL, hInstance, 0);
	lblAction          = CreateWindow(WC_STATIC, "Please stand by and wait.", WS_CHILD | WS_VISIBLE, (cliW - 420)/2, 42, 420, 16, hWnd, NULL, hInstance, 0);
	lblProgress        = CreateWindow(PROGRESS_CLASS, "Download progress bar", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, (cliW - 420)/2, 64, 420, 16, hWnd, NULL, hInstance, 0);
	lblProgressSpinner = CreateWindow(WC_STATIC, "   ", WS_CHILD | WS_VISIBLE, 8, 64, 16, 16, hWnd, NULL, hInstance, 0);
	lblBytesLeft       = CreateWindow(WC_STATIC, "", WS_CHILD | WS_VISIBLE, (cliW-420)/2, 84, 420, 16, hWnd, NULL, hInstance, 0);
	lblFreeSoftware    = CreateWindow(WC_STATIC, "This installer is free software.", WS_CHILD | WS_VISIBLE, (cliW-420)/2, cliH-20, 420, 16, hWnd, NULL, hInstance, 0);

	if (hFont != NULL) {
		SendMessage(lblPAction, WM_SETFONT, WPARAM(hFont), TRUE);
		SendMessage(lblAction, WM_SETFONT, WPARAM(hFont), TRUE);
		SendMessage(lblBytesLeft, WM_SETFONT, WPARAM(hFont), TRUE);
		SendMessage(lblFreeSoftware, WM_SETFONT, WPARAM(hFont), TRUE);
	}

	SendMessage(lblProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 65535));

	/*
		txtProtocol    = CreateWindow("EDIT", "hi", WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
			ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 42, 84, 420, 420, hWnd, NULL, hInstance, 0);
	*/
	

	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	static HBRUSH hbrBkgnd = NULL;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CTLCOLORSTATIC:
		{
		HDC hdcStatic = (HDC) wParam;
        SetBkColor(hdcStatic, RGB(255,255,255));
        if (hbrBkgnd == NULL)
            hbrBkgnd = CreateSolidBrush(RGB(255,255,255));
        return (INT_PTR)hbrBkgnd;
		}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for text dialog
INT_PTR CALLBACK TextDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
		lua_State *L = (lua_State*) lParam;
		HWND hWndText = GetDlgItem(hDlg, IDC_TEXT);
		SetWindowText(hDlg, lua_tostring(L, 1));
		SetWindowText(hWndText, lua_tostring(L, 2));
		CentreWindow(hDlg);
		return (INT_PTR)TRUE;
		}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for file dialog
INT_PTR CALLBACK FileDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static lua_State *L = NULL; /* ugly; lParam is only set to it during WM_INITDIALOG */
	char strbuf[MAX_PATH];

	switch (message)
	{
	case WM_INITDIALOG:
		L = (lua_State*) lParam;
		SetWindowText(hDlg, lua_tostring(L, 1));
		SetWindowText(GetDlgItem(hDlg, IDC_MESSAGE), lua_tostring(L, 2));
		lua_getglobal(L, "InstallSrc");
		SetWindowText(GetDlgItem(hDlg, IDC_ORIGDXPATH), lua_tostring(L, -1));
		lua_pop(L, 1);
		//lua_getglobal(L, "InstallDst");
		//SetWindowText(GetDlgItem(hDlg, IDC_DSTPATH), lua_tostring(L, -1));
		//lua_pop(L, 1);
		CentreWindow(hDlg);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			if (LOWORD(wParam) == IDOK) {
				GetWindowText(GetDlgItem(hDlg, IDC_ORIGDXPATH), strbuf, MAX_PATH);
				lua_pushstring(L, strbuf);
				lua_setglobal(L, "InstallSrc");
				//GetWindowText(GetDlgItem(hDlg, IDC_DSTPATH), strbuf, MAX_PATH);
				//lua_pushstring(L, strbuf);
				//lua_setglobal(L, "InstallDst");
			}
			EndDialog(hDlg, LOWORD(wParam));
			L = NULL;
			return (INT_PTR)TRUE;
		} else if (LOWORD(wParam) == IDC_BROWSE) {
			BROWSEINFO bi;
			ZeroMemory(&bi, sizeof(BROWSEINFO));
			bi.hwndOwner = hDlg;
			bi.lpszTitle = "Browse for Deus Ex Original";
			bi.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
			PIDLIST_ABSOLUTE pla = SHBrowseForFolder(&bi);
			if (pla != NULL) {
				char buf[MAX_PATH];
				if (SHGetPathFromIDList(pla, buf))
					SetWindowText(GetDlgItem(hDlg, IDC_ORIGDXPATH), buf);
				CoTaskMemFree(pla);
			}
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for finish dialog
INT_PTR CALLBACK FinishDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static lua_State *L = NULL; /* ugly; lParam is only set to it during WM_INITDIALOG */

	switch (message)
	{
	case WM_INITDIALOG:
		L = (lua_State*) lParam;
		SetWindowText(hDlg, lua_tostring(L, 1));
		SetWindowText(GetDlgItem(hDlg, IDC_MESSAGE), lua_tostring(L, 2));
		SetWindowText(GetDlgItem(hDlg, IDC_CHKPLAYNOW), lua_tostring(L, 3));
		CheckDlgButton(hDlg, IDC_CHKPLAYNOW, lua_toboolean(L, 4));
		CentreWindow(hDlg);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK) {
				lua_pushboolean(L, IsDlgButtonChecked(hDlg, IDC_CHKPLAYNOW));
			}
			EndDialog(hDlg, LOWORD(wParam));
			L = NULL;
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
