#include "font_loader.h"
#include "ttf_types.h"

#include <stdio.h>
#include "core/asserts.h"
#include <string.h>


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

        i32 nextRecordStreamPos = ftell(file);

        if (0 == strncmp(ttfData.tableRecords[i].tag, "hhea", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));
            ttfData.horizontalHeaderTable = readHorizontalHeaderTable(file);
            GRASSERT(ttfData.horizontalHeaderTable.numOfLongHorMetrics < MAX_LONG_HOR_METRICS);
            _DEBUG("Entries: %i", ttfData.horizontalHeaderTable.numOfLongHorMetrics);
            _DEBUG("Max advance width: %u", ttfData.horizontalHeaderTable.advanceWidthMax);
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "hmtx", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));
            for (int j = 0; j < ttfData.horizontalHeaderTable.numOfLongHorMetrics; j++)
            {
                ttfData.longHorMetrics[j] = readLongHorMetric(file);
                if (j % 20 == 0)
                    _DEBUG("Advance width: %u", ttfData.longHorMetrics[j].advanceWidth);
            }
        }
        else if (0 == strncmp(ttfData.tableRecords[i].tag, "cmap", 4))
        {
            GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset, SEEK_SET));

            ttfData.cmapIndex = readCmapIndex(file);

            i32 nextCmapEncoding = ftell(file);

            for (int j = 0; j < ttfData.cmapIndex.numberSubtables; j++)
            {
                GRASSERT(0 == fseek(file, nextCmapEncoding, SEEK_SET));
                ttfData.cmapEncodings[j] = readCmapEncoding(file);
                nextCmapEncoding = ftell(file);
                _DEBUG("Platform ID: %u", ttfData.cmapEncodings[j].platformID);

                GRASSERT(0 == fseek(file, ttfData.tableRecords[i].offset + ttfData.cmapEncodings[j].offset, SEEK_SET));

                u16 format = readU16(file);
                _DEBUG("format: %u", format);

                GRASSERT_MSG(format == 4, "Only format 4 is supported");

                ttfData.cmap = readCmapFormat4(file);
            }
        }


        GRASSERT(0 == fseek(file, nextRecordStreamPos, SEEK_SET));
    }
}

