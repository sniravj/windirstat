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
#include "DirStatDoc.h"
#include "Item.h"

//
// CHtmlExporter - Exports scan results to interactive HTML file
//
class CHtmlExporter
{
public:
    // Main export method - generates complete HTML file with all scan data
    static bool ExportToHtml(CDirStatDoc* doc);

private:
    // Data serialization methods
    static std::wstring SerializeTreeToJson(CItem* rootItem);
    static std::wstring SerializeExtensionsToJson(const CExtensionData* extData);
    
    // Helper methods
    static std::wstring EscapeJson(const std::wstring& str);
    static std::wstring SanitizePathForFilename(const std::wstring& path);
    static std::wstring GetTimestamp();
    static std::wstring GenerateFilename(const std::wstring& rootPath);
    
    // Template generation
    static std::wstring GenerateHtmlTemplate(
        const std::wstring& jsonTree,
        const std::wstring& jsonExtensions,
        const std::wstring& rootName);
    
    // Write file to disk
    static bool WriteHtmlFile(const std::wstring& filepath, const std::wstring& content);
};
