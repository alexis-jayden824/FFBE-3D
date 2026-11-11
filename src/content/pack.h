// Reader for .f3dp table packs produced by tools/convert/mstpack.py.
// Zero-copy: the pack is one blob loaded from SD (or romfs) and read in place,
// which matters with ~124 MB of application memory.
//
// Layout (little endian):
//   "F3DP" u32 version(1) u32 table_count
//   per table: u32 name_off, u32 col_count, u32 row_count, u32 cols_off, u32 rows_off
//   cols: per column  u32 name_off, u32 type (0 = i32, 1 = string)
//   rows: row_count * col_count * 4 bytes (i32 value, or string offset)
//   strings: NUL terminated UTF-8, offsets from start of blob
// Rows are sorted by column 0 when it is an integer, so find() is a binary search.
#pragma once
#include <cstddef>
#include <cstdint>

namespace f3d {

class Table {
public:
    bool valid() const { return base_ != nullptr; }
    const char* name() const;
    uint32_t rows() const { return row_count_; }
    uint32_t cols() const { return col_count_; }
    int column(const char* name) const;          // -1 if missing
    bool is_string(uint32_t col) const;
    int32_t i(uint32_t row, uint32_t col) const;
    const char* s(uint32_t row, uint32_t col) const;
    int find(int32_t key) const;                 // row index by column 0, -1 if missing

private:
    friend class Pack;
    const uint8_t* base_ = nullptr;
    uint32_t name_off_ = 0, col_count_ = 0, row_count_ = 0, cols_off_ = 0, rows_off_ = 0;
};

class Pack {
public:
    // Returns false if the blob is not a well-formed pack. Does not copy.
    bool open(const uint8_t* data, size_t size);
    uint32_t table_count() const { return count_; }
    Table table(uint32_t index) const;
    Table table(const char* name) const;

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    uint32_t count_ = 0;
};

} // namespace f3d
