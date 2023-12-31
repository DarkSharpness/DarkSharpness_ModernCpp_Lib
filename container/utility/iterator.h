#pragma once

#include "common.h"

#include <version>
#include <iterator>
#include <concepts>
#include <compare>

namespace dark {

namespace iterator_base {

template <class _Traits, class _Tp, bool _Dir>
concept advanceable =
    (_Dir == true   && requires (_Tp *__ptr) { _Traits::advance(__ptr);     })
||  (_Dir == false  && requires (_Tp *__ptr) { _Traits::backtrace(__ptr);   });

template <class _Traits, class _Tp, bool _Dir>
concept random_accessible =
    (_Dir == true   && requires (_Tp *__ptr,std::ptrdiff_t __n) { _Traits::advance(__ptr, __n);     })
||  (_Dir == false  && requires (_Tp *__ptr,std::ptrdiff_t __n) { _Traits::backtrace(__ptr, __n);   });

template <class _Traits, class _Tp>
concept diffable = requires (_Tp *__lhs, _Tp *__rhs) {
    { _Traits::difference(__lhs,__rhs) } -> std::same_as <typename _Traits::difference_type>;
};

template <class _Traits, class _Tp>
concept comparable = requires (_Tp *__lhs, _Tp *__rhs) {
    { _Traits::compare(__lhs,__rhs) } -> std::same_as <typename _Traits::compare_type>;
};

} // namespace iterator_base


namespace helper {


/**
 * @brief A helper struct for iterator.
 * It works as a wrapper of the iterator traits.
 */
template <class _Traits, bool _Dir, class _Tp>
requires std::same_as <typename _Traits::node_type, std::decay_t <_Tp>>
inline static void advance_pointer(_Tp *&__ptr) noexcept {
    if constexpr (_Dir == false) {
        _Traits::backtrace(__ptr);
    } else {
        _Traits::advance(__ptr);
    }
}


template <class _Traits, bool _Dir, class _Tp>
requires std::same_as <typename _Traits::node_type, std::decay_t <_Tp>>
inline static void advance_pointer(_Tp *&__ptr, std::ptrdiff_t __n) noexcept {
    if constexpr (_Dir == false) {
        _Traits::backtrace(__ptr, __n);
    } else {
        _Traits::advance(__ptr, __n);
    }
}


/**
 * @brief A normal pointer as iterator.
 * Implement the basic iterator traits.
 * 
 */
struct normal_pointer {
    template <class _Tp>
    static void advance(_Tp *&__ptr, std::ptrdiff_t __n = 1) noexcept { __ptr += __n; }
    template <class _Tp>
    static void backtrace(_Tp *&__ptr, std::ptrdiff_t __n = 1) noexcept { __ptr -= __n; }
    template <class _Tp>
    static auto difference(_Tp *__lhs, _Tp *__rhs) noexcept { return __lhs - __rhs; }
    template <class _Tp>
    static _Tp &dereference(_Tp *__ptr) noexcept { return *__ptr; }
    template <class _Tp>
    static auto compare(_Tp *__lhs, _Tp *__rhs) noexcept { return __lhs <=> __rhs; }
};


} // namespace helper


/**
 * @brief Wrapper of basic type pointer as iterator.
 * 
 * @tparam _Traits Iterator traits.
 * @tparam _Const  True for const_iterator, false for non_const_iterator.
 * @tparam _Dir    True for forward iterator, false for backward iterator.
 */
template <class _Traits, bool _Const, bool _Dir>
struct basic_iterator {
  protected:
    /* Inner data. */

    using _Node_Type = typename _Traits::node_type;
    using _Data_Type = std::conditional_t <_Const,const _Node_Type, _Node_Type>;
    _Data_Type *node;

  public:
    using mutable_iterator  = basic_iterator <_Traits, false,_Dir>;
    using reverse_iterator  = basic_iterator <_Traits,_Const,!_Dir>;

    using iterator_category = typename _Traits::iterator_category;
    using difference_type   = typename _Traits::difference_type;
    using compare_type      = typename _Traits::compare_type;

    using value_type        = std::conditional_t <_Const,
        const typename _Traits::value_type, typename _Traits::value_type>;
    using pointer           = value_type *;
    using reference         = value_type &;

  public:
    /* Construct from nothing. */
    basic_iterator() noexcept : node() {}

    /**
     * @brief Construct the iterator from a pointer explicitly.
     * @param __ptr Pointer of the inner data.
     */
    explicit basic_iterator(_Data_Type *__ptr) noexcept : node(__ptr) {}

    /**
     * @brief Construct from an non_const_iterator to a const_iterator.
     * Of course, the reverse is not allowed.
     */
    template <void * = nullptr> requires (_Const == true)
    basic_iterator(mutable_iterator __rhs)
    noexcept : node(__rhs.base()) {}

    basic_iterator(const basic_iterator &) noexcept = default;
    basic_iterator &operator = (const basic_iterator &) noexcept = default;

    reference operator  *() const noexcept { return _Traits::dereference(node); }
    pointer   operator ->() const noexcept { return   std::addressof(*this);    }

    template <void * = nullptr>
    requires iterator_base::advanceable <_Traits,_Data_Type,_Dir>
    basic_iterator &operator ++() noexcept {
        helper::advance_pointer <_Traits,_Dir> (node);
        return *this;
    }

    template <void * = nullptr>
    requires iterator_base::advanceable <_Traits,_Data_Type,!_Dir>
    basic_iterator &operator --() noexcept {
        helper::advance_pointer <_Traits,!_Dir> (node);
        return *this;
    }

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,_Dir>
    basic_iterator &operator += (difference_type __n) noexcept {
        helper::advance_pointer <_Traits,_Dir> (node, __n);
        return *this;
    }

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,!_Dir>
    basic_iterator &operator -= (difference_type __n) noexcept {
        helper::advance_pointer <_Traits,!_Dir> (node, __n);
        return *this;
    }

    template <void * = nullptr>
    requires iterator_base::diffable <_Traits,_Data_Type>
    friend difference_type operator -
        (basic_iterator __lhs, basic_iterator __rhs) noexcept {
        return _Traits::difference(__lhs.node, __rhs.node);
    }

    /**
     * @brief Judge whether 2 iterators point to the same data.
     * @param __lhs Left  hand side.
     * @param __rhs Right hand side.
     * @return True iff 2 iterators point to the same data.
     */
    friend bool operator == (basic_iterator __lhs, basic_iterator __rhs) noexcept {
        return __lhs.node == __rhs.node;
    }

    /**
     * @brief Three-way comparison for random access iterator.
     * @param __lhs Left  hand side.
     * @param __rhs Right hand side.
     * @return The result of comparison.
     */
    template <void * = nullptr>
    requires iterator_base::comparable <_Traits,_Data_Type>
    friend auto operator <=> (basic_iterator __lhs, basic_iterator __rhs) noexcept {
        if constexpr (_Dir == false) {
            return _Traits::compare(__rhs.node, __lhs.node);
        } else {
            return _Traits::compare(__lhs.node, __rhs.node);
        }
    }

    /**
     * @return Inner data pointer.
     */
    _Data_Type *base() const noexcept { return node; }

    /**
     * @return Reverse iterator pointing to the same data.
     */
    reverse_iterator reverse() const noexcept {
        return reverse_iterator { node };
    }

    /**
     * @return Non-const iterator pointing to the same data.
     * @attention Be aware of what you are doing before abusing this function.
     */
    template <void * = nullptr> requires (_Const == true)
    mutable_iterator remove_const() const noexcept {
        return mutable_iterator { const_cast <_Node_Type *> (node) };
    }

  public:
    /* All functions below are generated from the basic. */

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,_Dir>
    value_type &operator [](difference_type __n) const noexcept {
        basic_iterator __tmp { *this };
        return *(__tmp += __n);
    }

    template <void * = nullptr>
    requires iterator_base::advanceable <_Traits,_Data_Type,_Dir>
    basic_iterator operator ++(int) noexcept {
        basic_iterator __tmp { *this };
        this->operator++();
        return __tmp;
    }

    template <void * = nullptr>
    requires iterator_base::advanceable <_Traits,_Data_Type,!_Dir>
    basic_iterator operator --(int) noexcept {
        basic_iterator __tmp { *this };
        this->operator--();
        return __tmp;
    }

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,_Dir>
    friend basic_iterator operator +
        (basic_iterator __lhs, difference_type __n) noexcept {
        basic_iterator __tmp { __lhs };
        return __tmp += __n;
    }

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,_Dir>
    friend basic_iterator operator +
        (difference_type __n, basic_iterator __rhs) noexcept {
        basic_iterator __tmp { __rhs };
        return __tmp += __n;
    }

    template <void * = nullptr>
    requires iterator_base::random_accessible <_Traits,_Data_Type,!_Dir>
    friend basic_iterator operator -
        (basic_iterator __lhs, difference_type __n) noexcept {
        basic_iterator __tmp { __lhs };
        return __tmp -= __n;
    }

};


/**
 * @brief The traits of a pointer.
 * @tparam _Tp Type of the inner data.
 * @tparam ..._Tags The custom tags.
 */
template <class _Tp, class ..._Tags>
struct pointer_traits : helper::normal_pointer {
    using node_type         = _Tp;
    using value_type        = _Tp;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::contiguous_iterator_tag;
    using pointer           = value_type *;
    using reference         = value_type &;
    using compare_type      = std::__detail::__synth3way_t <pointer>;
};


template <class _Tp, bool _Const, bool _Dir = false>
using pointer_iterator = basic_iterator <pointer_traits <_Tp>,_Const,_Dir>;


} // namespace dark

