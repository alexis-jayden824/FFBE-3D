#include "content/pack.h"
#include <cstring>

namespace f3d {

static uint32_t rd(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

bool Pack::open(const uint8_t* data, size_t size) {
    data_ = nullptr; count_ = 0;
    if (!data || size < 12 || std::memcmp(data, "F3DP", 4) != 0 || rd(data + 4) != 1) return false;
    uint32_t n = rd(data + 8);
    if (n > (size - 12) / 20) return false;
    // Validate every table's extents once so accessors can stay unchecked.
    for (uint32_t t = 0; t < n; ++t) {
        const uint8_t* h = data + 12 + t * 20;
        uint64_t name = rd(h), cols = rd(h + 4), rows = rd(h + 8), coff = rd(h + 12), roff = rd(h + 16);
        if (name >= size) return false;
        if (coff + cols * 8 > size) return false;
        if (roff + rows * cols * 4 > size) return false;
    }
    if (data[size - 1] != 0) return false;   // string area must be terminated
    data_ = data; size_ = size; count_ = n;
    return true;
}

Table Pack::table(uint32_t index) const {
    Table t;
    if (!data_ || index >= count_) return t;
    const uint8_t* h = data_ + 12 + index * 20;
    t.base_ = data_;
    t.name_off_ = rd(h); t.col_count_ = rd(h + 4); t.row_count_ = rd(h + 8);
    t.cols_off_ = rd(h + 12); t.rows_off_ = rd(h + 16);
    return t;
}

Table Pack::table(const char* name) const {
    for (uint32_t i = 0; i < count_; ++i) {
        Table t = table(i);
        if (std::strcmp(t.name(), name) == 0) return t;
    }
    return Table();
}

const char* Table::name() const { return reinterpret_cast<const char*>(base_ + name_off_); }

int Table::column(const char* name) const {
    for (uint32_t c = 0; c < col_count_; ++c)
        if (std::strcmp(reinterpret_cast<const char*>(base_ + rd(base_ + cols_off_ + c * 8)), name) == 0)
            return int(c);
    return -1;
}

bool Table::is_string(uint32_t col) const { return rd(base_ + cols_off_ + col * 8 + 4) == 1; }

int32_t Table::i(uint32_t row, uint32_t col) const {
    return int32_t(rd(base_ + rows_off_ + (uint64_t(row) * col_count_ + col) * 4));
}

const char* Table::s(uint32_t row, uint32_t col) const {
    return reinterpret_cast<const char*>(base_ + uint32_t(i(row, col)));
}

int Table::find(int32_t key) const {
    uint32_t lo = 0, hi = row_count_;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        int32_t v = i(mid, 0);
        if (v == key) return int(mid);
        if (v < key) lo = mid + 1; else hi = mid;
    }
    return -1;
}

} // namespace f3d
