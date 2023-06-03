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

#include "common/archive.h"
#include "common/debug.h"
#include "common/hashmap.h"

#include "mtropolis/vfs.h"


namespace MTropolis {


IVirtualFileSystem::~IVirtualFileSystem() {
}

class VirtualFileSystem : public IVirtualFileSystem {
public:
	VirtualFileSystem(char pathSeparator, bool caseSensitive, bool autoJunctionMacFiles);

	void junctionFile(const char *virtualPath, const char *physicalPath) override;
	void junctionDir(const char *virtualPath, const char *physicalPath) override;
	void junctionFileFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) override;
	void junctionDirFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) override;

	bool hasFile(const Common::Path &path) const override;
	int listMembers(Common::ArchiveMemberList &list) const override;
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override;
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override;

private:
	struct Junction {
		Junction();

		Common::Archive *_archive;
		Common::Path _virtualPath;
		Common::Path _physicalPath;
		bool _isDirectory;
	};

	void internalJunction(Common::Archive *archive, const char *virtualPath, const char *physicalPath, bool isDirectory);
	const Junction *findJunction(const Common::Path &path) const;
	static Common::Path remapPath(const Common::Path &sourcePath, const Common::Path &virtualDir, const Common::Path &physicalDir);

	Common::Array<Junction> _junctions;

	char _pathSeparator;
	bool _caseSensitive;
	bool _autoJunctionMacFiles;
};

VirtualFileSystem::Junction::Junction() : _archive(nullptr) {
}

VirtualFileSystem::VirtualFileSystem(char pathSeparator, bool caseSensitive, bool autoJunctionMacFiles)
	: _pathSeparator(pathSeparator), _caseSensitive(caseSensitive), _autoJunctionMacFiles(autoJunctionMacFiles) {
}

void VirtualFileSystem::junctionFile(const char *virtualPath, const char *physicalPath) {
	junctionFileFromArchive(&SearchMan, virtualPath, physicalPath);
}

void VirtualFileSystem::junctionDir(const char *virtualPath, const char *physicalPath) {
	junctionDirFromArchive(&SearchMan, virtualPath, physicalPath);
}

void VirtualFileSystem::junctionFileFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) {

	internalJunction(archive, virtualPath, physicalPath, false);

	if (_autoJunctionMacFiles) {
		internalJunction(archive, (Common::String(virtualPath) + ".finf").c_str(), (Common::String(physicalPath) + ".finf").c_str(), false);
		internalJunction(archive, (Common::String(virtualPath) + ".rsrc").c_str(), (Common::String(physicalPath) + ".rsrc").c_str(), false);
	}
}

void VirtualFileSystem::junctionDirFromArchive(Common::Archive *archive, const char *virtualPath, const char *physicalPath) {
	internalJunction(archive, virtualPath, physicalPath, true);
}

void VirtualFileSystem::internalJunction(Common::Archive *archive, const char *virtualPath, const char *physicalPath, bool isDirectory) {
	Junction junction;
	junction._archive = archive;
	junction._isDirectory = isDirectory;
	junction._physicalPath = physicalPath;
	junction._virtualPath = virtualPath;

	if (!_caseSensitive) {
		Common::StringArray components = junction._virtualPath.splitComponents();

		for (Common::String &str : components)
			str.toLowercase();

		junction._virtualPath.joinComponents(components);
	}

	_junctions.push_back(Common::move(junction));
}

const VirtualFileSystem::Junction *VirtualFileSystem::findJunction(const Common::Path &path) const {
	Common::StringArray pathComponents = path.splitComponents();

	if (!_caseSensitive) {
		for (Common::String &str : pathComponents)
			str.toLowercase();
	}

	const Junction *bestJunction = nullptr;
	uint longestJunctionLength = 0;

	for (const Junction &junction : _junctions) {
		Common::StringArray virtualPathComponents = junction._virtualPath.splitComponents();

		if (bestJunction != nullptr && virtualPathComponents.size() <= longestJunctionLength)
			continue;

		if (junction._isDirectory) {
			// Directory junctions must be longer than the path
			if (virtualPathComponents.size() >= pathComponents.size())
				continue;
		} else {
			// File junctions must be exactly the requested length
			if (virtualPathComponents.size() != pathComponents.size())
				continue;
		}

		bool failed = false;
		for (uint i = 0; i < virtualPathComponents.size(); i++) {
			if (virtualPathComponents[i] != pathComponents[i]) {
				failed = true;
				break;
			}
		}

		if (!failed)
			continue;

		longestJunctionLength = virtualPathComponents.size();
		bestJunction = &junction;
	}

	return bestJunction;
}

Common::Path VirtualFileSystem::remapPath(const Common::Path &sourcePath, const Common::Path &virtualDir, const Common::Path &physicalDir) {
	Common::StringArray sourcePathComponents = sourcePath.splitComponents();
	Common::StringArray virtualDirComponents = virtualDir.splitComponents();
	Common::StringArray physicalPathComponents = physicalDir.splitComponents();

	uint numVirtualDirComponents = virtualDirComponents.size();
	assert(sourcePathComponents.size() > numVirtualDirComponents);

	for (uint i = numVirtualDirComponents; i < sourcePathComponents.size(); i++)
		physicalPathComponents.push_back(sourcePathComponents[i]);

	Common::Path result;
	result.joinComponents(physicalPathComponents);

	return result;
}

bool VirtualFileSystem::hasFile(const Common::Path &path) const {
	const Junction *junction = findJunction(path);

	if (!junction)
		return false;

	if (junction->_isDirectory)
		return junction->_archive->hasFile(remapPath(path, junction->_virtualPath, junction->_physicalPath));
	else
		return junction->_archive->hasFile(junction->_physicalPath);
}

int VirtualFileSystem::listMembers(Common::ArchiveMemberList &list) const {
	int numMembers = 0;

	for (const Junction &junction : _junctions) {
		if (junction._isDirectory) {
			Common::ArchiveMemberList ptrs;

			Common::Path pattern = junction._physicalPath;
			pattern.appendComponent("*");

			junction._archive->listMatchingMembers(ptrs, pattern, true);

			for (const Common::ArchiveMemberPtr &ptr : ptrs) {
				numMembers++;
				list.push_back(ptr);
			}
		} else {
			Common::ArchiveMemberPtr ptr = junction._archive->getMember(junction._physicalPath);
			if (ptr) {
				list.push_back(ptr);
				numMembers++;
			}
		}
	}

	return numMembers;
}

const Common::ArchiveMemberPtr VirtualFileSystem::getMember(const Common::Path &path) const {
	const Junction *junction = findJunction(path);

	if (!junction)
		return nullptr;

	if (junction->_isDirectory)
		return junction->_archive->getMember(remapPath(path, junction->_virtualPath, junction->_physicalPath));
	else
		return junction->_archive->getMember(junction->_physicalPath);
}

Common::SeekableReadStream *VirtualFileSystem::createReadStreamForMember(const Common::Path &path) const {
	const Common::ArchiveMemberPtr ptr = getMember(path);

	if (ptr)
		return ptr->createReadStream();

	return nullptr;
}

IVirtualFileSystem *createVirtualFileSystem(char pathSeparator, bool caseSensitive, bool autoJunctionMacFiles) {
	return new VirtualFileSystem(pathSeparator, caseSensitive, autoJunctionMacFiles);
}

} // End of namespace MTropolis
