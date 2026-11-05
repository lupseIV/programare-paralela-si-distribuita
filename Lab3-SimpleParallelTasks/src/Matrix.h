#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class Matrix {
public:
    Matrix(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols), data_(rows * cols, 0) {}

    [[nodiscard]] std::size_t rows() const { return rows_; }
    [[nodiscard]] std::size_t cols() const { return cols_; }

    std::int64_t& operator()(std::size_t i, std::size_t j) { return data_[i * cols_ + j]; }
    std::int64_t operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }

    bool operator==(const Matrix&) const = default;

private:
    std::size_t rows_;
    std::size_t cols_;
    std::vector<std::int64_t> data_;
};
