# Binary metadata archive documentation

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Overview

This document describes the binary format that is used to store Motion and Analytics metadata.

## General

There are two metadata file types:
- `motion_detailed_data.bin`/`motion_detailed_index.bin` - for Motion metadata.
- `analytics_detailed_data.bin`/`analytics_detailed_index.bin` - for Analytics metadata.

These files have the same binary format, but some fields are used for Analytics metadata only.
All numerical data in the files is stored in the little-endian format.

## Index file format

File name: `*index.bin`
The metadata binary index file consists of a serialized `indexHeader` structure,
followed by serialized `IndexRecord` entries. This file describes offsets and sizes
of records in the `*data.bin` file.

### IndexHeader structure:

| Name         | Bytes | Description                                                                                                                                               |
|--------------|-------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| startTimeMs  | 8     | Base UTC time in milliseconds used to calculate the full time of IndexRecord.                                                                                 |
| intervalMs   | 2     | The time interval used for grouping metadata into a single record.                                                                                        |
| version      | 1     | Metadata file version. The current documentation is for version 4.                                                                                               |
| recordSize   | 2     | Base size of each record in bytes.                                                                                                                    |
| flags        | 1     | 1-bit set of flags. Flags: `hasDiscontinue` (bit 0) - metadata in the file are not sorted; this flag is set if the time goes into the past while the file is being written. |
| wordSize     | 1     | Multiplier for the variable `extraWords`. The full record size in the main metadata file is calculated as: `baseRecordSize` + `extraWords` * `wordSize`.         |
| dummy        | 1     | Reserved for future use.                                                                                                                                  |
### IndexRecord structure:

| Name         | Bytes | Description                                                                                                                                               |
|--------------|-------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| start        | 4     | Relative time in milliseconds as an offset from the file header field `startTimeMs`.                                                                        |
| duration     | 2     | Record duration in milliseconds.                                                                                                                          |
| extraWords   | 1     | Addition record size in words. The word size is defined in `IndexHeader`. This field is used for Analytics data and is always zero for the Motion data.                     |
| recCount     | 1     | Number of records minus 1. If the value is bigger than 0, a single `IndexRecord` defines several records in the file `*data.bin`. Is always 0 for Motion metadata.   |
## Main metadata file format

File name: `*data.bin`
The main metadata file contains entries consisting of a one-bit binary grid and optional additional data. Grid data is a two-dimensional array of bits of size 44x32. The data in
the array is rotated by 90 degrees:
| 0  | 32 | .. | 1376 |
| 1  | 33 | .. | 1377 |
| 2  | 34 | .. | 1378 |
| .. | .. | .. | .... |
| 31 | 63 | .. | 1407 |
### Record format

| Name         | Bytes | Description                                                                                                                                                  |
|--------------|-------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| grid         | 176   | Binary 1-bit grid data of size 44x32 bits.                                                                                                                 |
| extraData    | 0..N  | Used for Analytics data only. The field size is `baseRecordSize` + `extraWords` * `wordSize` - 176 bytes. Not used for Motion metadata.                           |

### Extra data format for Analytics data

| Name              | Bytes | Description                                                                                                                                             |
|-------------------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| trackGroupId      | 4     | Reference to the `object_detection` SQL database. Table `track_group`, field `group_id`.                                                                |
| objectType        | 4     | Analytics Object type id (symbolic).                                                                                                                               |
| reserved          | 2     |                                                                                                                                                         |
| attributesHash    | 4     | Reference to the `object_detection` SQL database. Table `unique_attributes`, field `id`.                                                                |
| engineId          | 2     | Analytics Engine id (UUID).                                                                                                                                    |
| attributesHash2[] | 4 * N | Optional addition array of text Attribute ids, in the same format as for `attributesHash`. The number of records can be calculated from the size of the `extraData` field . |
