#include "font_loader.h"
#include "ttf_types.h"

#include "core/asserts.h"
#include "core/meminc.h"
#include "math/lin_alg.h"
#include <stdio.h>
#include <string.h>

#define FORMAT_4_MAX_SEGMENTS 200

GlyphData LoadFont(const char* filename)
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
                // if (j % 20 == 0)
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
                                    ttfData.glyphIndices[charCode] = 0;
                                    break;
                                }

                                if (idRangeOffset[segment] == 0)
                                {
                                    ttfData.glyphIndices[charCode] = (charCode + idDelta[segment]) % ID_DELTA_MOD;
                                }
                                else
                                {
                                    u32 glyphId = *(idRangeOffset[segment] / 2 + (charCode - startCode[segment]) + &idRangeOffset[segment]);
                                    if (glyphId == 0)
                                    {
                                        ttfData.glyphIndices[charCode] = 0;
                                    }
                                    else
                                    {
                                        ttfData.glyphIndices[charCode] = (glyphId + idDelta[segment]) % ID_DELTA_MOD;
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
        if (charCode != 'W')
            continue;

        i64 glyphOffset = ttfData.glyphTableOffset;
        if (ttfData.fontHeaderTable.indexToLocFormat == 1)
        {
            GRASSERT(0 == fseek(file, ttfData.glyphOffsetTableOffset + ttfData.glyphIndices[charCode] * sizeof(u32), SEEK_SET));
            glyphOffset += readU32(file);
        }
        else
        {
            GRASSERT(0 == fseek(file, ttfData.glyphOffsetTableOffset + ttfData.glyphIndices[charCode] * sizeof(u16), SEEK_SET));
            glyphOffset += readU16(file) * 2;
        }

        GRASSERT(0 == fseek(file, glyphOffset, SEEK_SET));

        GlyphHeader glyphHeader = readGlyphHeader(file);

        GRASSERT(glyphHeader.numberOfContours < MAX_CONTOURS);

        if (glyphHeader.numberOfContours >= 0) // if simple glyph
        {
            u16 endPtsOfContours[MAX_CONTOURS] = {};
            readU16Array(file, endPtsOfContours, glyphHeader.numberOfContours);
            u32 totalPoints = endPtsOfContours[glyphHeader.numberOfContours - 1] + 1;
            GRASSERT(totalPoints < MAX_POINTS);

            u16 instructionLength = readU16(file);
            GRASSERT(0 == fseek(file, instructionLength, SEEK_CUR));

            _DEBUG("Countour count: %i, Point count: %u", glyphHeader.numberOfContours, totalPoints);
            for (int d = 0; d < glyphHeader.numberOfContours; d++)
                _DEBUG("Contour end %i: %u", d, endPtsOfContours[d]);

            u8 processedFlags[MAX_POINTS] = {};

            u32 flagIndex = 0;
            while (flagIndex < totalPoints)
            {
                u8 currentFlag = readU8(file);
                processedFlags[flagIndex] = currentFlag;
                flagIndex++;

                if (currentFlag & POINT_FLAG_REPEAT_FLAG)
                {
                    u8 repetitionCount = readU8(file);
                    for (int repetition = 0; repetition < repetitionCount; repetition++)
                    {
                        processedFlags[flagIndex] = currentFlag;
                        flagIndex++;
                    }
                }
            }

            GRASSERT(flagIndex == totalPoints);

            vec2* resultPositions = Alloc(GetGlobalAllocator(), sizeof(*resultPositions) * totalPoints, MEM_TAG_LOGGING_SUBSYS);

            // Reading x coordinates
            i32 relativePosition = 0;
            for (int pointIndex = 0; pointIndex < totalPoints; pointIndex++)
            {
                if (processedFlags[pointIndex] & POINT_FLAG_X_SHORT_VECTOR)
                {
                    i32 xOffset = readU8(file);
                    if (processedFlags[pointIndex] & POINT_FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)
                        relativePosition += xOffset;
                    else
                        relativePosition -= xOffset;
                }
                else if (0 == (processedFlags[pointIndex] & POINT_FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR))
                {
                    relativePosition += readI16(file);
                }

                resultPositions[pointIndex].x = (f32)relativePosition / (f32)ttfData.fontHeaderTable.unitsPerEm;
            }

            // Reading y coordinates
            relativePosition = 0;
            for (int pointIndex = 0; pointIndex < totalPoints; pointIndex++)
            {
                if (processedFlags[pointIndex] & POINT_FLAG_Y_SHORT_VECTOR)
                {
                    i32 yOffset = readU8(file);
                    if (processedFlags[pointIndex] & POINT_FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)
                        relativePosition += yOffset;
                    else
                        relativePosition -= yOffset;
                }
                else if (0 == (processedFlags[pointIndex] & POINT_FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR))
                {
                    relativePosition += readI16(file);
                }

                resultPositions[pointIndex].y = (f32)relativePosition / (f32)ttfData.fontHeaderTable.unitsPerEm;
            }

            // Debug printing
            u32 contourIndex = 0;
            for (int pointIndex = 0; pointIndex < totalPoints; pointIndex++)
            {
                _DEBUG("x: %f, y: %f", resultPositions[pointIndex].x, resultPositions[pointIndex].y);
                if (endPtsOfContours[contourIndex] == pointIndex)
                {
                    _DEBUG("End of contour");
                    contourIndex++;
                }
            }

            GlyphData glyphData = {};
            glyphData.pointCount = totalPoints;
            glyphData.points = resultPositions;

            return glyphData;
        }
        else    // if complex glyph
        {

        }
    }
}
