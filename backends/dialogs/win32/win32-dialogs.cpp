/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

 // We cannot use common/scummsys.h directly as it will include
 // windows.h and we need to do it by hand to allow excluded functions
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(WIN32) && defined(USE_SYSDIALOGS)

// HACK: To get __MINGW64_VERSION_foo defines we need to manually include
// _mingw.h in this file because we do not include any system headers at this
// point on purpose. The defines are required to detect whether this is a
// classic MinGW toolchain or a MinGW-w64 based one.
#if defined(__MINGW32__)
#include <_mingw.h>
#endif

// Needed for dialog functions
// HACK: MinGW-w64 based toolchains include the symbols we require in their
// headers. The 32 bit incarnation only defines __MINGW32__. This leads to
// build breakage due to clashes with our compat header. Luckily MinGW-w64
// based toolchains define __MINGW64_VERSION_foo macros inside _mingw.h,
// which is included from all system headers. Thus we abuse that to detect
// them.
#if defined(__GNUC__) && defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#include "backends/dialogs/win32/mingw-compat.h"
#else
	// We use functionality introduced with Vista in this file.
	// To assure that including the respective system headers gives us all
	// required definitions we set Vista as minimum version we target.
	// See: https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers#macros-for-conditional-declarations
#include <sdkddkver.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <shlobj.h>

#include "common/scummsys.h"

#include "backends/dialogs/win32/win32-dialogs.h"
#include "backends/platform/sdl/win32/win32_wrapper.h"
#include "backends/platform/sdl/win32/win32-window.h"

#include "common/config-manager.h"
#include "common/translation.h"

Win32DialogManager::Win32DialogManager(SdlWindow_Win32 *window) : _window(window) {
	CoInitialize(nullptr);
}

Win32DialogManager::~Win32DialogManager() {
	CoUninitialize();
}

// Wrapper for old Windows versions
HRESULT winCreateItemFromParsingName(PCWSTR pszPath, IBindCtx *pbc, REFIID riid, void **ppv) {
	typedef HRESULT(WINAPI *SHFunc)(PCWSTR, IBindCtx *, REFIID, void **);

	SHFunc func = (SHFunc)(void *)GetProcAddress(GetModuleHandle(TEXT("shell32.dll")), "SHCreateItemFromParsingName");
	if (func == nullptr)
		return E_NOTIMPL;

	return func(pszPath, pbc, riid, ppv);
}

HRESULT getShellPath(IShellItem *item, Common::String &path) {
	LPWSTR name = nullptr;
	HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &name);
	if (SUCCEEDED(hr)) {
		char *str = Win32::unicodeToAnsi(name);
		path = Common::String(str);
		CoTaskMemFree(name);
		free(str);
	}
	return hr;
}

Common::DialogManager::DialogResult Win32DialogManager::showFileBrowser(const Common::U32String &title, Common::FSNode &choice, bool isDirBrowser) {
	DialogResult result = kDialogError;

	// Do nothing if not running on Windows Vista or later
	if (!Win32::confirmWindowsVersion(6, 0))
		return result;

	IFileOpenDialog *dialog = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IFileOpenDialog,
		reinterpret_cast<void **> (&(dialog)));

	if (SUCCEEDED(hr)) {
		beginDialog();

		// Customize dialog
		bool showHidden = ConfMan.getBool("gui_browser_show_hidden", Common::ConfigManager::kApplicationDomain);

		DWORD dwOptions;
		hr = dialog->GetOptions(&dwOptions);
		if (SUCCEEDED(hr)) {
			if (isDirBrowser)
				dwOptions |= FOS_PICKFOLDERS;
			if (showHidden)
				dwOptions |= FOS_FORCESHOWHIDDEN;
			hr = dialog->SetOptions(dwOptions);
		}

		LPWSTR dialogTitle = (LPWSTR)title.encodeUTF16Native();
		hr = dialog->SetTitle(dialogTitle);
		free(dialogTitle);

		LPWSTR okTitle = (LPWSTR)_("Choose").encodeUTF16Native();
		hr = dialog->SetOkButtonLabel(okTitle);
		free(okTitle);

		LPWSTR str;
		if (ConfMan.hasKey("browser_lastpath")) {
			str = Win32::ansiToUnicode(ConfMan.get("browser_lastpath").c_str());
			IShellItem *item = nullptr;
			hr = winCreateItemFromParsingName(str, nullptr, IID_IShellItem, reinterpret_cast<void **> (&(item)));
			if (SUCCEEDED(hr)) {
				hr = dialog->SetDefaultFolder(item);
			}
			free(str);
		}

		// Show dialog
		hr = dialog->Show(_window->getHwnd());

		if (SUCCEEDED(hr)) {
			// Get the selection from the user
			IShellItem *selectedItem = nullptr;
			hr = dialog->GetResult(&selectedItem);
			if (SUCCEEDED(hr)) {
				Common::String path;
				hr = getShellPath(selectedItem, path);
				if (SUCCEEDED(hr)) {
					choice = Common::FSNode(path);
					result = kDialogOk;
				}
				selectedItem->Release();
			}

			// Save last path
			IShellItem *lastFolder = nullptr;
			hr = dialog->GetFolder(&lastFolder);
			if (SUCCEEDED(hr)) {
				Common::String path;
				hr = getShellPath(lastFolder, path);
				if (SUCCEEDED(hr)) {
					ConfMan.set("browser_lastpath", path);
				}
				lastFolder->Release();
			}
		}
		else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			result = kDialogCancel;
		}

		dialog->Release();

		endDialog();
	}

	return result;
}

Common::DialogManager::DialogResult Win32DialogManager::showFileSaveBrowser(const Common::U32String &title, const Common::U32String &defaultName, const Common::U32String &fileTypeDescription, const Common::U32String &preferredExtension, Common::FormatInfo::FormatID format, Common::SeekableWriteStream *&outWriteStream) {
	DialogResult result = kDialogError;
	outWriteStream = nullptr;

	// Do nothing if not running on Windows Vista or later
	if (!Win32::confirmWindowsVersion(6, 0))
		return result;

	IFileSaveDialog *dialog = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog,
								  nullptr,
								  CLSCTX_INPROC_SERVER,
								  IID_IFileSaveDialog,
								  reinterpret_cast<void **>(&(dialog)));

	if (SUCCEEDED(hr)) {
		beginDialog();

		// Customize dialog
		bool showHidden = ConfMan.getBool("gui_browser_show_hidden", Common::ConfigManager::kApplicationDomain);

		DWORD dwOptions;
		hr = dialog->GetOptions(&dwOptions);
		if (SUCCEEDED(hr)) {
			if (showHidden)
				dwOptions |= FOS_FORCESHOWHIDDEN;
			hr = dialog->SetOptions(dwOptions);
		}

		if (preferredExtension.size() > 0) {
			Common::U32String filterDesc = fileTypeDescription + Common::String(" (*.").decode() + preferredExtension + Common::String(")").decode();
			LPWSTR filterDescU16 = reinterpret_cast<LPWSTR>(filterDesc.encodeUTF16Native());

			Common::U32String spec = Common::String("*.").decode() + preferredExtension;
			LPWSTR specU16 = reinterpret_cast<LPWSTR>(spec.encodeUTF16Native());

			COMDLG_FILTERSPEC filterSpec;
			filterSpec.pszSpec = specU16;
			filterSpec.pszName = filterDescU16;

			dialog->SetFileTypes(1, &filterSpec);

			free(specU16);
			free(filterDescU16);
		}

		LPWSTR defaultNameU16 = reinterpret_cast<LPWSTR>(defaultName.encodeUTF16Native());
		dialog->SetFileName(defaultNameU16);
		free(defaultNameU16);


		LPWSTR dialogTitle = (LPWSTR)title.encodeUTF16Native();
		hr = dialog->SetTitle(dialogTitle);
		free(dialogTitle);

		LPWSTR okTitle = (LPWSTR)_("Save").encodeUTF16Native();
		hr = dialog->SetOkButtonLabel(okTitle);
		free(okTitle);

		LPWSTR str;
		if (ConfMan.hasKey("browser_lastpath")) {
			str = Win32::ansiToUnicode(ConfMan.get("browser_lastpath").c_str());
			IShellItem *item = nullptr;
			hr = winCreateItemFromParsingName(str, nullptr, IID_IShellItem, reinterpret_cast<void **>(&(item)));
			if (SUCCEEDED(hr)) {
				hr = dialog->SetDefaultFolder(item);
			}
			free(str);
		}

		// Show dialog
		hr = dialog->Show(_window->getHwnd());

		if (SUCCEEDED(hr)) {
			// Get the selection from the user
			IShellItem *selectedItem = nullptr;
			hr = dialog->GetResult(&selectedItem);
			if (SUCCEEDED(hr)) {
				Common::String path;
				hr = getShellPath(selectedItem, path);
				if (SUCCEEDED(hr)) {
					Common::SeekableWriteStream *writeStream = Common::FSNode(path).createWriteStream();
					outWriteStream = writeStream;
					result = writeStream ? kDialogOk : kDialogError;
				}
				selectedItem->Release();
			}

			// Save last path
			IShellItem *lastFolder = nullptr;
			hr = dialog->GetFolder(&lastFolder);
			if (SUCCEEDED(hr)) {
				Common::String path;
				hr = getShellPath(lastFolder, path);
				if (SUCCEEDED(hr)) {
					ConfMan.set("browser_lastpath", path);
				}
				lastFolder->Release();
			}
		} else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			result = kDialogCancel;
		}

		dialog->Release();

		endDialog();
	}

	return result;
}

Common::FormatInfo::FormatSupportLevel Win32DialogManager::getSaveFormatSupportLevel(Common::FormatInfo::FormatID fileFormat) const {
	switch (fileFormat) {
	case Common::FormatInfo::kBMP:
		return Common::FormatInfo::kFormatSupportLevelSupported;
	case Common::FormatInfo::kPNG:
	case Common::FormatInfo::kJPEG:
		return Common::FormatInfo::kFormatSupportLevelPreferred;
	default:
		return Common::FormatInfo::getDefaultFormatSupportLevel(fileFormat);
	}
}

#endif
