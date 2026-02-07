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
#include "HtmlExporter.h"
#include "DirStatDoc.h"
#include "Item.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <chrono>

// Main export method
bool CHtmlExporter::ExportToHtml(CDirStatDoc* doc)
{
    if (!doc || !doc->GetRootItem()) return false;

    try
    {
        // Get root item and generate filename
        CItem* rootItem = doc->GetRootItem();
        std::wstring rootPath = rootItem->GetPath();
        std::wstring filename = GenerateFilename(rootPath);

        // Serialize data to JSON
        std::wstring jsonTree = SerializeTreeToJson(rootItem);
        std::wstring jsonExtensions = SerializeExtensionsToJson(doc->GetExtensionData());

        // Generate HTML content
        std::wstring html = GenerateHtmlTemplate(jsonTree, jsonExtensions, rootPath);

        // Write to file
        bool success = WriteHtmlFile(filename, html);

        if (success)
        {
            VTRACE(L"HTML export successful: {}", filename);
        }
        else
        {
            VTRACE(L"HTML export failed: {}", filename);
        }

        return success;
    }
    catch (const std::exception& e)
    {
        VTRACE(L"HTML export exception: {}", std::wstring(CStringW(e.what())));
        return false;
    }
}

// Escape JSON strings
std::wstring CHtmlExporter::EscapeJson(const std::wstring& str)
{
    std::wstring escaped;
    escaped.reserve(str.length() * 1.2);

    for (wchar_t c : str)
    {
        switch (c)
        {
        case L'\\': escaped += L"\\\\"; break;
        case L'\"': escaped += L"\\\""; break;
        case L'\n': escaped += L"\\n"; break;
        case L'\r': escaped += L"\\r"; break;
        case L'\t': escaped += L"\\t"; break;
        case L'/': escaped += L"\\/"; break;
        default:
            if (c < 32)
            {
                // Escape control characters
                wchar_t buf[8];
                swprintf_s(buf, L"\\u%04x", static_cast<int>(c));
                escaped += buf;
            }
            else
            {
                escaped += c;
            }
            break;
        }
    }

    return escaped;
}

// Sanitize path for filename
std::wstring CHtmlExporter::SanitizePathForFilename(const std::wstring& path)
{
    std::wstring sanitized;
    
    for (wchar_t c : path)
    {
        if (iswalnum(c))
        {
            sanitized += towlower(c);
        }
        else if (c == L' ' || c == L'\\' || c == L'/' || c == L':' || c == L'-')
        {
            if (sanitized.empty() || sanitized.back() != L'_')
            {
                sanitized += L'_';
            }
        }
    }

    // Remove trailing underscores
    while (!sanitized.empty() && sanitized.back() == L'_')
    {
        sanitized.pop_back();
    }

    // Limit length
    if (sanitized.length() > 50)
    {
        sanitized = sanitized.substr(0, 50);
    }

    return sanitized;
}

// Get timestamp string
std::wstring CHtmlExporter::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time_t);

    wchar_t buffer[64];
    // Format: YYYY_MM_DD_HH_MM_AM/PM
    int hour12 = tm.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const wchar_t* ampm = tm.tm_hour < 12 ? L"am" : L"pm";

    swprintf_s(buffer, L"%04d_%02d_%02d_%02d_%02d_%s",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        hour12, tm.tm_min, ampm);

    return buffer;
}

// Generate filename
std::wstring CHtmlExporter::GenerateFilename(const std::wstring& rootPath)
{
    std::wstring sanitized = SanitizePathForFilename(rootPath);
    std::wstring timestamp = GetTimestamp();

    // Get current exe directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::filesystem::path dir = std::filesystem::path(exePath).parent_path();

    std::wstring filename = sanitized + L"_" + timestamp + L".html";
    return (dir / filename).wstring();
}

// Serialize tree to JSON
std::wstring CHtmlExporter::SerializeTreeToJson(CItem* rootItem)
{
    if (!rootItem) return L"{}";

    std::wstring json;
    json.reserve(1024 * 1024); // Reserve 1MB

    // Helper lambda for recursive serialization
    std::function<void(CItem*, bool)> serializeItem = [&](CItem* item, bool isLast)
    {
        json += L"{";

        // Basic info
        json += L"\"name\":\"" + EscapeJson(item->GetName()) + L"\",";
        json += L"\"path\":\"" + EscapeJson(item->GetPath()) + L"\",";

        // Type
        ITEMTYPE type = item->GetItemType();
        if (type & IT_FILE) json += L"\"type\":\"file\",";
        else if (type & IT_DIRECTORY) json += L"\"type\":\"directory\",";
        else if (type & IT_DRIVE) json += L"\"type\":\"drive\",";
        else if (type & IT_S3BUCKET) json += L"\"type\":\"s3bucket\",";
        else if (type & IT_S3PREFIX) json += L"\"type\":\"s3prefix\",";
        else if (type & IT_S3OBJECT) json += L"\"type\":\"s3object\",";
        else json += L"\"type\":\"unknown\",";

        // Sizes
        json += L"\"size\":" + std::to_wstring(item->GetSizeLogical()) + L",";
        json += L"\"sizePhysical\":" + std::to_wstring(item->GetSizePhysical()) + L",";

        // Counts
        json += L"\"files\":" + std::to_wstring(item->GetFilesCount()) + L",";
        json += L"\"subdirs\":" + std::to_wstring(item->GetFoldersCount()) + L",";

        // Extension
        std::wstring ext = item->GetExtension();
        json += L"\"extension\":\"" + EscapeJson(ext) + L"\",";

        // Last change time
        FILETIME ft = item->GetLastChange();
        SYSTEMTIME st;
        if (FileTimeToSystemTime(&ft, &st) && st.wYear > 1601)
        {
            wchar_t timeStr[64];
            swprintf_s(timeStr, L"%02d-%02d-%04d %02d:%02d",
                st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute);
            json += L"\"lastChange\":\"" + std::wstring(timeStr) + L"\",";
        }
        else
        {
            json += L"\"lastChange\":\"N/A\",";
        }

        json += L"\"expanded\":false";

        // Children
        if (!item->IsLeaf() && item->GetChildren().size() > 0)
        {
            json += L",\"children\":[";
            const auto& children = item->GetChildren();
            for (size_t i = 0; i < children.size(); i++)
            {
                serializeItem(children[i], i == children.size() - 1);
                if (i < children.size() - 1) json += L",";
            }
            json += L"]";
        }

        json += L"}";
    };

    serializeItem(rootItem, true);
    return json;
}

// Serialize extensions to JSON
std::wstring CHtmlExporter::SerializeExtensionsToJson(const CExtensionData* extData)
{
    if (!extData || extData->empty()) return L"[]";

    std::wstring json = L"[";
    bool first = true;

    // Calculate total size for percentages
    ULONGLONG totalBytes = 0;
    for (const auto& [ext, record] : *extData)
    {
        totalBytes += record.bytes.load(std::memory_order_relaxed);
    }

    // Create vector for sorting
    std::vector<std::pair<std::wstring, const SExtensionRecord*>> sorted;
    for (const auto& [ext, record] : *extData)
    {
        sorted.push_back({ ext, &record });
    }

    // Sort by bytes descending
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) {
            return a.second->bytes.load() > b.second->bytes.load();
        });

    for (const auto& [ext, record] : sorted)
    {
        if (!first) json += L",";
        first = false;

        // Get RGB values from COLORREF
        COLORREF color = record->color;
        int r = GetRValue(color);
        int g = GetGValue(color);
        int b = GetBValue(color);

        wchar_t hexColor[8];
        swprintf_s(hexColor, L"#%02X%02X%02X", r, g, b);

        ULONGLONG files = record->files.load(std::memory_order_relaxed);
        ULONGLONG bytes = record->bytes.load(std::memory_order_relaxed);
        double percent = totalBytes > 0 ? (static_cast<double>(bytes) / totalBytes * 100.0) : 0.0;

        json += L"{";
        json += L"\"ext\":\"" + EscapeJson(ext) + L"\",";
        json += L"\"files\":" + std::to_wstring(files) + L",";
        json += L"\"bytes\":" + std::to_wstring(bytes) + L",";
        
        wchar_t percentStr[16];
        swprintf_s(percentStr, L"%.1f", percent);
        json += L"\"percent\":" + std::wstring(percentStr) + L",";
        
        json += L"\"color\":\"" + std::wstring(hexColor) + L"\"";
        json += L"}";
    }

    json += L"]";
    return json;
}

// Write HTML file
bool CHtmlExporter::WriteHtmlFile(const std::wstring& filepath, const std::wstring& content)
{
    try
    {
        // Convert wstring to UTF-8
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(), NULL, 0, NULL, NULL);
        std::string utf8Content(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(), &utf8Content[0], size_needed, NULL, NULL);

        // Write file
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        file.write(utf8Content.c_str(), utf8Content.size());
        file.close();

        return true;
    }
    catch (...)
    {
        return false;
    }
}

// Generate HTML template with embedded demo HTML
std::wstring CHtmlExporter::GenerateHtmlTemplate(
    const std::wstring& jsonTree,
    const std::wstring& jsonExtensions,
    const std::wstring& rootName)
{
    // Embed the complete demo HTML as template - split into multiple parts
    // to avoid MSVC string literal size limit (~16KB)
    // Replace dummy data with actual JSON
    
    std::wstring html;
    html.reserve(500000); // Reserve space for efficiency
    
    // Part 1: HTML head start
    html += LR"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WinDirStat Scan - )HTML";
    
    html += EscapeJson(rootName);
    
    html += LR"HTML(</title>
    <script src="https://d3js.org/d3.v7.min.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, sans-serif;
            font-size: 11px;
            background: #F0F0F0;
            overflow: hidden;
        }

        .title-bar {
            display: none;
        }

        .toolbar {
            background: #FAFAFA;
            border-bottom: 1px solid #E0E0E0;
            padding: 8px 12px;
            height: 40px;
            display: flex;
            gap: 8px;
            align-items: center;
            box-shadow: 0 1px 3px rgba(0,0,0,0.08);
        }

        .toolbar button {
            border: none;
            background: #FFFFFF;
            padding: 6px 16px;
            font-size: 11px;
            cursor: pointer;
            font-family: 'Segoe UI', Tahoma, sans-serif;
            color: #333;
            border-radius: 4px;
            transition: all 0.15s ease;
            box-shadow: 0 1px 2px rgba(0,0,0,0.1);
            font-weight: 500;
        }

        .toolbar button:hover {
            background: #F5F5F5;
            box-shadow: 0 2px 4px rgba(0,0,0,0.15);
            transform: translateY(-1px);
        }

        .toolbar button:active {
            background: #E0E0E0;
            box-shadow: 0 1px 2px rgba(0,0,0,0.1);
            transform: translateY(0);
        }

        .toolbar .separator {
            width: 1px;
            height: 24px;
            background: linear-gradient(to bottom, transparent, #D0D0D0 20%, #D0D0D0 80%, transparent);
            margin: 0 4px;
        }

        .top-container {
            display: flex;
            height: 30vh;
            position: relative;
        }

        .tree-panel {
            width: 60%;
            height: 100%;
            display: flex;
            flex-direction: column;
            position: relative;
        }

        .splitter-vertical {
            width: 6px;
            background: #F5F5F5;
            cursor: col-resize;
            position: relative;
            user-select: none;
            border-left: 1px solid #E0E0E0;
            border-right: 1px solid #E0E0E0;
        }

        .splitter-vertical:hover {
            background: #E0E0E0;
        }

        .splitter-vertical:active {
            background: #2196F3;
        }

        .tree-header {
            background: #FAFAFA;
            border: none;
            border-bottom: 2px solid #E0E0E0;
            padding: 4px 5px;
            font-weight: 500;
            font-size: 11px;
        }
        
        .tree-header-row {
            display: flex;
            gap: 0;
        }
        
        .tree-header-col {
            text-align: left;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            position: relative;
            padding-right: 8px;
            padding-left: 4px;
            border-right: 1px solid #E0E0E0;
            color: #555;
            font-weight: 500;
        }

        .tree-header-col:last-child {
            border-right: none;
        }

        .column-resizer {
            position: absolute;
            right: -4px;
            top: 0;
            bottom: 0;
            width: 8px;
            cursor: col-resize;
            user-select: none;
            z-index: 10;
            background: transparent;
        }

        .column-resizer:hover {
            background: linear-gradient(to right, transparent, rgba(100, 100, 100, 0.15) 30%, rgba(100, 100, 100, 0.15) 70%, transparent);
        }

        .column-resizer:active {
            background: linear-gradient(to right, transparent, rgba(33, 150, 243, 0.3) 30%, rgba(33, 150, 243, 0.3) 70%, transparent);
        }

        .tree-view {
            flex: 1;
            background: white;
            border: 1px solid #E0E0E0;
            border-top: none;
            overflow-y: auto;
            overflow-x: auto;
            padding: 2px;
        }

        .tree-item {
            min-height: 18px;
            line-height: 18px;
            cursor: pointer;
            white-space: nowrap;
            padding: 2px 4px 2px 0;
        }

        .tree-item:hover {
            background: #F5F5F5;
        }

        .tree-item.selected {
            background: #E3F2FD;
            border-left: 3px solid #2196F3;
        }

        .tree-item.extension-match {
            background: rgba(135, 206, 250, 0.3);
            border-left: 3px solid #1E90FF;
        }

        .tree-item.extension-dimmed {
            opacity: 0.3;
        }

        .tree-icon {
            display: inline-block;
            width: 16px;
            text-align: center;
            font-size: 10px;
            cursor: pointer;
        }

        .tree-name {
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }

        .tree-percent-bar {
            position: relative;
            height: 14px;
            background: white;
            border: 1px solid #C0C0C0;
            overflow: hidden;
        }

        .tree-percent-fill {
            height: 100%;
            float: left;
        }

        .tree-col {
            text-align: right;
            color: #606060;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            padding-right: 8px;
            padding-left: 4px;
        }

        .extension-panel {
            width: calc(40% - 4px);
            height: 100%;
            display: flex;
            flex-direction: column;
        }
)HTML";
    
    // Part 1b: Extension table and treemap CSS
    html += LR"HTML(
        .extension-table {
            flex: 1;
            background: white;
            border: 1px solid #E0E0E0;
            overflow-y: auto;
            width: 100%;
            border-collapse: collapse;
        }

        .extension-table th {
            background: #FAFAFA;
            border: none;
            border-bottom: 2px solid #E0E0E0;
            border-right: 1px solid #E0E0E0;
            padding: 4px 12px;
            font-weight: 500;
            text-align: left;
            position: sticky;
            top: 0;
            cursor: pointer;
            font-size: 11px;
            position: relative;
            color: #555;
        }

        .extension-table th:last-child {
            border-right: none;
        }

        .extension-table th:hover {
            background: #F0F0F0;
        }

        .extension-table td {
            padding: 3px 12px;
            border-bottom: 1px solid #F0F0F0;
            border-right: 1px solid #F0F0F0;
        }

        .extension-table td:last-child {
            border-right: none;
        }

        .ext-row {
            cursor: pointer;
        }

        .ext-row:hover {
            background: #F5F5F5;
        }

        .ext-row.selected {
            background: #E3F2FD;
            font-weight: 500;
            border-left: 3px solid #2196F3;
        }

        .color-swatch {
            width: 12px;
            height: 12px;
            border: 1px solid #000;
            display: inline-block;
        }

        .percentage-bar {
            position: relative;
            width: 100%;
            height: 14px;
            background: white;
            border: 1px solid #C0C0C0;
        }

        .bar-fill {
            height: 100%;
            float: left;
        }

        .bar-text {
            position: absolute;
            left: 4px;
            top: -1px;
            font-size: 10px;
            line-height: 14px;
            color: #000;
            text-shadow: 1px 1px 1px rgba(255,255,255,0.8);
        }

        .splitter-horizontal {
            height: 6px;
            background: #F5F5F5;
            cursor: row-resize;
            position: relative;
            user-select: none;
            border-top: 1px solid #E0E0E0;
            border-bottom: 1px solid #E0E0E0;
        }

        .splitter-horizontal:hover {
            background: #E0E0E0;
        }

        .splitter-horizontal:active {
            background: #2196F3;
        }

        .treemap-panel {
            width: 100%;
            height: calc(70vh - 46px);
            background: white;
            border: 1px solid #E0E0E0;
            position: relative;
            display: flex;
            flex-direction: column;
        }

        .treemap-header {
            display: none;
        }

        #treemap {
            flex: 1;
            width: 100%;
            overflow: hidden;
            background: #000;
        }

        #treemap svg {
            display: block;
            background: #000;
        }

        .treemap-rect {
            stroke: #000;
            stroke-width: 0.5px;
            cursor: pointer;
            transition: none;
        }

        .treemap-rect:hover {
            stroke: #FFF;
            stroke-width: 2px;
        }

        .treemap-rect.selected {
            stroke: #FFFFFF !important;
            stroke-width: 3px !important;
        }

        #tooltip {
            position: absolute;
            background: #FFFFCC;
            border: 1px solid #000;
            padding: 6px;
            pointer-events: none;
            opacity: 0;
            font-size: 11px;
            line-height: 1.4;
            z-index: 1000;
            box-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }

        #tooltip strong {
            display: block;
            margin-bottom: 3px;
        }

        .text-muted {
            color: #606060;
            font-size: 10px;
        }

        .breadcrumbs {
            display: none;
        }

        .breadcrumbs a {
            color: #0066CC;
            text-decoration: none;
        }

        .breadcrumbs a:hover {
            text-decoration: underline;
        }

        ::-webkit-scrollbar {
            width: 16px;
            height: 16px;
        }

        ::-webkit-scrollbar-track {
            background: #F0F0F0;
        }

        ::-webkit-scrollbar-thumb {
            background: #C0C0C0;
            border: 1px solid #808080;
        }

        ::-webkit-scrollbar-thumb:hover {
            background: #A0A0A0;
        }
    </style>
</head>
)HTML";
    
    // Part 2: HTML body structure
    html += LR"HTML(<body>
    <div class="title-bar">WinDirStat Scan</div>

    <div class="toolbar">
        <button onclick="expandAll()">Expand All</button>
        <button onclick="collapseAll()">Collapse All</button>
        <div class="separator"></div>
        <button onclick="clearSelection()">Clear Selection</button>
        <button onclick="clearExtFilter()">Clear Filter</button>
        <div class="separator"></div>
        <button onclick="exportToImage()">Export Image</button>
    </div>

    <div class="top-container" id="top-container">
        <div class="tree-panel" id="tree-panel">
            <div class="tree-header">
                <div class="tree-header-row" id="tree-header-row">
                    <span class="tree-header-col" data-col="name" style="width: 200px; text-align: left;">
                        Name
                        <div class="column-resizer" data-col="name"></div>
                    </span>
                    <span class="tree-header-col" data-col="subtree" style="width: 120px; text-align: left;">
                        Subtree %
                        <div class="column-resizer" data-col="subtree"></div>
                    </span>
                    <span class="tree-header-col" data-col="percent" style="width: 60px; text-align: right;">
                        %
                        <div class="column-resizer" data-col="percent"></div>
                    </span>
                    <span class="tree-header-col" data-col="physical" style="width: 90px; text-align: right;">
                        Physical Size
                        <div class="column-resizer" data-col="physical"></div>
                    </span>
                    <span class="tree-header-col" data-col="logical" style="width: 90px; text-align: right;">
                        Logical Size
                        <div class="column-resizer" data-col="logical"></div>
                    </span>
                    <span class="tree-header-col" data-col="files" style="width: 60px; text-align: right;">
                        Files
                        <div class="column-resizer" data-col="files"></div>
                    </span>
                    <span class="tree-header-col" data-col="lastchange" style="width: 120px; text-align: left;">
                        Last Change
                    </span>
                </div>
            </div>
            <div class="tree-view" id="tree-view"></div>
        </div>

        <div class="splitter-vertical" id="splitter-vertical"></div>
)HTML";
    
    // Part 2b: Extension panel and treemap
    html += LR"HTML(
        <div class="extension-panel" id="extension-panel">
            <table class="extension-table" id="extension-table">
                <thead>
                    <tr id="ext-header-row">
                        <th data-col="ext" style="width: 80px;">
                            Extension ▼
                            <div class="column-resizer" data-col="ext"></div>
                        </th>
                        <th data-col="color" style="width: 30px;">
                            <div class="column-resizer" data-col="color"></div>
                        </th>
                        <th data-col="percent" style="width: 120px;">
                            Percentage
                            <div class="column-resizer" data-col="percent"></div>
                        </th>
                        <th data-col="files" style="width: 60px;">
                            Files
                            <div class="column-resizer" data-col="files"></div>
                        </th>
                        <th data-col="size" style="width: 80px;">
                            Size
                        </th>
                    </tr>
                </thead>
                <tbody id="extension-tbody"></tbody>
            </table>
        </div>
    </div>

    <div class="splitter-horizontal" id="splitter-horizontal"></div>

    <div class="treemap-panel">
        <div class="treemap-header">Treemap</div>
        <div class="breadcrumbs" id="breadcrumbs">
            <a href="#" onclick="zoomToPath(''); return false;">Root</a>
        </div>
        <div id="treemap"></div>
    </div>

    <div id="tooltip"></div>

    <script>
        // DATA PLACEHOLDER - Will be replaced with actual scan data
        const treeData = )HTML";
    
    html += jsonTree;
    
    html += LR"HTML(;
        const extensionData = )HTML";
    
    html += jsonExtensions;
    
    html += LR"HTML(;

        // State Management
        const AppState = {
            selectedPath: null,
            highlightedExtension: null,
            zoomPath: "",
            pathIndex: new Map(),
            currentTreemapData: null
        };

        function buildPathIndex(node) {
            AppState.pathIndex.set(node.path, node);
            if (node.children) {
                node.children.forEach(child => buildPathIndex(child));
            }
        }
        buildPathIndex(treeData);

        function formatSize(bytes) {
            if (bytes >= 1099511627776) return (bytes / 1099511627776).toFixed(2) + ' TB';
            if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(2) + ' GB';
            if (bytes >= 1048576) return (bytes / 1048576).toFixed(2) + ' MB';
            if (bytes >= 1024) return (bytes / 1024).toFixed(2) + ' KB';
            return bytes + ' B';
        }

        function renderTree() {
            const container = document.getElementById('tree-view');
            container.innerHTML = '';
            renderTreeNode(treeData, container, 0);
        }

        function renderTreeNode(node, container, level) {
            const div = document.createElement('div');
            div.className = 'tree-item';
            div.dataset.path = node.path;
            div.dataset.extension = node.extension || '';
            div.style.display = 'flex';
            div.style.gap = '4px';
            div.style.alignItems = 'center';

            const nameCol = document.createElement('div');
            nameCol.className = 'tree-col-name';
            nameCol.style.width = getTreeColumnWidth('name');
            nameCol.style.display = 'flex';
            nameCol.style.alignItems = 'center';
            nameCol.style.paddingLeft = (level * 16 + 8) + 'px';
            nameCol.style.paddingRight = '8px';

            const expandIcon = document.createElement('span');
            expandIcon.className = 'tree-icon';
            if (node.children && node.children.length > 0) {
                expandIcon.textContent = node.expanded ? '−' : '+';
                expandIcon.onclick = (e) => {
                    e.stopPropagation();
                    toggleExpand(node.path);
                };
            }
            nameCol.appendChild(expandIcon);

            const typeIcon = document.createElement('span');
            typeIcon.className = 'tree-icon';
            if (node.type === 'file' || node.type === 's3object') {
                typeIcon.textContent = '📄';
            } else {
                typeIcon.textContent = node.expanded ? '📂' : '📁';
            }
            nameCol.appendChild(typeIcon);

            const name = document.createElement('span');
            name.className = 'tree-name';
            name.textContent = node.name;
            name.style.flex = '1';
            nameCol.appendChild(name);
            div.appendChild(nameCol);

            const percentBar = document.createElement('div');
            percentBar.className = 'tree-col-subtree';
            percentBar.style.width = getTreeColumnWidth('subtree');
            percentBar.style.paddingRight = '8px';
            percentBar.style.paddingLeft = '4px';
            const totalSize = treeData.size;
            const percent = ((node.size / totalSize) * 100).toFixed(1);
            
            const extData = node.extension ? extensionData.find(e => e.ext === node.extension) : null;
            const color = extData ? extData.color : '#808080';
            
            const barContainer = document.createElement('div');
            barContainer.className = 'tree-percent-bar';
            const barFill = document.createElement('div');
            barFill.className = 'tree-percent-fill';
            barFill.style.width = Math.min(100, percent) + '%';
            barFill.style.background = color;
            barContainer.appendChild(barFill);
            percentBar.appendChild(barContainer);
            div.appendChild(percentBar);

            const percentText = document.createElement('div');
            percentText.className = 'tree-col tree-col-percent';
            percentText.style.width = getTreeColumnWidth('percent');
            percentText.textContent = percent + '%';
            div.appendChild(percentText);

            const physicalSize = document.createElement('div');
            physicalSize.className = 'tree-col tree-col-physical';
            physicalSize.style.width = getTreeColumnWidth('physical');
            physicalSize.textContent = formatSize(node.sizePhysical || node.size);
            div.appendChild(physicalSize);

            const logicalSize = document.createElement('div');
            logicalSize.className = 'tree-col tree-col-logical';
            logicalSize.style.width = getTreeColumnWidth('logical');
            logicalSize.textContent = formatSize(node.size);
            div.appendChild(logicalSize);

            const filesCol = document.createElement('div');
            filesCol.className = 'tree-col tree-col-files';
            filesCol.style.width = getTreeColumnWidth('files');
            filesCol.textContent = node.files.toLocaleString();
            div.appendChild(filesCol);

            const lastChange = document.createElement('div');
            lastChange.className = 'tree-col tree-col-lastchange';
            lastChange.style.width = getTreeColumnWidth('lastchange');
            lastChange.style.textAlign = 'left';
            lastChange.style.paddingLeft = '8px';
            lastChange.textContent = node.lastChange || 'N/A';
            div.appendChild(lastChange);

            div.onclick = () => selectTreeItem(node.path);

            container.appendChild(div);

            if (node.expanded && node.children) {
                node.children.forEach(child => renderTreeNode(child, container, level + 1));
            }
        }

        function toggleExpand(path) {
            const node = AppState.pathIndex.get(path);
            if (node && node.children) {
                node.expanded = !node.expanded;
                renderTree();
            }
        }

        function expandAll() {
            function expandNode(node) {
                if (node.children && node.children.length > 0) {
                    node.expanded = true;
                    node.children.forEach(expandNode);
                }
            }
            expandNode(treeData);
            renderTree();
        }

        function collapseAll() {
            function collapseNode(node) {
                node.expanded = false;
                if (node.children) {
                    node.children.forEach(collapseNode);
                }
            }
            collapseNode(treeData);
            renderTree();
        }

        function selectTreeItem(path) {
            AppState.selectedPath = path;
            
            document.querySelectorAll('.tree-item').forEach(el => {
                el.classList.remove('selected');
                if (el.dataset.path === path) {
                    el.classList.add('selected');
                }
            });

            d3.selectAll('.file-rect')
                .classed('selected', false)
                .attr('stroke', 'none')
                .attr('stroke-width', 0);
            
            const selectedRects = d3.selectAll('.file-rect').filter(function() {
                return d3.select(this).attr('data-path') === path;
            });
            
            selectedRects
                .classed('selected', true)
                .attr('stroke', '#FFF')
                .attr('stroke-width', 3);
        }

        function renderExtensions() {
            const tbody = document.getElementById('extension-tbody');
            tbody.innerHTML = '';

            const headerCells = document.querySelectorAll('#ext-header-row th');
            const widths = Array.from(headerCells).map(th => th.style.width || th.offsetWidth + 'px');
)HTML";
    
    // Part 3: Continue JavaScript
    html += LR"HTML(
            extensionData.forEach(ext => {
                const tr = document.createElement('tr');
                tr.className = 'ext-row';
                tr.dataset.extension = ext.ext;
                tr.onclick = () => selectExtension(ext.ext);

                const td1 = document.createElement('td');
                td1.style.width = widths[0];
                td1.textContent = ext.ext;
                tr.appendChild(td1);

                const td2 = document.createElement('td');
                td2.style.width = widths[1];
                td2.innerHTML = `<div class="color-swatch" style="background:${ext.color}"></div>`;
                tr.appendChild(td2);

                const td3 = document.createElement('td');
                td3.style.width = widths[2];
                td3.innerHTML = `
                    <div class="percentage-bar">
                        <div class="bar-fill" style="width:${ext.percent}%; background:${ext.color}"></div>
                        <span class="bar-text">${ext.percent}%</span>
                    </div>
                `;
                tr.appendChild(td3);

                const td4 = document.createElement('td');
                td4.style.width = widths[3];
                td4.textContent = ext.files.toLocaleString();
                tr.appendChild(td4);

                const td5 = document.createElement('td');
                td5.style.width = widths[4];
                td5.textContent = formatSize(ext.bytes);
                tr.appendChild(td5);

                tbody.appendChild(tr);
            });
        }

        function selectExtension(extension) {
            if (AppState.highlightedExtension === extension) {
                AppState.highlightedExtension = null;
            } else {
                AppState.highlightedExtension = extension;
            }

            document.querySelectorAll('.ext-row').forEach(row => {
                row.classList.toggle('selected', row.dataset.extension === AppState.highlightedExtension);
            });

            document.querySelectorAll('.tree-item').forEach(el => {
                const itemExt = el.dataset.extension;
                el.classList.remove('extension-match', 'extension-dimmed');
                
                if (AppState.highlightedExtension) {
                    if (itemExt === AppState.highlightedExtension) {
                        el.classList.add('extension-match');
                    } else if (itemExt) {
                        el.classList.add('extension-dimmed');
                    }
                }
            });

            d3.selectAll('.treemap-rect').each(function(d) {
                const rect = d3.select(this);
                if (AppState.highlightedExtension) {
                    if (d.data.extension === AppState.highlightedExtension) {
                        rect.style('opacity', 1.0)
                            .style('stroke', '#FFF')
                            .style('stroke-width', '1.5px');
                    } else {
                        rect.style('opacity', 0.3)
                            .style('stroke', '#000')
                            .style('stroke-width', '0.5px');
                    }
                } else {
                    rect.style('opacity', 1.0)
                        .style('stroke', '#000')
                        .style('stroke-width', '0.5px');
                }
            });
        }

        function clearExtFilter() {
            AppState.highlightedExtension = null;
            document.querySelectorAll('.ext-row').forEach(row => row.classList.remove('selected'));
            document.querySelectorAll('.tree-item').forEach(el => {
                el.classList.remove('extension-match', 'extension-dimmed');
            });
            d3.selectAll('.treemap-rect')
                .style('opacity', 1.0)
                .style('stroke', '#000')
                .style('stroke-width', '0.5px');
        }

        function clearSelection() {
            AppState.selectedPath = null;
            document.querySelectorAll('.tree-item').forEach(el => el.classList.remove('selected'));
            d3.selectAll('.file-rect')
                .classed('selected', false)
                .attr('stroke', 'none')
                .attr('stroke-width', 0);
        }

        function applyCushion(baseColor, intensity) {
            const r = parseInt(baseColor.substr(1, 2), 16);
            const g = parseInt(baseColor.substr(3, 2), 16);
            const b = parseInt(baseColor.substr(5, 2), 16);
            
            const newR = Math.round(r * intensity);
            const newG = Math.round(g * intensity);
            const newB = Math.round(b * intensity);
            
            return `rgb(${newR}, ${newG}, ${newB})`;
        }

        function renderTreemap(rootNode = treeData) {
            const container = document.getElementById('treemap');
            container.innerHTML = '';

            const width = container.clientWidth;
            const height = container.clientHeight;

            if (width === 0 || height === 0) {
                return;
            }

            const svg = d3.create('svg')
                .attr('width', width)
                .attr('height', height)
                .attr('viewBox', `0 0 ${width} ${height}`);

            const defs = svg.append('defs');

            const hierarchy = d3.hierarchy(rootNode)
                .sum(d => (d.type === 'file' || d.type === 's3object') ? d.size : 0)
                .sort((a, b) => b.value - a.value);

            const treemapLayout = d3.treemap()
                .size([width, height])
                .paddingOuter(0)
                .paddingInner(1)
                .paddingTop(0)
                .round(false);

            treemapLayout(hierarchy);

            function getBaseColor(d) {
                if (d.data.type === 'file' || d.data.type === 's3object') {
                    const extData = extensionData.find(e => e.ext === d.data.extension);
                    return extData ? extData.color : '#808080';
                }
                return '#E0E0E0';
            }

            const allNodes = hierarchy.descendants().filter(d => d.depth > 0);

            svg.selectAll('.folder-border')
                .data(allNodes.filter(d => d.children))
                .join('rect')
                .attr('class', 'folder-border')
                .attr('x', d => d.x0)
                .attr('y', d => d.y0)
                .attr('width', d => d.x1 - d.x0)
                .attr('height', d => d.y1 - d.y0)
                .attr('fill', 'none')
                .attr('stroke', '#000')
                .attr('stroke-width', 0.5);

            const leaves = hierarchy.leaves();
            
            leaves.forEach((d, i) => {
                const baseColor = getBaseColor(d);
                const gradientId = `gradient-${i}`;
                
                const gradient = defs.append('linearGradient')
                    .attr('id', gradientId)
                    .attr('x1', '0%')
                    .attr('y1', '0%')
                    .attr('x2', '0%')
                    .attr('y2', '100%');
                
                gradient.append('stop')
                    .attr('offset', '0%')
                    .attr('stop-color', applyCushion(baseColor, 1.3));
                
                gradient.append('stop')
                    .attr('offset', '50%')
                    .attr('stop-color', baseColor);
                
                gradient.append('stop')
                    .attr('offset', '100%')
                    .attr('stop-color', applyCushion(baseColor, 0.6));
            });
)HTML";
    
    // Part 4a-1: Treemap cells
    html += LR"HTML(
            const cells = svg.selectAll('.file-rect')
                .data(leaves)
                .join('rect')
                .attr('class', 'file-rect treemap-rect')
                .attr('data-path', d => d.data.path)
                .attr('x', d => d.x0)
                .attr('y', d => d.y0)
                .attr('width', d => Math.max(0, d.x1 - d.x0))
                .attr('height', d => Math.max(0, d.y1 - d.y0))
                .attr('fill', (d, i) => `url(#gradient-${i})`)
                .attr('stroke', 'none')
                .attr('stroke-width', 0)
                .on('click', (event, d) => {
                    event.stopPropagation();
                    selectTreeItemFromTreemap(d.data.path);
                })
)HTML";
    
    // Part 4: Continue JavaScript event handlers
    html += LR"HTML(
                .on('mouseover', function(event, d) {
                    const isSelected = d3.select(this).classed('selected');
                    if (!isSelected) {
                        d3.select(this)
                            .attr('stroke', '#FFF')
                            .attr('stroke-width', 1);
                    }
                    
                    const tooltip = document.getElementById('tooltip');
                    tooltip.style.opacity = 1;
                    tooltip.style.left = event.pageX + 10 + 'px';
                    tooltip.style.top = event.pageY + 10 + 'px';
                    tooltip.innerHTML = `
                        <strong>${d.data.name}</strong>
                        <div class="text-muted">${d.data.path}</div>
                        Size: ${formatSize(d.data.size)}<br/>
                        Extension: ${d.data.extension || 'N/A'}
                    `;
                })
                .on('mouseout', function() {
                    const isSelected = d3.select(this).classed('selected');
                    if (!isSelected) {
                        d3.select(this)
                            .attr('stroke', 'none')
                            .attr('stroke-width', 0);
                    }
                    document.getElementById('tooltip').style.opacity = 0;
                });

            container.appendChild(svg.node());
            AppState.currentTreemapData = rootNode;
        }

        function selectTreeItemFromTreemap(path) {
            AppState.selectedPath = path;
            
            d3.selectAll('.file-rect')
                .classed('selected', false)
                .attr('stroke', 'none')
                .attr('stroke-width', 0);
            
            const selectedRects = d3.selectAll('.file-rect').filter(function() {
                return d3.select(this).attr('data-path') === path;
            });
            
            selectedRects
                .classed('selected', true)
                .attr('stroke', '#FFF')
                .attr('stroke-width', 3);

            const node = AppState.pathIndex.get(path);
            if (node) {
                const pathParts = path.split('\\\\').filter(p => p);
                for (let i = 0; i <= pathParts.length; i++) {
                    const ancestorPath = i === 0 ? pathParts[0] + '\\\\' : pathParts.slice(0, i).join('\\\\');
                    const ancestor = AppState.pathIndex.get(ancestorPath);
                    if (ancestor) {
                        ancestor.expanded = true;
                    }
                }
                
                renderTree();

                setTimeout(() => {
                    document.querySelectorAll('.tree-item').forEach(el => {
                        el.classList.remove('selected');
                    });
                    
                    const treeItem = document.querySelector(`.tree-item[data-path="${CSS.escape(path)}"]`);
                    
                    if (treeItem) {
                        treeItem.classList.add('selected');
                        treeItem.scrollIntoView({ behavior: 'smooth', block: 'center' });
                    }
                }, 200);
            }
        }
)HTML";
    
    // Part 4b: Utility functions and column management
    html += LR"HTML(
        function zoomToPath(path) {
            const node = path ? AppState.pathIndex.get(path) : treeData;
            if (node) {
                renderTreemap(node);
            }
        }

        function exportToImage() {
            alert('Export to image feature');
        }

        function sortExtensions(column) {
            alert('Sort by: ' + column);
        }

        const treeColumnWidths = {
            name: '200px',
            subtree: '120px',
            percent: '60px',
            physical: '90px',
            logical: '90px',
            files: '60px',
            lastchange: '120px'
        };

        function getTreeColumnWidth(col) {
            return treeColumnWidths[col] || '100px';
        }

        function setTreeColumnWidth(col, width) {
            treeColumnWidths[col] = width;
            const header = document.querySelector(`#tree-header-row [data-col="${col}"]`);
            if (header) header.style.width = width;
            document.querySelectorAll(`.tree-col-${col}`).forEach(el => {
                el.style.width = width;
            });
        }

        let isResizingTreeColumn = false;
        let resizingTreeColumn = null;
        let treeColumnStartX = 0;
        let treeColumnStartWidth = 0;

        function initTreeColumnResizers() {
            document.querySelectorAll('#tree-header-row .column-resizer').forEach(resizer => {
                resizer.addEventListener('mousedown', (e) => {
                    e.stopPropagation();
                    e.preventDefault();
                    isResizingTreeColumn = true;
                    resizingTreeColumn = resizer.dataset.col;
                    treeColumnStartX = e.clientX;
                    const header = resizer.parentElement;
                    treeColumnStartWidth = header.offsetWidth;
                    document.body.style.cursor = 'col-resize';
                });
            });
        }

        let isResizingExtColumn = false;
        let resizingExtColumn = null;
        let extColumnStartX = 0;
        let extColumnStartWidth = 0;
        let extColumnIndex = 0;

        function initExtColumnResizers() {
            document.querySelectorAll('#ext-header-row .column-resizer').forEach(resizer => {
                resizer.addEventListener('mousedown', (e) => {
                    e.stopPropagation();
                    e.preventDefault();
                    isResizingExtColumn = true;
                    resizingExtColumn = resizer.dataset.col;
                    extColumnStartX = e.clientX;
                    const header = resizer.closest('th');
                    extColumnStartWidth = header.offsetWidth;
                    extColumnIndex = Array.from(header.parentElement.children).indexOf(header);
                    document.body.style.cursor = 'col-resize';
                });
            });
        }
)HTML";
    
    // Part 5a: Splitter and mouse event listeners
    html += LR"HTML(
        let isResizingVertical = false;
        let isResizingHorizontal = false;

        document.getElementById('splitter-vertical').addEventListener('mousedown', (e) => {
            isResizingVertical = true;
            document.body.style.cursor = 'col-resize';
            e.preventDefault();
        });

        document.getElementById('splitter-horizontal').addEventListener('mousedown', (e) => {
            isResizingHorizontal = true;
            document.body.style.cursor = 'row-resize';
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if (isResizingTreeColumn) {
                const diff = e.clientX - treeColumnStartX;
                const newWidth = Math.max(30, treeColumnStartWidth + diff);
                setTreeColumnWidth(resizingTreeColumn, newWidth + 'px');
                return;
            }

            if (isResizingExtColumn) {
                const diff = e.clientX - extColumnStartX;
                const newWidth = Math.max(30, extColumnStartWidth + diff);
                
                const headerCells = document.querySelectorAll('#ext-header-row th');
                headerCells[extColumnIndex].style.width = newWidth + 'px';
                
                const bodyCells = document.querySelectorAll(`#extension-tbody tr`);
                bodyCells.forEach(row => {
                    const cell = row.children[extColumnIndex];
                    if (cell) cell.style.width = newWidth + 'px';
                });
                return;
            }

            if (isResizingVertical) {
                const container = document.getElementById('top-container');
                const treePanel = document.getElementById('tree-panel');
                const extensionPanel = document.getElementById('extension-panel');
                
                const containerRect = container.getBoundingClientRect();
                const mouseX = e.clientX - containerRect.left;
                const percent = (mouseX / containerRect.width) * 100;
                
                if (percent > 30 && percent < 80) {
                    treePanel.style.width = percent + '%';
                    extensionPanel.style.width = `calc(${100 - percent}% - 6px)`;
                }
            }
)HTML";
    
    // Part 5: Final JavaScript and cleanup
    html += LR"HTML(
            if (isResizingHorizontal) {
                const topContainer = document.getElementById('top-container');
                const treemapPanel = document.querySelector('.treemap-panel');
                
                const viewportHeight = window.innerHeight;
                const toolbarHeight = 46;
                const mouseY = e.clientY - toolbarHeight;
                const percent = (mouseY / (viewportHeight - toolbarHeight)) * 100;
                
                if (percent > 20 && percent < 80) {
                    topContainer.style.height = percent + 'vh';
                    treemapPanel.style.height = `calc(${100 - percent}vh - 46px)`;
                    
                    setTimeout(() => {
                        if (AppState.currentTreemapData) {
                            renderTreemap(AppState.currentTreemapData);
                        }
                    }, 50);
                }
            }
        });

        document.addEventListener('mouseup', () => {
            if (isResizingTreeColumn) {
                isResizingTreeColumn = false;
                resizingTreeColumn = null;
                document.body.style.cursor = 'default';
            }
            if (isResizingExtColumn) {
                isResizingExtColumn = false;
                resizingExtColumn = null;
                document.body.style.cursor = 'default';
            }
            if (isResizingVertical || isResizingHorizontal) {
                document.body.style.cursor = 'default';
                isResizingVertical = false;
                isResizingHorizontal = false;
            }
        });
)HTML";
    
    // Part 6: Initialization code
    html += LR"HTML(
        renderTree();
        renderExtensions();
        
        initTreeColumnResizers();
        initExtColumnResizers();
        
        setTimeout(() => {
            renderTreemap();
        }, 100);

        window.addEventListener('resize', () => {
            if (AppState.currentTreemapData) {
                renderTreemap(AppState.currentTreemapData);
            }
        });
    </script>
</body>
</html>)HTML";

    return html;
}
