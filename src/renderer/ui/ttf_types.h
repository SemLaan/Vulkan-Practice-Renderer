#pragma once
#include "core/asserts.h"
#include "defines.h"
#include <stdio.h>



static inline u32 readU32(FILE* file)
{
    u8 buf[4];
    u32 readSize = fread(buf, 1, 4, file);
    GRASSERT(readSize == 4);
    u8 temp = buf[0];
    buf[0] = buf[3];
    buf[3] = temp;
    temp = buf[1];
    buf[1] = buf[2];
    buf[2] = temp;
    return *(u32*)buf;
}

static inline u16 readU16(FILE* file)
{
    u8 buf[2];
    u32 readSize = fread(buf, 1, 2, file);
    GRASSERT(readSize == 2);
    u8 temp = buf[0];
    buf[0] = buf[1];
    buf[1] = temp;
    return *(u16*)buf;
}

static inline i16 readI16(FILE* file)
{
    u8 buf[2];
    u32 readSize = fread(buf, 1, 2, file);
    GRASSERT(readSize == 2);
    u8 temp = buf[0];
    buf[0] = buf[1];
    buf[1] = temp;
    return *(i16*)buf;
}

// Describes how many other tables there are
typedef struct OffsetTable
{
    u32 scalerType;
    u16 numTables;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
} OffsetTable;

static inline OffsetTable readOffsetTable(FILE* file)
{
    OffsetTable offsetTable;
    offsetTable.scalerType = readU32(file);
    offsetTable.numTables = readU16(file);
    offsetTable.searchRange = readU16(file);
    offsetTable.entrySelector = readU16(file);
    offsetTable.rangeShift = readU16(file);
    return offsetTable;
}

// Describes a table of the font
typedef struct TableRecord
{
    char tag[5];
    u32 checkSum;
    u32 offset;
    u32 length;
} TableRecord;

static inline TableRecord readTableRecord(FILE* file)
{
    TableRecord tableRecord;

    u32 readSize = fread(tableRecord.tag, 1, 4, file);
    GRASSERT(readSize == 4);

    tableRecord.checkSum = readU32(file);
    tableRecord.offset = readU32(file);
    tableRecord.length = readU32(file);
    return tableRecord;
}

typedef struct HorizontalHeaderTable
{
    u32 version;
    i16 ascent;
    i16 descent;
    i16 lineGap;
    u16 advanceWidthMax;
    i16 minLeftSideBearing;
    i16 minRightSideBearing;
    i16 xMaxExtent;
    i16 caretSlopeRise;
    i16 caretSlopeRun;
    i16 caretOffset;
    i16 reversed;
    i16 reversed1;
    i16 reversed2;
    i16 reversed4;
    i16 metricDataFormat;
    u16 numOfLongHorMetrics;
} HorizontalHeaderTable;

static inline HorizontalHeaderTable readHorizontalHeaderTable(FILE* file)
{
    HorizontalHeaderTable horizontalHeaderTable;
    horizontalHeaderTable.version = readU32(file);
    horizontalHeaderTable.ascent = readI16(file);
    horizontalHeaderTable.descent = readI16(file);
    horizontalHeaderTable.lineGap = readI16(file);
    horizontalHeaderTable.advanceWidthMax = readU16(file);
    horizontalHeaderTable.minLeftSideBearing = readI16(file);
    horizontalHeaderTable.minRightSideBearing = readI16(file);
    horizontalHeaderTable.xMaxExtent = readI16(file);
    horizontalHeaderTable.caretSlopeRise = readI16(file);
    horizontalHeaderTable.caretSlopeRun = readI16(file);
    horizontalHeaderTable.caretOffset = readI16(file);
    horizontalHeaderTable.reversed = readI16(file);
    horizontalHeaderTable.reversed1 = readI16(file);
    horizontalHeaderTable.reversed2 = readI16(file);
    horizontalHeaderTable.reversed4 = readI16(file);
    horizontalHeaderTable.metricDataFormat = readI16(file);
    horizontalHeaderTable.numOfLongHorMetrics = readU16(file);
    return horizontalHeaderTable;
}

typedef struct LongHorMetric
{
    u16 advanceWidth;
    i16 leftSideBearing;
} LongHorMetric;


static inline LongHorMetric readLongHorMetric(FILE* file)
{
    LongHorMetric longHorMetric;
    longHorMetric.advanceWidth = readU16(file);
    longHorMetric.leftSideBearing = readI16(file);
    return longHorMetric;
}

typedef struct CmapIndex
{
    u16 version;
    u16 numberSubtables;
} CmapIndex;

static inline CmapIndex readCmapIndex(FILE* file)
{
    CmapIndex cmapIndex;
    cmapIndex.version = readU16(file);
    cmapIndex.numberSubtables = readU16(file);
    return cmapIndex;
}

typedef struct CmapEncoding
{
    u16 platformID;
    u16 platformSpecificID;
    u32 offset;
} CmapEncoding;

static inline CmapEncoding readCmapEncoding(FILE* file)
{
    CmapEncoding cmapEncoding;
    cmapEncoding.platformID = readU16(file);
    cmapEncoding.platformSpecificID = readU16(file);
    cmapEncoding.offset = readU32(file);
    return cmapEncoding;
}

typedef struct CmapFormat4
{
    u16 length;
    u16 language;
    u16 segCountX2;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
} CmapFormat4;

static inline CmapFormat4 readCmapFormat4(FILE* file)
{
    CmapFormat4 cmap;
    cmap.length = readU16(file);
    cmap.language = readU16(file);
    cmap.segCountX2 = readU16(file);
    cmap.searchRange = readU16(file);
    cmap.entrySelector = readU16(file);
    cmap.rangeShift = readU16(file);
    return cmap;
}


#define MAX_TABLE_RECORDS 50
#define MAX_LONG_HOR_METRICS 2000
#define MAX_CMAP_ENCODINGS 10

typedef struct TTFData
{
    OffsetTable offsetTable;
    TableRecord tableRecords[MAX_TABLE_RECORDS];
    HorizontalHeaderTable horizontalHeaderTable;
    LongHorMetric longHorMetrics[MAX_LONG_HOR_METRICS];
    CmapIndex cmapIndex;
    CmapEncoding cmapEncodings[MAX_CMAP_ENCODINGS];
    CmapFormat4 cmap;
} TTFData;
