// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"
#include "Finder.h"
#include "S3ClientManager.h"

class CItem;

/// <summary>
/// Finder implementation for AWS S3 buckets
/// Uses S3 ListObjectsV2 API to enumerate objects and prefixes
/// </summary>
class FinderS3 final : public Finder
{
public:
    FinderS3() = default;
    ~FinderS3() = default;

    // Finder interface implementation
    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    DWORD GetAttributes() const override;
    ULONGLONG GetFileSizePhysical() const override;
    ULONGLONG GetFileSizeLogical() const override;
    FILETIME GetLastWriteTime() const override;
    std::wstring GetFilePath() const override;
    std::wstring GetFileName() const override;
    ULONGLONG GetIndex() const override;
    DWORD GetReparseTag() const override;
    bool IsReserved() const override;

private:
    // Set current item state from name and type
    void SetCurrentItem(const std::wstring& name, bool isPrefix);
    
    // Current state
    std::wstring m_currentPath;
    std::wstring m_currentName;
    ULONGLONG m_currentSize = 0;
    FILETIME m_currentTime = {};
    bool m_currentIsPrefix = false;  // true if it's a folder (prefix), false if object (file)
    
    // S3 enumeration state
    std::vector<std::wstring> m_objectKeys;  // List of object keys from S3
    std::vector<std::wstring> m_prefixes;     // List of common prefixes (folders) from S3
    std::unordered_map<std::wstring, S3ObjectInfo> m_objectInfo;  // Cached object metadata
    size_t m_currentIndex = 0;
    bool m_enumeratingPrefixes = true;
    
    // Helper to extract filename from S3 key
    static std::wstring ExtractFileName(const std::wstring& key);
};
