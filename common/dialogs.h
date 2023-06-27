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

#ifndef COMMON_DIALOG_MANAGER_H
#define COMMON_DIALOG_MANAGER_H

#include "common/scummsys.h"

#if defined(USE_SYSDIALOGS)

#include "common/formats/formatinfo.h"
#include "common/fs.h"
#include "common/str.h"
#include "common/system.h"
#include "common/events.h"

namespace Common {

class SeekableWriteStream;

/**
 * @defgroup common_dialogs Dialog Manager
 * @ingroup common
 *
 * @brief  The Dialog Manager allows GUI code to interact with native system dialogs.
 *
 * @{
 */

/**
 * The DialogManager allows GUI code to interact with native system dialogs.
 */
class DialogManager {
public:
	/**
	 * Values representing the user response to a dialog.
	 */
	enum DialogResult {
		kDialogError = -1,		///< Dialog could not be displayed.
		kDialogCancel = 0,		///< User cancelled the dialog (Cancel/No/Close buttons).
		kDialogOk = 1,			///< User confirmed the dialog (OK/Yes buttons).
		kDialogDeferred = 2,	///< The function succeeded, but the dialog will display later
	};

	DialogManager() : _wasFullscreen(false) {}
	virtual ~DialogManager() {}

	/**
	 * Display a dialog for selecting a file or folder in the file system.
	 *
	 * @param title        Dialog title.
	 * @param choice       Path selected by the user.
	 * @param isDirBrowser Restrict selection to directories.
	 * @return The dialog result.
	 */
	virtual DialogResult showFileBrowser(const Common::U32String &title, Common::FSNode &choice, bool isDirBrowser = false) { return kDialogError; }

	/**
	 * Display a dialog for saving or sharing a file and outputs a write stream to an empty file at the selected location on success.
	 *
	 * If this returns kDialogDeferred, then the dialog was not displayed and will display when the output write stream is closed instead.
	 *
	 * If a backend supports this, then its OSystem::hasFeature override should return "true" for kFeatureSystemSaveFileDialog.
	 *
	 * @param title               Dialog title.
	 * @param defaultName         The default name of the file.
	 * @param fileTypeDescription A string containing a human-readable description of the file type being saved.
	 * @param preferredExtension  The preferred file extension of the file.
	 * @param fileFormat          The file format ID of the supplied file.  @see Common::FormatInfo::FormatID
	 * @param outWriteStream      A write stream to the created or overwritten file.
	 * @return                    The dialog result.
	 */
	virtual DialogResult showFileSaveBrowser(const Common::U32String &title, const Common::U32String &defaultName, const Common::U32String &fileTypeDescription, const Common::U32String &preferredExtension, Common::FormatInfo::FormatID fileFormat, SeekableWriteStream *&outWriteStream) { return kDialogError; }

	/**
	 * Returns the support level of a file format. 
	 *
	 * @param fileFormat  The file format to check the support level of.
	 * @return The support level of the format.
	 */
	virtual Common::FormatInfo::FormatSupportLevel getSaveFormatSupportLevel(Common::FormatInfo::FormatID fileFormat) const { return Common::FormatInfo::kFormatSupportLevelNone; }

protected:
	bool _wasFullscreen;

	/**
	 * Call before opening a dialog.
	 */
	void beginDialog() {
		// If we were in fullscreen mode, switch back
		_wasFullscreen = g_system->getFeatureState(OSystem::kFeatureFullscreenMode);
		if (_wasFullscreen) {
			g_system->beginGFXTransaction();
			g_system->setFeatureState(OSystem::kFeatureFullscreenMode, false);
			g_system->endGFXTransaction();
		}
	}

	/**
	 * Call after closing a dialog.
	 */
	void endDialog() {
		// While the native file browser is open, any input events (e.g. keypresses) are
		// still received by the application. With SDL backend for example this results in the
		// events beeing queued and processed after we return, thus dispatching events that were
		// intended for the native file browser. For example: pressing Esc to cancel the native
		// file browser would cause the application to quit in addition to closing the
		// file browser. To avoid this happening clear all pending events.
		g_system->getEventManager()->getEventDispatcher()->clearEvents();

		// If we were in fullscreen mode, switch back
		if (_wasFullscreen) {
			g_system->beginGFXTransaction();
			g_system->setFeatureState(OSystem::kFeatureFullscreenMode, true);
			g_system->endGFXTransaction();
		}
	}
};

/** @} */

} // End of namespace Common

#endif

#endif // COMMON_DIALOG_MANAGER_H
