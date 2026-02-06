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
#include "S3ClientManager.h"

#if USE_AWS_SDK
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/ListObjectsV2Result.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#endif

CS3ClientManager& CS3ClientManager::Get()
{
    static CS3ClientManager instance;
    return instance;
}

CS3ClientManager::~CS3ClientManager()
{
    Shutdown();
}

void CS3ClientManager::Initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) return;
    
#if USE_AWS_SDK
    // Initialize AWS SDK
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    VTRACE(L"AWS SDK initialized");
#else
    VTRACE(L"AWS SDK not compiled - using stub implementation");
#endif
    
    m_initialized = true;
}

void CS3ClientManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) return;
    
#if USE_AWS_SDK
    // Cleanup S3 client
    m_client.reset();
    
    // Shutdown AWS SDK
    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);
    VTRACE(L"AWS SDK shutdown");
#endif
    
    m_initialized = false;
}

void CS3ClientManager::SetCredentials(const std::wstring& bucketName,
                                     const std::wstring& accessKey,
                                     const std::wstring& secretKey,
                                     const std::wstring& region)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_bucketName = bucketName;
    m_accessKey = accessKey;
    m_secretKey = secretKey;
    m_region = region;
    
#if USE_AWS_SDK
    // Create S3 client with credentials
    auto credentialsProvider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
        WStringToString(accessKey).c_str(),
        WStringToString(secretKey).c_str());
    
    Aws::S3::S3ClientConfiguration config;
    config.region = WStringToString(region);
    
    // Use the constructor that takes credentials provider, endpoint provider (nullptr), and config
    m_client = std::make_unique<Aws::S3::S3Client>(credentialsProvider, nullptr, config);
    VTRACE(L"S3 Client created for bucket: {}, region: {}", bucketName, region);
#else
    VTRACE(L"Credentials set (stub mode): bucket={}, region={}", bucketName, region);
#endif
}

void CS3ClientManager::ClearCredentials()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_bucketName.clear();
    m_accessKey.clear();
    m_secretKey.clear();
    m_region.clear();
    
#if USE_AWS_SDK
    m_client.reset();
#endif
}

std::vector<S3ObjectInfo> CS3ClientManager::ListObjects(const std::wstring& prefix, 
                                                         const std::wstring& delimiter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<S3ObjectInfo> results;
    
#if USE_AWS_SDK
    if (!m_client) return results;
    
    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(WStringToString(m_bucketName));
    
    if (!prefix.empty())
        request.SetPrefix(WStringToString(prefix));
    
    if (!delimiter.empty())
        request.SetDelimiter(WStringToString(delimiter));
    
    //VTRACE(L"ListObjects called: prefix='{}', delimiter='{}'", prefix, delimiter);
    
    // Handle pagination - AWS S3 returns max 1000 items per call
    bool isTruncated = false;
    std::string continuationToken;
    int pageCount = 0;
    int totalPrefixes = 0;
    int totalObjects = 0;
    
    do
    {
        if (!continuationToken.empty())
        {
            request.SetContinuationToken(continuationToken);
        }
        
        auto outcome = m_client->ListObjectsV2(request);
        
        if (outcome.IsSuccess())
        {
            const auto& result = outcome.GetResult();
            pageCount++;
            
            int pagePrefixes = static_cast<int>(result.GetCommonPrefixes().size());
            int pageObjects = static_cast<int>(result.GetContents().size());
            totalPrefixes += pagePrefixes;
            totalObjects += pageObjects;
            
            //VTRACE(L"ListObjects page {} SUCCESS: {} prefixes, {} objects", 
            //       pageCount, pagePrefixes, pageObjects);
            
            // Add common prefixes (folders)
            for (const auto& commonPrefix : result.GetCommonPrefixes())
            {
                S3ObjectInfo info;
                info.key = StringToWString(commonPrefix.GetPrefix());
                info.name = ExtractFileName(info.key);
                info.size = 0;
                info.isPrefix = true;
                info.lastModified = {};
                results.push_back(info);
                //VTRACE(L"  PREFIX: {}", info.key);
            }
            
            // Add objects (files)
            for (const auto& object : result.GetContents())
            {
                S3ObjectInfo info;
                info.key = StringToWString(object.GetKey());
                info.name = ExtractFileName(info.key);
                info.size = object.GetSize();
                info.isPrefix = false;
                info.lastModified = TimeToFileTime(object.GetLastModified().Millis() / 1000);
                info.storageClass = StringToWString(
                    Aws::S3::Model::ObjectStorageClassMapper::GetNameForObjectStorageClass(
                        object.GetStorageClass()));
                info.etag = StringToWString(object.GetETag());
                results.push_back(info);
                //VTRACE(L"  OBJECT: {} ({} bytes)", info.key, info.size);
            }
            
            // Check if there are more results
            isTruncated = result.GetIsTruncated();
            if (isTruncated)
            {
                continuationToken = result.GetNextContinuationToken();
                //VTRACE(L"  More results available, fetching next page...");
            }
        }
        else
        {
            VTRACE(L"ListObjects failed: {}", 
                   StringToWString(outcome.GetError().GetMessage()));
            break;
        }
    }
    while (isTruncated);
    
    //if (pageCount > 1)
    //{
    //    VTRACE(L"ListObjects COMPLETE: {} total pages, {} total prefixes, {} total objects", 
    //           pageCount, totalPrefixes, totalObjects);
    //}
#else
    // STUB: Return empty list - in real implementation, this would call AWS S3 API
    VTRACE(L"ListObjects stub called: bucket={}, prefix={}, delimiter={}", 
           m_bucketName, prefix, delimiter);
#endif
    
    return results;
}

bool CS3ClientManager::DeleteObject(const std::wstring& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
#if USE_AWS_SDK
    if (!m_client) return false;
    
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(WStringToString(m_bucketName));
    request.SetKey(WStringToString(key));
    
    auto outcome = m_client->DeleteObject(request);
    
    if (outcome.IsSuccess())
    {
        VTRACE(L"Deleted S3 object: {}", key);
        return true;
    }
    else
    {
        VTRACE(L"DeleteObject failed: {}", 
               StringToWString(outcome.GetError().GetMessage()));
        return false;
    }
#else
    // STUB: Return true - in real implementation, this would call AWS S3 DeleteObject API
    VTRACE(L"DeleteObject stub called: bucket={}, key={}", m_bucketName, key);
    return true;
#endif
}

S3ObjectInfo CS3ClientManager::GetObjectMetadata(const std::wstring& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    S3ObjectInfo info;
    
#if USE_AWS_SDK
    if (!m_client) return info;
    
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(WStringToString(m_bucketName));
    request.SetKey(WStringToString(key));
    
    auto outcome = m_client->HeadObject(request);
    
    if (outcome.IsSuccess())
    {
        const auto& result = outcome.GetResult();
        info.key = key;
        info.name = ExtractFileName(key);
        info.size = result.GetContentLength();
        info.isPrefix = false;
        info.lastModified = TimeToFileTime(result.GetLastModified().Millis() / 1000);
        info.storageClass = StringToWString(
            Aws::S3::Model::StorageClassMapper::GetNameForStorageClass(
                result.GetStorageClass()));
        info.etag = StringToWString(result.GetETag());
    }
    else
    {
        VTRACE(L"HeadObject failed: {}", 
               StringToWString(outcome.GetError().GetMessage()));
    }
#else
    // STUB: Return empty info - in real implementation, this would call AWS S3 HeadObject API
    VTRACE(L"GetObjectMetadata stub called: bucket={}, key={}", m_bucketName, key);
#endif
    
    return info;
}

// Helper: Convert wstring to UTF-8 string
std::string CS3ClientManager::WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                                                 static_cast<int>(wstr.length()), 
                                                 nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), 
                        str.data(), size_needed, nullptr, nullptr);
    return str;
}

// Helper: Convert UTF-8 string to wstring
std::wstring CS3ClientManager::StringToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                                                 static_cast<int>(str.length()), 
                                                 nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), 
                        wstr.data(), size_needed);
    return wstr;
}

// Helper: Convert time_t to FILETIME
FILETIME CS3ClientManager::TimeToFileTime(time_t t)
{
    FILETIME ft{};
    const LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000LL;
    ft.dwLowDateTime = static_cast<DWORD>(ll);
    ft.dwHighDateTime = static_cast<DWORD>(ll >> 32);
    return ft;
}

// Helper: Convert FILETIME to time_t
time_t CS3ClientManager::FileTimeToTime(const FILETIME& ft)
{
    const ULARGE_INTEGER ull = {ft.dwLowDateTime, ft.dwHighDateTime};
    return static_cast<time_t>((ull.QuadPart - 116444736000000000LL) / 10000000);
}

// Helper: Extract filename from S3 key
std::wstring CS3ClientManager::ExtractFileName(const std::wstring& key)
{
    const size_t pos = key.find_last_of(L'/');
    if (pos != std::wstring::npos)
    {
        return key.substr(pos + 1);
    }
    return key;
}
