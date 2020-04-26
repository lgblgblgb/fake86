/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers
            (C)2020      Gabor Lenart "LGB"

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "../config.h"
#include "rc/resource.h"
#include <windows.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "win32/menus.h"

#include "render.h"
#include "parsecl.h"
#include "disk.h"
#include "cpu.h"
#include "main.h"

HWND myWindow;
HINSTANCE myInstance;
HMENU myMenu;
WNDPROC oldProc;
HWND GetHwnd(void);
HICON myIcon;
void SetWndProc(void);
void MenuItemClick(WPARAM wParam);

void ShowMenu(void) {
	SetMenu(myWindow, myMenu);
}

void HideMenu(void) {
	SetMenu(myWindow, NULL);
}

void initmenus(void) {
	myWindow = GetHwnd();
	myInstance = GetModuleHandle(NULL);
	myMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU1));
	ShowMenu();
	SetWndProc();
	//myIcon = LoadIcon(myInstance, MAKEINTRESOURCE(IDI_ICON1));
	//SetClassLong(myWindow, GCLP_HICON, (LONG)(uint64_t)myIcon);
	//SetClassLong(myWindow, GCLP_HICONSM, (LONG)(uint64_t)myIcon);

	return;
}

//extern SDL_Window *sdl_win;

HWND GetHwnd(void) {
#if 0
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);

	if (!SDL_GetWMInfo(&wmi)) return(NULL);
	return(wmi.window);
#endif
	SDL_SysWMinfo systemInfo;
	SDL_VERSION(&systemInfo.version);
	SDL_GetWindowWMInfo(sdl_win, &systemInfo);
	return systemInfo.info.win.window;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			MenuItemClick(wParam);
			return(TRUE);
	}

	return(CallWindowProc(oldProc, hwnd, msg, wParam, lParam));
}

void SetWndProc(void) {
#ifdef _WIN64
	oldProc = (WNDPROC)SetWindowLongPtr(myWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
#else
	oldProc = (WNDPROC)SetWindowLong(myWindow, GWL_WNDPROC, (LONG_PTR)WndProc);
#endif
}

//extern uint8_t running, bootdrive, dohardreset, scrmodechange;
//extern uint16_t constantw, constanth;
//uint8_t insertdisk (uint8_t drivenum, char *filename);
//uint8_t ejectdisk (uint8_t drivenum);

void MenuItemClick(WPARAM wParam) {
	OPENFILENAME of_dlg;
	uint8_t filename[MAX_PATH] = { 0 };

	switch (LOWORD(wParam)) {
		//file menu
		case ID_FILE_EXIT:
			running = 0;
			break;

		//emulation menu
		case ID_EMULATION_HARDRESETEMULATOR:
			dohardreset = 1;
			break;

		case ID_FLOPPY0_INSERTDISK:
		case ID_FLOPPY1_INSERTDISK:
			memset(&of_dlg, 0, sizeof(of_dlg));
			of_dlg.lStructSize = sizeof(of_dlg);
			of_dlg.lpstrTitle = "Open disk image";
			of_dlg.hInstance = NULL;
			of_dlg.lpstrFile = (LPSTR)filename;
			of_dlg.lpstrFilter = "Floppy disk images (*.img)\0*.img\0All files (*.*)\0*.*\0\0";
			of_dlg.nMaxFile = MAX_PATH;
			of_dlg.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES;
			if (GetOpenFileName(&of_dlg)) {
				if (LOWORD(wParam) == ID_FLOPPY0_INSERTDISK) {
					insertdisk(0, (char *)of_dlg.lpstrFile);
					if (bootdrive == 255) bootdrive = 0;
				} else insertdisk(1, (char *)of_dlg.lpstrFile);
			}
			break;
		case ID_FLOPPY0_EJECTDISK:
			ejectdisk(0);
			break;
		case ID_FLOPPY1_EJECTDISK:
			ejectdisk(1);
			break;

		case ID_HARDDRIVE0_INSERTDISK:
		case ID_HARDDRIVE1_INSERTDISK:
			memset(&of_dlg, 0, sizeof(of_dlg));
			of_dlg.lStructSize = sizeof(of_dlg);
			of_dlg.lpstrTitle = "Open disk image";
			of_dlg.hInstance = NULL;
			of_dlg.lpstrFile = (LPSTR)filename;
			of_dlg.lpstrFilter = "Raw disk images (*.raw, *.img)\0*.raw;*.img\0All files (*.*)\0*.*\0\0";
			of_dlg.nMaxFile = MAX_PATH;
			of_dlg.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES;
			if (GetOpenFileName(&of_dlg)) {
				if (LOWORD(wParam) == ID_HARDDRIVE0_INSERTDISK) {
					insertdisk(128, (char *)of_dlg.lpstrFile);
					if (bootdrive == 255) bootdrive = 128;
				} else insertdisk(129, (char *)of_dlg.lpstrFile);
			}
			break;
		case ID_HARDDRIVE0_EJECTDISK:
			ejectdisk(128);
			break;
		case ID_HARDDRIVE1_EJECTDISK:
			ejectdisk(129);
			break;

		case ID_SETBOOTDRIVE_FLOPPY0:
			bootdrive = 0;
			break;
		case ID_SETBOOTDRIVE_HARDDRIVE0:
			bootdrive = 128;
			break;

		//video menu
		case ID_WINDOWRESOLUTION_AUTOMATIC:
			constantw = 0;
			constanth = 0;
			scrmodechange = 1;
			break;
		case ID_WINDOWRESOLUTION_320X200:
			constantw = 320;
			constanth = 200;
			scrmodechange = 1;
			break;
		case ID_WINDOWRESOLUTION_640X400:
			constantw = 640;
			constanth = 400;
			scrmodechange = 1;
			break;
		case ID_WINDOWRESOLUTION_960X600:
			constantw = 960;
			constanth = 600;
			scrmodechange = 1;
			break;
		case ID_WINDOWRESOLUTION_1280X800:
			constantw = 1280;
			constanth = 800;
			scrmodechange = 1;
			break;
	}
}
