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

#include "pch.h"
#include "FinderS3.h"
#include "Item.h"
#include "S3ClientManager.h"

// ============================================================================
// FinderS3 Implementation
// ============================================================================
// This class enumerates S3 objects and prefixes (folders) in a bucket.
// It implements the Finder interface to integrate with WinDirStat's scanning engine.
// 
// The implementation uses CS3ClientManager to make S3 API calls.
// When USE_AWS_SDK=0 (default), CS3ClientManager returns empty results (stub mode).
// When USE_AWS_SDK=1, actual S3 ListObjectsV2 calls are made.
// ============================================================================

bool FinderS3::FindFile(const CItem* item)
{
    ASSERT(item != nullptr);
    
    // Reset state
    m_currentIndex = 0;
    m_enumeratingPrefixes = true;
    m_objectKeys.clear();
    m_prefixes.clear();
    m_objectInfo.clear();
    
    // Get the S3 path to enumerate
    std::wstring prefix;
    if (item->IsTypeOrFlag(IT_S3BUCKET))
    {
        // Enumerating bucket root
        prefix = L"";
        //VTRACE(L"FinderS3: Enumerating bucket root");
    }
    else if (item->IsTypeOrFlag(IT_S3PREFIX))
    {
        // Enumerating a prefix (folder)
        // Get full path (e.g., "s3://bucket-name/reports/processing-state/")
        std::wstring fullPath = item->GetPath();
        //VTRACE(L"FinderS3: Full path from GetPath(): '{}'", fullPath);
        
        // Strip "s3://" prefix
        if (fullPath.starts_with(L"s3://"))
        {
            fullPath = fullPath.substr(5);  // Remove "s3://"
        }
        
        // Find first '/' after bucket name to get the prefix part
        size_t firstSlash = fullPath.find(L'/');
        if (firstSlash != std::wstring::npos)
        {
            // Extract prefix after bucket name (e.g., "bucket-name/reports/processing-state/" -> "reports/processing-state/")
            prefix = fullPath.substr(firstSlash + 1);
        }
        else
        {
            // No slash found, this shouldn't happen for valid S3PREFIX
            prefix = L"";
        }
        
        //VTRACE(L"FinderS3: Enumerating prefix: '{}'", prefix);
    }
    else
    {
        // Not an S3 item
        //VTRACE(L"FinderS3: Item is not S3BUCKET or S3PREFIX");
        return false;
    }
    
    // Call S3 ListObjects API via CS3ClientManager
    auto& s3Manager = CS3ClientManager::Get();
    if (!s3Manager.HasCredentials())
    {
        //VTRACE(L"FinderS3: No credentials set");
        return false;
    }
    
    // Get list of objects and prefixes from S3
    std::vector<S3ObjectInfo> objects = s3Manager.ListObjects(prefix, L"/");
    
    //VTRACE(L"FinderS3: Got {} results from ListObjects", objects.size());
    
    // Separate into prefixes and objects
    for (const auto& obj : objects)
    {
        if (obj.isPrefix)
        {
            // Remove parent prefix to get relative name
            std::wstring relativeName = obj.key;
            if (!prefix.empty() && relativeName.starts_with(prefix))
            {
                relativeName = relativeName.substr(prefix.length());
            }
            // Remove trailing slash
            if (relativeName.ends_with(L'/'))
            {
                relativeName = relativeName.substr(0, relativeName.length() - 1);
            }
            
            if (!relativeName.empty())
            {
                m_prefixes.push_back(relativeName);
                //VTRACE(L"FinderS3: Added prefix '{}' (full key='{}')", relativeName, obj.key);
            }
        }
        else
        {
            // Regular object (file)
            std::wstring relativeName = obj.key;
            if (!prefix.empty() && relativeName.starts_with(prefix))
            {
                relativeName = relativeName.substr(prefix.length());
            }
            
            if (!relativeName.empty())
            {
                m_objectKeys.push_back(relativeName);
                m_objectInfo[relativeName] = obj;
                //VTRACE(L"FinderS3: Added object '{}' to cache (full key='{}', size={})", 
                //       relativeName, obj.key, obj.size);
            }
        }
    }
    
    //VTRACE(L"FinderS3: Found {} prefixes and {} objects", m_prefixes.size(), m_objectKeys.size());
    
    // Start with prefixes (folders)
    m_currentIndex = 0;
    m_enumeratingPrefixes = true;
    
    // Return true if we have any items
    if (!m_prefixes.empty())
    {
        SetCurrentItem(m_prefixes[0], true);
        return true;
    }
    else if (!m_objectKeys.empty())
    {
        m_enumeratingPrefixes = false;
        SetCurrentItem(m_objectKeys[0], false);
        return true;
    }
    
    return false;
}

bool FinderS3::FindNext()
{
    m_currentIndex++;
    
    // Check if we're still enumerating prefixes
    if (m_enumeratingPrefixes)
    {
        if (m_currentIndex < m_prefixes.size())
        {
            SetCurrentItem(m_prefixes[m_currentIndex], true);
            return true;
        }
        
        // Done with prefixes, move to objects
        m_enumeratingPrefixes = false;
        m_currentIndex = 0;
    }
    
    // Enumerate objects
    if (m_currentIndex < m_objectKeys.size())
    {
        SetCurrentItem(m_objectKeys[m_currentIndex], false);
        return true;
    }
    
    // No more items
    return false;
}

void FinderS3::SetCurrentItem(const std::wstring& name, bool isPrefix)
{
    m_currentName = name;
    m_currentIsPrefix = isPrefix;
    
    if (isPrefix)
    {
        // Prefix (folder)
        m_currentPath = name;
        m_currentSize = 0;
        m_currentTime = {};
        //VTRACE(L"FinderS3::SetCurrentItem - PREFIX: '{}' (size=0)", name);
    }
    else
    {
        // Object (file)
        m_currentPath = name;
        
        // Get metadata from cached info
        if (m_objectInfo.contains(name))
        {
            const auto& info = m_objectInfo[name];
            m_currentSize = info.size;
            m_currentTime = info.lastModified;
            //VTRACE(L"FinderS3::SetCurrentItem - OBJECT: '{}' (size={} bytes, found in cache)", 
            //       name, m_currentSize);
        }
        else
        {
            m_currentSize = 0;
            m_currentTime = {};
            //VTRACE(L"FinderS3::SetCurrentItem - OBJECT: '{}' (size=0, NOT in cache!) - objectInfo size={}", 
            //       name, m_objectInfo.size());
        }
    }
}

DWORD FinderS3::GetAttributes() const
{
    return m_currentIsPrefix ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

ULONGLONG FinderS3::GetFileSizePhysical() const
{
    // For S3, physical = logical (no cluster allocation)
    return m_currentSize;
}

ULONGLONG FinderS3::GetFileSizeLogical() const
{
    return m_currentSize;
}

FILETIME FinderS3::GetLastWriteTime() const
{
    return m_currentTime;
}

std::wstring FinderS3::GetFilePath() const
{
    return m_currentPath;
}

std::wstring FinderS3::GetFileName() const
{
    return m_currentName;
}

ULONGLONG FinderS3::GetIndex() const
{
    // S3 doesn't have file indices
    return 0;
}

DWORD FinderS3::GetReparseTag() const
{
    // S3 objects are never reparse points
    return 0;
}

bool FinderS3::IsReserved() const
{
    // S3 objects are never reserved
    return false;
}

std::wstring FinderS3::ExtractFileName(const std::wstring& key)
{
    // Extract the last component from S3 key
    // e.g., "folder/subfolder/file.txt" -> "file.txt"
    const size_t pos = key.find_last_of(L'/');
    if (pos != std::wstring::npos)
    {
        return key.substr(pos + 1);
    }
    return key;
}
