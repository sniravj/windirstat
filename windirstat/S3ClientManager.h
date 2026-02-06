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

// ============================================================================
// AWS SDK INTEGRATION
// ============================================================================
// To enable AWS SDK:
// 1. Install AWS SDK for C++ via vcpkg: vcpkg install aws-sdk-cpp[s3]:x64-windows
// 2. Set USE_AWS_SDK to 1 below
// 3. Uncomment the AWS SDK includes
// 4. Add aws-cpp-sdk-core and aws-cpp-sdk-s3 to linker inputs
// ============================================================================

#define USE_AWS_SDK 1  // AWS SDK is now enabled!

#if USE_AWS_SDK
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#endif

// S3 Object metadata structure
struct S3ObjectInfo
{
    std::wstring key;           // Object key (full path in bucket)
    std::wstring name;          // Object name (filename only)
    ULONGLONG size;            // Object size in bytes
    FILETIME lastModified;     // Last modified timestamp
    bool isPrefix;             // True for folders/prefixes, false for objects
    std::wstring storageClass; // S3 storage class (STANDARD, GLACIER, etc.)
    std::wstring etag;         // Object ETag
};

/// <summary>
/// Singleton class to manage AWS S3 client and credentials
/// Provides S3 operations: ListObjects, DeleteObject, GetObjectMetadata
/// </summary>
class CS3ClientManager
{
public:
    // Get singleton instance
    static CS3ClientManager& Get();

    // Initialize AWS SDK (call once at app startup)
    void Initialize();
    
    // Shutdown AWS SDK (call once at app exit)
    void Shutdown();

    // Set credentials (called from dialog)
    void SetCredentials(const std::wstring& bucketName,
                       const std::wstring& accessKey,
                       const std::wstring& secretKey,
                       const std::wstring& region);

    // Get credentials
    std::wstring GetBucketName() const { return m_bucketName; }
    std::wstring GetAccessKey() const { return m_accessKey; }
    std::wstring GetSecretKey() const { return m_secretKey; }
    std::wstring GetRegion() const { return m_region; }

    // Check if credentials are set
    bool HasCredentials() const { return !m_bucketName.empty() && !m_accessKey.empty(); }

    // Clear credentials
    void ClearCredentials();
    
    // ========================================================================
    // S3 Operations (to be implemented when AWS SDK is integrated)
    // ========================================================================
    
    // List objects in bucket with optional prefix and delimiter
    // prefix: Filter objects by prefix (e.g., "folder/")
    // delimiter: Used to group by common prefixes (e.g., "/" for folders)
    std::vector<S3ObjectInfo> ListObjects(const std::wstring& prefix = L"", 
                                          const std::wstring& delimiter = L"/");
    
    // Delete an S3 object by key
    bool DeleteObject(const std::wstring& key);
    
    // Get metadata for a single object
    S3ObjectInfo GetObjectMetadata(const std::wstring& key);

private:
    CS3ClientManager() = default;
    ~CS3ClientManager();

    // Prevent copying
    CS3ClientManager(const CS3ClientManager&) = delete;
    CS3ClientManager& operator=(const CS3ClientManager&) = delete;
    
    // Helper functions for string/time conversions
    static std::string WStringToString(const std::wstring& wstr);
    static std::wstring StringToWString(const std::string& str);
    static FILETIME TimeToFileTime(time_t t);
    static time_t FileTimeToTime(const FILETIME& ft);
    static std::wstring ExtractFileName(const std::wstring& key);

    // Credentials storage
    std::wstring m_bucketName;
    std::wstring m_accessKey;
    std::wstring m_secretKey;
    std::wstring m_region;
    
    bool m_initialized = false;

    // Thread safety
    mutable std::mutex m_mutex;
    
#if USE_AWS_SDK
    // AWS SDK client instance (created when credentials are set)
    std::unique_ptr<Aws::S3::S3Client> m_client;
#endif
};
