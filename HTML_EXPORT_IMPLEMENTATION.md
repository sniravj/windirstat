# HTML Export Implementation

## Overview

This document describes the implementation of the automatic HTML export feature for WinDirStat, which generates an interactive static HTML page after every scan completes.

## Feature Summary

After each scan (local drive or AWS S3 bucket), WinDirStat automatically exports the scan results as a self-contained interactive HTML file with:

- **File Tree View**: Expandable/collapsible hierarchical tree with all metadata columns
- **Extension Summary**: Sortable table showing file type statistics with color-coded percentage bars
- **Interactive Treemap**: D3.js-powered visualization with cushion rendering (3D effect), hover tooltips, and click-to-zoom
- **Bidirectional Synchronization**: Selecting items in tree/extensions/treemap highlights them across all views
- **Resizable Panels**: Draggable splitters for tree/extension panels and top/bottom panels
- **Column Resizing**: Draggable column headers in both tree and extension views
- **Modern Flat UI**: Clean, responsive design matching the desktop app's visual style

## Files Added

### 1. `windirstat/HtmlExporter.h`
Header file defining the `CHtmlExporter` class with static methods for HTML export.

**Key Methods:**
- `ExportToHtml(CDirStatDoc*)` - Main export method called after scan completion
- `SerializeTreeToJson(CItem*)` - Converts CItem hierarchy to JSON
- `SerializeExtensionsToJson(const CExtensionData*)` - Converts extension data to JSON
- `GenerateFilename(const std::wstring&)` - Creates timestamped filename
- `GenerateHtmlTemplate(...)` - Embeds data into HTML template
- `EscapeJson(const std::wstring&)` - JSON string escaping
- `SanitizePathForFilename(const std::wstring&)` - Path sanitization for filenames
- `GetTimestamp()` - Generates formatted timestamp
- `WriteHtmlFile(...)` - Writes UTF-8 encoded HTML to disk

### 2. `windirstat/HtmlExporter.cpp`
Implementation file containing:
- Complete HTML template embedded as C++ raw string literal
- JSON serialization logic for tree and extension data
- File I/O operations with UTF-8 encoding
- Filename generation with path sanitization

**HTML Template Features:**
- Embedded D3.js v7 from CDN (https://d3js.org/d3.v7.min.js)
- Complete CSS styling (modern flat design, Windows scrollbars, responsive layout)
- Full JavaScript implementation (tree rendering, treemap with cushion effect, synchronization, splitters, column resizing)
- ~1500 lines of embedded HTML/CSS/JavaScript

### 3. `windirstat_demo.html` (Reference Only)
Standalone HTML demo with dummy data, used as the master reference for visual design and interactive features. This file is not part of the C++ build but served as the development prototype.

## Files Modified

### 1. `windirstat/DirStatDoc.cpp`
**Changes:**
- Added `#include "HtmlExporter.h"` to includes section
- Added `CHtmlExporter::ExportToHtml(Get());` call after scan completion in `StartScanningEngine()` method (line ~2079)

**Location of Hook:**
```cpp
// Invoke a UI thread to do updates
CMainFrame::Get()->InvokeInMessageThread([&]
{
    // ... existing UI updates ...
    
    // AUTO-EXPORT HTML after scan completes
    CHtmlExporter::ExportToHtml(Get());
});
```

### 2. `windirstat/windirstat.vcxproj`
**Changes:**
- Added `<ClCompile Include="HtmlExporter.cpp" />` to compilation list (after DirStatDoc.cpp)
- Added `<ClInclude Include="HtmlExporter.h" />` to header list (after DirStatDoc.h)

## Filename Format

Generated HTML files follow this naming convention:

```
{sanitized_scan_target}_{YYYY_MM_DD_HH_MM_AM/PM}.html
```

**Examples:**
- `c_windows_2026_02_06_03_45_pm.html` (C:\Windows scan)
- `s3_my_bucket_name_2026_02_06_04_32_am.html` (S3 bucket scan)

**Path Sanitization Rules:**
- Alphanumeric characters converted to lowercase
- Spaces, backslashes, forward slashes, colons, and hyphens replaced with underscores
- Consecutive underscores collapsed to single underscore
- Trailing underscores removed
- Maximum length: 50 characters

**Timestamp Format:**
- 12-hour format with am/pm suffix
- Format: `YYYY_MM_DD_HH_MM_AM/PM`

**Save Location:**
- Same directory as the WinDirStat executable

## Data Serialization

### Tree Data (JSON)
Each CItem node is serialized with:
- `name` - Item name
- `path` - Full path (backslash-escaped for JSON)
- `type` - Item type (file, directory, drive, s3bucket, s3prefix, s3object)
- `size` - Logical size in bytes
- `sizePhysical` - Physical size in bytes
- `files` - File count
- `subdirs` - Subdirectory count
- `extension` - File extension (if file)
- `lastChange` - Last modified timestamp (formatted as "MM-DD-YYYY HH:MM")
- `expanded` - Boolean (default: false)
- `children` - Array of child nodes (recursive)

**Example JSON Structure:**
```json
{
  "name": "C:\\",
  "path": "C:\\",
  "type": "drive",
  "size": 523400000000,
  "sizePhysical": 523400000000,
  "files": 15234,
  "subdirs": 456,
  "extension": "",
  "lastChange": "02-06-2026 15:30",
  "expanded": false,
  "children": [...]
}
```

### Extension Data (JSON)
Each extension record is serialized with:
- `ext` - Extension string (e.g., ".jpg", "[Folders]")
- `files` - Number of files
- `bytes` - Total bytes
- `percent` - Percentage of total size (1 decimal)
- `color` - RGB color as hex string (e.g., "#FF5733")

Extensions are sorted by byte count (descending).

**Example JSON Structure:**
```json
[
  {
    "ext": ".jpg",
    "files": 3456,
    "bytes": 95400000000,
    "percent": 18.2,
    "color": "#FF5733"
  },
  ...
]
```

## HTML Features

### 1. Tree View
- Expandable/collapsible hierarchy with +/− icons
- Multiple columns: Name, Subtree %, %, Physical Size, Logical Size, Files, Last Change
- Color-coded horizontal percentage bars (by extension color)
- Selection highlighting (light blue background, blue left border)
- Extension filtering (highlights matching items, dims others)
- Resizable columns via draggable headers
- Icon indicators (📁/📂 for folders, 📄 for files)

### 2. Extension Table
- Sortable columns: Extension, Color, Percentage, Files, Size
- Color swatches for each extension
- Percentage bars matching extension colors
- Selection highlighting
- Click to filter tree and treemap by extension
- Resizable columns

### 3. Treemap
- D3.js treemap layout with:
  - Cushion rendering (3D gradient effect)
  - Deep nested folder boundaries (thin black lines)
  - File rectangles with no borders
  - Hover tooltips (name, path, size, extension)
  - Click-to-select synchronization with tree
  - White highlight border for selected items (3px)
  - Black background (matching desktop app)
  - Extension filtering (opacity dimming)
- Responsive (re-renders on window resize)

### 4. Toolbar
- Expand All / Collapse All buttons
- Clear Selection / Clear Filter buttons
- Export Image button (placeholder)
- Modern flat design (white buttons, rounded corners, shadows)
- Gradient separators

### 5. Resizable Panels
- Vertical splitter between tree and extension panels (drag left/right)
- Horizontal splitter between top and treemap panels (drag up/down)
- Default split: 60% tree / 40% extensions (horizontal), 30% top / 70% treemap (vertical)
- Visual feedback (color change on hover/active)

### 6. Synchronization
- Tree selection → Treemap highlight (white border)
- Treemap click → Tree expansion and selection
- Extension selection → Tree/Treemap filtering
- All views update in real-time

## Technical Details

### Dependencies
- **D3.js v7**: Loaded from CDN (https://d3js.org/d3.v7.min.js)
- **No other external dependencies** - fully self-contained HTML file

### Browser Compatibility
- Modern browsers with ES6+ support (Chrome, Edge, Firefox, Safari)
- Requires SVG support for treemap
- Uses CSS3 features (flexbox, gradients, transitions)

### Performance Considerations
- Large scans (100k+ files) may have slower initial rendering
- Treemap uses D3's optimized hierarchical layout
- Tree view uses virtual scrolling via browser's native scrollbar
- JSON data is embedded directly (no lazy loading)

### File Size
- Typical HTML export size: 1-10 MB depending on scan size
- Template overhead: ~50 KB (HTML/CSS/JS)
- Data size scales linearly with file count

## Build Integration

The HTML export is fully integrated into the WinDirStat build:

1. **Compilation**: `HtmlExporter.cpp` is compiled with the rest of the project
2. **Linking**: No additional libraries required (uses Windows API and STL)
3. **Runtime**: Export happens automatically after every scan on the UI thread
4. **Error Handling**: Export failures are logged but don't interrupt the scan workflow

## Testing

### Manual Testing Steps

1. **Local Drive Scan:**
   - Build and run WinDirStat
   - Scan a local drive (e.g., C:\Windows)
   - After scan completes, check the executable directory for the generated HTML file
   - Open HTML file in browser, verify all interactive features work

2. **S3 Bucket Scan:**
   - Configure AWS credentials
   - Scan an S3 bucket
   - After scan completes, check for generated HTML file with S3 bucket name
   - Verify S3 paths are correctly formatted in the tree and treemap

3. **Interactive Features:**
   - Click items in tree → verify treemap highlights
   - Click rectangles in treemap → verify tree expansion and selection
   - Click extensions → verify filtering in tree and treemap
   - Drag splitters → verify panels resize smoothly
   - Drag column headers → verify columns resize
   - Use Expand All / Collapse All buttons
   - Verify tooltips show on treemap hover

### Known Limitations

1. **Timestamp Accuracy**: File timestamps are converted to local time and may differ slightly from the desktop app display
2. **Color Assignment**: Extension colors must match the desktop app's color assignments (handled by `CExtensionData`)
3. **Large Scans**: Very large scans (1M+ files) may result in large HTML files (100+ MB) and slow browser performance
4. **CDN Dependency**: Requires internet connection to load D3.js (could be made offline by embedding D3.js, but would increase file size by ~250 KB)

## Future Enhancements

Potential improvements for future versions:

1. **Offline Mode**: Embed D3.js directly to eliminate CDN dependency
2. **Data Compression**: Use gzip or base64 compression for JSON data
3. **Lazy Loading**: Load tree data progressively for very large scans
4. **Export Options**: User preference to enable/disable auto-export
5. **Custom Filename**: Allow user to specify export filename/location
6. **Multiple Formats**: Support additional export formats (JSON, CSV)
7. **Zoom/Pan**: Add zoom controls for treemap
8. **Search**: Implement file search within HTML export

## Conclusion

The HTML export feature successfully provides a portable, interactive snapshot of WinDirStat scan results. The implementation is clean, maintainable, and fully integrated into the existing codebase without requiring external dependencies beyond the D3.js CDN.

The exported HTML files faithfully replicate the desktop app's visual style and interactive behavior, making them suitable for sharing scan results, documentation, or archival purposes.
