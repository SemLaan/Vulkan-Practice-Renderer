#include "font_loader.h"
#include "ttf_types.h"

#include "core/asserts.h"
#include <stdio.h>
#include <string.h>

#define FORMAT_4_MAX_SEGMENTS 200

void LoadFont(const char* filename)
{
    TTFData ttfData = {};

    FILE* file = fopen(filename, "rb");

    GRASSERT_MSG(file, "Font file failed to open");

    ttfData.offsetTable = readOffsetTable(file);

    GRASSERT(ttfData.offsetTable.numTables < MAX_TABLE_RECORDS);

    _DEBUG("tables: %i", ttfData.offsetTable.numTables);

    for (int i = 0; i < ttfData.offsetTable.numTables; i++)
    {
        ttfData.tableRecords[i] = readTableRecord(file);
        _DEBUG(ttfData.tableRecords[i].tag);

        i64 nextRecordStreamPos = ftell(file);

        if (0 == strncmp(ttfData.tableRecords[i].tag, "hhea", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));
            ttfData.horizontalHeaderTable = readHorizontalHeaderTable(file);
            GRASSERT(ttfData.horizontalHeaderTable.numOfLongHorMetrics < MAX_LONG_HOR_METRICS);
            _DEBUG("Entries: %i", ttfData.horizontalHeaderTable.numOfLongHorMetrics);
            _DEBUG("Max advance width: %u", ttfData.horizontalHeaderTable.advanceWidthMax);
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "loca", 4))
        {
            ttfData.glyphOffsetTableOffset = ttfData.tableRecords[i].offset;
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "glyf", 4))
        {
            ttfData.glyphTableOffset = ttfData.tableRecords[i].offset;
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "head", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));
            ttfData.fontHeaderTable = readFontHeaderTable(file);
            _DEBUG("indexToLocFormat: %i", ttfData.fontHeaderTable.indexToLocFormat);
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "hmtx", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));
            for (int j = 0; j < ttfData.horizontalHeaderTable.numOfLongHorMetrics; j++)
            {
                ttfData.longHorMetrics[j] = readLongHorMetric(file);
                //if (j % 20 == 0)
                    //_DEBUG("Advance width: %u", ttfData.longHorMetrics[j].advanceWidth);
            }
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "cmap", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));

            ttfData.cmapIndex = readCmapIndex(file);

            i64 nextCmapEncoding = ftell(file);

            bool foundSuitableFormat = false;

            for (int j = 0; j < ttfData.cmapIndex.numberSubtables; j++)
            {
                GRASSERT(0 == fseek(file, nextCmapEncoding, SEEK_SET));
                ttfData.cmapEncodings[j] = readCmapEncoding(file);
                nextCmapEncoding = ftell(file);
                _DEBUG("Platform ID: %u", ttfData.cmapEncodings[j].platformID);

                GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset + ttfData.cmapEncodings[j].offset, SEEK_SET));

                u16 format = readU16(file);
                _DEBUG("format: %u", format);

                if (format == 4)
                {
                    foundSuitableFormat = true;
                    ttfData.cmap = readCmapFormat4(file);

                    u32 segCount = ttfData.cmap.segCountX2 / 2;
                    _DEBUG("seg count: %u", segCount);
                    GRASSERT(segCount < FORMAT_4_MAX_SEGMENTS);
                    u16 endCode[FORMAT_4_MAX_SEGMENTS] = {};
                    u16 startCode[FORMAT_4_MAX_SEGMENTS] = {};
                    u16 idDelta[FORMAT_4_MAX_SEGMENTS] = {};
                    u16 idRangeOffset[FORMAT_4_MAX_SEGMENTS] = {};
                    readU16Array(file, endCode, segCount);
                    readU16(file); // Going past padding
                    readU16Array(file, startCode, segCount);
                    readU16Array(file, idDelta, segCount);
                    readU16Array(file, idRangeOffset, segCount);

                    // Find all indices for charcodes we care about
                    for (u32 charCode = 0; charCode < CHAR_COUNT; charCode++)
                    {
                        for (int segment = 0; segment < segCount; segment++)
                        {
                            if (charCode <= endCode[segment])
                            {
                                // This means the font doesn't have this character
                                if (charCode < startCode[segment])
                                {
                                    ttfData.cmapIndices[charCode] = 0;
                                    break;
                                }

                                if (idRangeOffset[segment] == 0)
                                {
                                    ttfData.cmapIndices[charCode] = (charCode + idDelta[segment]) % ID_DELTA_MOD;
                                }
                                else
                                {
                                    u32 glyphId = *(idRangeOffset[segment] / 2 + (charCode - startCode[segment]) + &idRangeOffset[segment]);
                                    if (glyphId == 0)
                                    {
                                        ttfData.cmapIndices[charCode] = 0;
                                    }
                                    else
                                    {
                                        ttfData.cmapIndices[charCode] = (glyphId + idDelta[segment]) % ID_DELTA_MOD;
                                    }
                                }

                                break;
                            }
                        }
                    }
                }
            }

            GRASSERT_MSG(foundSuitableFormat, "Only format 4 is supported");
        }

        GRASSERT(0 == fseek(file, nextRecordStreamPos, SEEK_SET));
    }


    for (u32 charCode = 0; charCode < CHAR_COUNT; charCode++)
    {
        i64 glyphOffset = ttfData.glyphTableOffset;
        if (ttfData.fontHeaderTable.indexToLocFormat == 1)
        {
            GRASSERT(0 == fseek(file, ttfData.glyphOffsetTableOffset + ttfData.cmapIndices[charCode] * sizeof(u32), SEEK_SET));
            glyphOffset += readU32(file);
        }
        else
        {
            GRASSERT(0 == fseek(file, ttfData.glyphOffsetTableOffset + ttfData.cmapIndices[charCode] * sizeof(u16), SEEK_SET));
            glyphOffset += readU16(file) * 2;
        }

        GRASSERT(0 == fseek(file, glyphOffset, SEEK_SET));
        
    }
}
