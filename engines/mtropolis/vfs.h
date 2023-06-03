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

#ifndef MTROPOLIS_VFS_H
#define MTROPOLIS_VFS_H

#include "common/archive.h"

namespace MTropolis {

class IVirtualFileSystem : public Common::Archive {
public:
	virtual ~IVirtualFileSystem();

	virtual void junctionFile(const char *virtualPath, const char *physicalPath) = 0;
	virtual void junctionDir(const char *virtualPath, const char *physicalPath) = 0;
	virtual void junctionFileFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) = 0;
	virtual void junctionDirFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) = 0;
};

/**
 * Creates a virtual file system at the game root.
 *
 * @param pathSeparator         The path separator character for paths specified for this VFS.
 * @param caseSensitive         If true, then file accesses are case-sensitive.
 * @param autoJunctionMacFiles  Automatically junctions .finf and .rsrc files when using junctionFile.
 *
 * @return The virtual file system
 */
IVirtualFileSystem *createVirtualFileSystem(char pathSeparator, bool caseSensitive, bool autoJunctionMacFiles);

} // End of namespace MTropolis

#endif
