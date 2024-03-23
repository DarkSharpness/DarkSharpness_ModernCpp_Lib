#pragma once
#include <bit>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <exception>
#include "allocator.h"

namespace dark {


struct dynamic_bitset;


namespace __detail::__bitset {

/* Word used in bitset. */
using _Word_t = std::size_t;

/* Word bits. */
inline constexpr std::size_t __WBits = sizeof(_Word_t) * CHAR_BIT;

/* Set __n-th bit to 1.  */
inline constexpr _Word_t
mask_pos(std::size_t __n) { return _Word_t{1} << __n; }

/* Set first n low bits to 1, others to 0. */
inline constexpr _Word_t
mask_low(std::size_t __n) { return (_Word_t{1} << __n) - 1; }

/* Set first n low bits to 0, others to 1. */
inline constexpr _Word_t
mask_top(std::size_t __n) { return (~_Word_t{0}) << __n; }

/* Allocate a sequence of zero memory. */
inline constexpr _Word_t *
alloc_zero(std::size_t __n) { return allocator<_Word_t>::calloc(__n); }

/* Allocate a sequence of raw memory. */
inline constexpr _Word_t *
alloc_none(std::size_t __n) { return allocator<_Word_t>::allocate(__n); }

/* Deallocate memory. */
inline constexpr void deallocate(_Word_t *__ptr, std::size_t __n)
{ allocator<_Word_t>::deallocate(__ptr, __n); }

/* Copy __n words from __src to __dst. */
inline constexpr void word_copy
    (_Word_t *__dst, const _Word_t *__src, std::size_t __n) {
    if (std::is_constant_evaluated()) {
        for (std::size_t i = 0 ; i != __n ; ++i)
            __dst[i] = __src[i];
    } else {
        std::memcpy(__dst, __src, __n * sizeof(_Word_t));
    }
}

/* Return the quotient and remainder of __n , 64 */
inline constexpr auto div_mod(_Word_t __n) {
    struct {
        _Word_t div;
        _Word_t mod;
    } __ret = {__n / __WBits, __n % __WBits};
    return __ret;
}

/* Return ceiling of __n / 64 */
inline constexpr _Word_t div_ceil(_Word_t __n) {
    return (__n + __WBits - 1) / __WBits;
}

/* Return ceiling of __n / 64 - 1, last available word. */
inline constexpr _Word_t div_down(_Word_t __n) {
    return (__n - 1) / __WBits;
}

/* Return 64 - __n, where __n is less than 64 */
inline constexpr _Word_t rev_bits(_Word_t __n) {
    return ((__WBits - 1) ^ __n) + 1;
}

/* Make the last word valid. */
inline constexpr void validate(_Word_t *__dst, std::size_t __n) {
    const auto [__div, __mod] = div_mod(__n);
    if (__mod != 0) __dst[__div] &= mask_low(__mod);
}

/* Custom bit manipulator. */
struct reference {
  private:
    _Word_t *   ptr;        // Pointer to the word
    std::size_t msk;        // Mask word of the bit

    friend class ::dark::dynamic_bitset;

    /* ctor */
    constexpr reference(_Word_t *__ptr, std::size_t __pos)
    noexcept : ptr(__ptr), msk(mask_pos(__pos)) {}

  public:
    /* Convert to bool. */
    constexpr operator bool() const { return *ptr & msk; }

    /* Set current bit to 1. */
    constexpr void set()    { *ptr |= msk; }
    /* Set current bit to 0. */
    constexpr void reset()  { *ptr &= ~msk; }
    /* Flip current bit. */
    constexpr void flip()   { *ptr ^= msk; }

    /* Assign value to current bit. */
    constexpr bool
    operator = (bool val) { val ? set() : reset(); return val; }
    /* Assign value to current bit. */
    constexpr bool
    operator = (const reference &rhs) { return *this = bool(rhs); }
};

/* Custom bit vector. */
struct dynamic_storage {
  private:
    _Word_t *   head;   // Pointer to the first word
    std::size_t buffer; // Buffer size
  protected:
    std::size_t length; // Real length of the bitset

    /* Reallocate memory. */
    constexpr void
    realloc(std::size_t __n) { head = alloc_none(__n); buffer = __n; }

    /* Deallocate memory. */
    constexpr void dealloc() { deallocate(head, buffer); }

    /* Deallocate memory. */
    constexpr static void dealloc(_Word_t *__ptr, std::size_t __n) {
        deallocate(__ptr, __n);
    }

    /* Reset the storage. */
    constexpr void reset() { head = nullptr; buffer = 0; length = 0; }

  public:
    /* ctor & operator section. */

    constexpr ~dynamic_storage()  noexcept { this->dealloc(); }
    constexpr dynamic_storage()   noexcept { this->reset();   }

    constexpr dynamic_storage(std::size_t __n) {
        head = alloc_none(__n);
        buffer = length = __n;
    }

    constexpr dynamic_storage(std::size_t __n, std::nullptr_t) {
        head = alloc_zero(__n);
        buffer = __n;
        length = div_ceil(__n) * __WBits;
    }

    constexpr dynamic_storage(const dynamic_storage &rhs)
        : dynamic_storage(rhs.length) {
        word_copy(head, rhs.head, rhs.word_count());
    }

    constexpr dynamic_storage(dynamic_storage &&rhs) noexcept {
        head   = rhs.head;
        buffer = rhs.buffer;
        length = rhs.length;
        rhs.reset();
    }

    constexpr dynamic_storage &operator = (const dynamic_storage &rhs) {
        if (this == &rhs) return *this;
        if (this->capacity() < rhs.word_count()){
            this->dealloc();
            this->realloc(rhs.length);
            length = rhs.length;
        }
        word_copy(head, rhs.head, rhs.word_count());
        return *this;
    }

    constexpr dynamic_storage &operator = (dynamic_storage &&rhs)
    noexcept { return this->swap(rhs); }

  public:
    /* Function section. */

    /* Return the real word in the bitmap */
    constexpr std::size_t word_count() const { return div_ceil(length); }
    /* Return the capacity of the storage. */
    constexpr std::size_t capacity()   const { return buffer; }

    constexpr dynamic_storage &swap(dynamic_storage &rhs) {
        std::swap(head, rhs.head);
        std::swap(buffer, rhs.buffer);
        std::swap(length, rhs.length);
        return *this;
    }

    constexpr _Word_t *data() const { return head; }
    constexpr _Word_t  data(std::size_t __n) const { return head[__n]; }
    constexpr _Word_t &data(std::size_t __n)       { return head[__n]; }

    /* Grow the size by one, and fill with given value in the back. */
    constexpr void grow_full(bool __val) {
        const auto __size = length / __WBits;
        const auto __capa = this->capacity();
        if (__size == __capa) {
            auto *__temp = head;
            this->realloc(__capa << 1 | !__capa);
            word_copy(head, __temp, __capa);
            this->dealloc(__temp, __capa);
        }
        data(__size) = __val;
    }

    /* Pop one element. */
    constexpr void pop_back() noexcept {
        __detail::__bitset::validate(this->data(), --length);
    }

    /* Clear to empty. */
    constexpr void clear() noexcept { length = 0; }
};

inline constexpr void
do_and(_Word_t *__dst, const _Word_t *__rhs, std::size_t __n) {
    const auto [__div, __mod] = div_mod(__n);
    for (std::size_t i = 0; i != __div; ++i)
        __dst[i] &= __rhs[i];
    if (__mod != 0)
        __dst[__div] &= __rhs[__div] | mask_top(__mod);
}

inline constexpr void
do_or_(_Word_t *__dst, const _Word_t *__rhs, std::size_t __n) {
    const auto [__div, __mod] = div_mod(__n);
    for (std::size_t i = 0; i != __div; ++i)
        __dst[i] |= __rhs[i];
    if (__mod != 0)
        __dst[__div] |= __rhs[__div] & mask_low(__mod);
}

inline constexpr void
do_xor(_Word_t *__dst, const _Word_t *__rhs, std::size_t __n) {
    const auto [__div, __mod] = div_mod(__n);
    for (std::size_t i = 0; i != __div; ++i)
        __dst[i] ^= __rhs[i];
    if (__mod != 0)
        __dst[__div] ^= __rhs[__div] & mask_low(__mod);
}

static_assert(std::endian::native == std::endian::little,
    "Our implement only supports little endian now.");

/* 2-pointer small struct. */
struct vec2 { _Word_t *dst; const _Word_t *src; };

inline constexpr void
byte_lshift(vec2 __vec, std::size_t __n, std::size_t __count) {
    if (__count == 0) return;
    auto [__dst, __src] = __vec;

    const auto __size = sizeof(_Word_t);              // Word size.
    const auto __tail = (__n + __size - 1) / __size;  // Byte last.

    auto *__raw = reinterpret_cast <std::byte *> (__dst);
    std::memmove(__raw + __count, __src, __tail - __count);
    std::memset(__raw, 0, __count);
}

inline constexpr void
byte_rshift(vec2 __vec, std::size_t __n, std::size_t __count) {
    if (__count == 0) return;
    auto [__dst, __src] = __vec;

    const auto __size = sizeof(_Word_t);              // Word size.
    const auto __tail = (__n + __size - 1) / __size;  // Byte last.

    auto *__raw = reinterpret_cast <const std::byte *> (__src);
    std::memmove(__dst, __raw + __count, __tail);
}

/* Perform left shift operation without validation. */
inline constexpr void
do_lshift(vec2 __vec, std::size_t __n, std::size_t __shift) {
    if (__shift % CHAR_BIT == 0)
        return byte_lshift(__vec, __n, __shift / CHAR_BIT);

    const auto [__div, __mod] = div_mod(__shift);
    const auto [__dst, __src] = __vec;

    const auto __rev = rev_bits(__mod); // 64 - __mod
    const auto __len = div_down(__n);

    _Word_t __pre = __src[__len - __div];

    for (std::size_t i = __len ; i != __div ; --i) {
        _Word_t __cur = __src[i - 1 - __div];
        __dst[i] = __pre << __mod | __cur >> __rev;
        __pre = __cur; // Update the previous word.
    }

    __dst[__div] = __pre << __mod;
    std::memset(__dst, 0, __div * sizeof(_Word_t));
}

/* Perform right shift operation without validation. */
inline constexpr void
do_rshift(vec2 __vec, std::size_t __n, std::size_t __shift) {
    if (__shift % CHAR_BIT == 0)
        return byte_rshift(__vec, __n, __shift / CHAR_BIT);

    const auto [__dst, __src] = __vec;
    const auto [__div, __mod] = div_mod(__shift);

    const auto __rev = (__mod ^ (__WBits - 1)) + 1; // _WBits - __mod
    const auto __len = div_down(__n + __shift) - __div;
    _Word_t __pre = __src[__div];

    for (std::size_t i = 0 ; i != __len ; ++i) {
        _Word_t __cur = __src[i + 1 + __div];
        __dst[i] = __pre >> __mod | __cur << __rev;
        __pre = __cur; // Update the previous word.
    }

    __dst[__len] = __pre >> __mod;
}


} // namespace __detail::__bitset


struct dynamic_bitset : private __detail::__bitset::dynamic_storage {
  public:
    using _Bitset   = dynamic_bitset;
    using reference = __detail::__bitset::reference;

    inline static constexpr std::size_t npos = -1;

  private:
    using _Base_t = __detail::__bitset::dynamic_storage;
    using _Word_t = __detail::__bitset::_Word_t;

    constexpr static _Word_t min(_Word_t __x, _Word_t __y) { return __x < __y ? __x : __y; }
  public:
    /* ctor and operator section. */

    constexpr dynamic_bitset() = default;
    constexpr ~dynamic_bitset() = default;

    constexpr dynamic_bitset(const dynamic_bitset &) = default;
    constexpr dynamic_bitset(dynamic_bitset &&) noexcept = default;

    constexpr dynamic_bitset &operator = (const dynamic_bitset &) = default;
    constexpr dynamic_bitset &operator = (dynamic_bitset &&) noexcept = default;

    constexpr dynamic_bitset(std::size_t __n) : _Base_t(__n, nullptr) {}

    constexpr dynamic_bitset(std::size_t __n, bool __x) : _Base_t(__n) {
        std::memset(this->data(), -__x, this->word_count() * sizeof(_Word_t));
        if (__x) __detail::__bitset::validate(this->data(), length);
    }

    constexpr _Bitset &operator |= (const _Bitset &__rhs) {
        const auto __min = this->min(length, __rhs.length);
        __detail::__bitset::do_or_(this->data(), __rhs.data(), __min);
        return *this;
    }

    constexpr _Bitset &operator &= (const _Bitset &__rhs) {
        const auto __min = this->min(length, __rhs.length);
        __detail::__bitset::do_and(this->data(), __rhs.data(), __min);
        return *this;
    }

    constexpr _Bitset &operator ^= (const _Bitset &__rhs) {
        const auto __min = this->min(length, __rhs.length);
        __detail::__bitset::do_xor(this->data(), __rhs.data(), __min);
        return *this;
    }

    constexpr _Bitset &operator <<= (std::size_t __n) {
        if (!length) return this->assign(__n, 0), *this;
        length += __n;

        const auto __size = this->word_count();
        const auto __capa = this->capacity();
        const auto __head = this->data();

        /* This is a nice estimation of next potential size.*/
        /* It will be larger, but that too much bigger.     */
        if (__capa < __size) this->realloc(__size + __capa);

        const auto __data = this->data();
        __detail::__bitset::do_lshift({__data, __head}, length, __n);
        __detail::__bitset::validate(__data, length);

        /* If realloc, deallocate the old memory. */
        if (__head != __data) this->dealloc(__head, __capa);
        return *this;
    }

    constexpr _Bitset &operator >>= (std::size_t __n) {
        if (__n < length) {
            length -= __n;
            const auto __data = this->data();
            __detail::__bitset::do_rshift({__data, __data}, length, __n);
            __detail::__bitset::validate(__data, length);
        } else this->clear();
        return *this;
    }

    constexpr _Bitset operator ~() const;

  public:
    /* Section of member functions that won't bring size changes. */

    constexpr _Bitset &set() {
        const auto __size = this->word_count();
        std::memset(this->data(), -1, __size * sizeof(_Word_t));
        __detail::__bitset::validate(this->data(), length);
        return *this;
    }

    constexpr _Bitset &flip();
    constexpr _Bitset &reset();

    /* Return whether there is any bit set to 1. */
    constexpr bool any() const { return !this->none(); }
    /* Return whether all bits are set to 1. */
    constexpr bool all() const {
        auto [__div, __mod] = __detail::__bitset::div_mod(length);
        for (std::size_t i = 0 ; i != __div ; ++i)
            if (~data(i) != 0) return false;
        return data(__div) == __detail::__bitset::mask_low(__mod);
    }
    /* Return whether all bits are set to 0. */
    constexpr bool none() const {
        auto __top = this->word_count();
        for (std::size_t i = 0 ; i != __top ; ++i)
            if (data(i) != 0) return false;
        return true;
    }

    /* Return the number of bits set to 1. */
    constexpr std::size_t count() const {
        std::size_t __cnt = 0;
        auto __top = this->word_count();
        for (std::size_t i = 0 ; i != __top ; ++i)
            __cnt += std::popcount(data(i));
        return __cnt;
    }

    constexpr void set(std::size_t __n)       { (*this)[__n].set();     }
    constexpr void reset(std::size_t __n)     { (*this)[__n].reset();   }
    constexpr void flip(std::size_t __n)      { (*this)[__n].flip();    }

    constexpr bool test(std::size_t __n) const {
        auto [__div, __mod] = __detail::__bitset::div_mod(__n);
        return (data(__div) >> __mod) & 1;
    }

    constexpr std::size_t size()  const { return length; }

    constexpr reference operator [] (std::size_t __n) {
        auto [__div, __mod] = __detail::__bitset::div_mod(__n);
        return reference(data() + __div, __mod);
    }
    constexpr reference at(std::size_t __n) { this->range_check(__n); return (*this)[__n]; }
    constexpr bool operator [] (std::size_t __n) const { return test(__n); }
    constexpr bool at(std::size_t __n) const { range_check(__n); return test(__n); }

    constexpr reference front() { return (*this)[0]; }
    constexpr reference back()  { return (*this)[length - 1]; }
    constexpr bool front() const { return test(0); }
    constexpr bool back()  const { return test(length - 1); }

    constexpr std::size_t find_first();
    constexpr std::size_t find_next(std::size_t);

  public:
    /* Section of member functions that may bring size changes. */

    constexpr void push_back(bool __x) {
        using namespace __detail::__bitset;
        if (const auto __mod = length++ % __WBits) {
            data(div_down(length)) |= __x << (length % __WBits - 1);
        } else { // Full word, so grow the storage by 1.
            this->grow_full(__x);
        }
    }

    constexpr void pop_back() noexcept { return _Base_t::pop_back(); }
    constexpr void clear()    noexcept { return _Base_t::clear();    }

    constexpr void assign(std::size_t __n, bool __x) {
        length = __n;

        const auto __size = this->word_count();
        const auto __capa = this->capacity();

        if (__capa < __size) {
            this->dealloc();
            this->realloc(__size + __capa);
        }

        const auto __data = this->data();
        std::memset(__data, -__x, __size * sizeof(_Word_t));

        // Validate the last word for the last bits.
        if (__x) __detail::__bitset::validate(__data, length);
    }

  public:
    void debug() {
        using namespace __detail::__bitset;
        const auto [__div, __mod] = div_mod(length);
        for (std::size_t i = 0 ; i != __div ; ++i) {
            auto __str = std::bitset <__WBits> (data(i)).to_string();
            std::reverse(__str.begin(), __str.end());
            std::cout << __str << '\n';
        }
        if (__mod != 0) {
            auto __str = std::bitset <__WBits> (data(__div)).to_string();
            std::reverse(__str.begin(), __str.end());
            for (std::size_t i = __mod ; i != __WBits ; ++i)
                if (__str[i] != '0') throw std::runtime_error("Invalid bitset.");
                else __str[i] = '-';
            std::cout << __str << '\n';
        }
    }

    constexpr void range_check(std::size_t __n) const {
        if (__n >= length)
            throw std::out_of_range("dynamic_bitset::range_check");
    }
};


} // namespace dark