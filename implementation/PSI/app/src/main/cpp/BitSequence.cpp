#ifndef PSI_PROTOCOL_H
#define PSI_PROTOCOL_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <limits>

/**
 * @brief Unified BitSequence class that supports both "First K" and "Range [A, B]" initializations.
 */
template <typename T = uint64_t>
class BitSequence {
private:
    std::vector<T> blocks;
    size_t total_bits;

    static constexpr uint8_t BITS_PER_BLOCK = sizeof(T) * 8;

    // Helper to resize blocks based on n
    void resize_blocks(size_t n) {
        if (n == 0) return;
        size_t num_blocks = (n + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
        blocks.resize(num_blocks, 0);
    }

public:
    // Default constructor
    BitSequence() : total_bits(0) {}

    /**
     * @brief Constructor for "Less Than" mode: Sets first k bits to 1.
     * @param n Total number of bits
     * @param k Number of bits to set from the start
     */
    BitSequence(size_t n, size_t k) : total_bits(n) {
        resize_blocks(n);
        if (n == 0 || k == 0) return;
        if (k > n) k = n;

        size_t full_blocks = k / BITS_PER_BLOCK;
        for (size_t i = 0; i < full_blocks; ++i) {
            blocks[i] = std::numeric_limits<T>::max();
        }

        size_t remaining_bits = k % BITS_PER_BLOCK;
        if (remaining_bits > 0) {
            blocks[full_blocks] = (static_cast<T>(1) << remaining_bits) - 1;
        }
    }

    /**
     * @brief Constructor for "Equal Range" mode: Sets bits from a to b to 1.
     * @param a Start index (inclusive)
     * @param b End index (inclusive)
     * @param n Total number of bits
     */
    BitSequence(size_t a, size_t b, size_t n) : total_bits(n) {
        resize_blocks(n);
        if (n == 0) return;
        
        if (a > b) return; // Empty range
        if (b >= n) b = n - 1; // Clamp to n
        if (a >= n) return; // Start beyond n

        // Optimized block filling
        size_t start_block = a / BITS_PER_BLOCK;
        size_t end_block = b / BITS_PER_BLOCK;
        size_t start_offset = a % BITS_PER_BLOCK;
        size_t end_offset = b % BITS_PER_BLOCK;

        if (start_block == end_block) {
            // Range is within a single block
            T mask = ((static_cast<T>(1) << (end_offset - start_offset + 1)) - 1) << start_offset;
            blocks[start_block] |= mask;
        } else {
            // First partial block
            blocks[start_block] |= (~static_cast<T>(0)) << start_offset;

            // Full blocks in between
            for (size_t i = start_block + 1; i < end_block; ++i) {
                blocks[i] = std::numeric_limits<T>::max();
            }

            // Last partial block
            blocks[end_block] |= ((static_cast<T>(1) << (end_offset + 1)) - 1);
        }
    }

    bool get(size_t i) const {
        if (i >= total_bits) throw std::out_of_range("Index out of bounds");
        size_t block_idx = i / BITS_PER_BLOCK;
        size_t bit_pos = i % BITS_PER_BLOCK;
        return (blocks[block_idx] >> bit_pos) & static_cast<T>(1);
    }

    void set(size_t i) {
        if (i >= total_bits) throw std::out_of_range("Index out of bounds");
        size_t block_idx = i / BITS_PER_BLOCK;
        size_t bit_pos = i % BITS_PER_BLOCK;
        blocks[block_idx] |= (static_cast<T>(1) << bit_pos);
    }

    void clear(size_t i) {
        if (i >= total_bits) throw std::out_of_range("Index out of bounds");
        size_t block_idx = i / BITS_PER_BLOCK;
        size_t bit_pos = i % BITS_PER_BLOCK;
        blocks[block_idx] &= ~(static_cast<T>(1) << bit_pos);
    }

    void print_range(size_t start, size_t end) const {
        if (end > total_bits) end = total_bits;
        for (size_t i = start; i < end; ++i) {
            std::cout << get(i);
        }
        std::cout << std::endl;
    }

    const std::vector<T>& get_blocks() const { return blocks; }
    size_t num_blocks() const { return blocks.size(); }
    size_t size() const { return total_bits; }

    BitSequence& operator|=(const BitSequence& other) {
        if (this->total_bits != other.total_bits) throw std::invalid_argument("Size mismatch");
        for (size_t i = 0; i < blocks.size(); ++i) blocks[i] |= other.blocks[i];
        return *this;
    }

    BitSequence& operator&=(const BitSequence& other) {
        if (this->total_bits != other.total_bits) throw std::invalid_argument("Size mismatch");
        for (size_t i = 0; i < blocks.size(); ++i) blocks[i] &= other.blocks[i];
        return *this;
    }

    /**
     * @brief Returns a new BitSequence that is the 1's complement (bitwise NOT) of this sequence.
     */
    BitSequence ones_complement() const {
        BitSequence result = *this;
        for (size_t i = 0; i < result.blocks.size(); ++i) {
            result.blocks[i] = ~result.blocks[i];
        }

        // Mask out unset bits in the last block if total_bits is not a multiple of BITS_PER_BLOCK
        size_t remaining_bits = total_bits % BITS_PER_BLOCK;
        if (remaining_bits > 0) {
            size_t last_block_idx = result.blocks.size() - 1;
            T mask = (static_cast<T>(1) << remaining_bits) - 1;
            result.blocks[last_block_idx] &= mask;
        }

        return result;
    }

    /**
     * @brief Returns the bitwise OR of this sequence and another.
     * Named bitwise_or to avoid conflict with C++ 'or' keyword.
     */
    BitSequence bitwise_or(const BitSequence& other) const {
        if (this->total_bits != other.total_bits) throw std::invalid_argument("Size mismatch");
        BitSequence result = *this;
        result |= other;
        return result;
    }

    /**
     * @brief Returns the bitwise AND of this sequence and another.
     * Named bitwise_and to avoid conflict with C++ 'and' keyword.
     */
    BitSequence bitwise_and(const BitSequence& other) const {
        if (this->total_bits != other.total_bits) throw std::invalid_argument("Size mismatch");
        BitSequence result = *this;
        result &= other;
        return result;
    }
};

// Factory functions for convenience (and backward compatibility feeling if needed)

/**
 * @brief Factory for "Less Than" sequence.
 */
template <typename T = uint64_t>
BitSequence<T> less_than(size_t n, size_t k) {
    return BitSequence<T>(n, k);
}

/**
 * @brief Factory for "Equal Range" sequence.
 */
template <typename T = uint64_t>
BitSequence<T> equal_bits(size_t a, size_t b, size_t n) {
    return BitSequence<T>(a, b, n);
}

#endif
