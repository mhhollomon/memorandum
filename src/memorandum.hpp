#pragma once

#ifndef _memorandum_include_guard__
#define _memorandum_include_guard__

#include <cstddef>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <functional>
#include <concepts>



namespace Memorandum {
/**************************************/

template<class ValueType>
requires requires(ValueType a, ValueType b) {
    { a == b } -> std::convertible_to<bool>;
}
class Table {

    using size_type = std::size_t;
    using oid_type = std::size_t;

    static constexpr size_type rows_per_bucket = 100;

public :
    using value_type = ValueType;
    using predicate_type = std::function<bool(const value_type&)>;

private :

    struct _row {
        oid_type oid;
        value_type value;
        bool deleted = false;

        _row() = default;
        _row(oid_type o, const value_type &v) : oid{o}, value{v} {}
    };

    struct _bucket {
        _bucket * next = nullptr;
        _bucket * previous = nullptr;

        oid_type oid;
        size_type used_slots = 0;
        std::array<_row, rows_per_bucket> rows;

        _bucket() = default;
        _bucket(oid_type new_oid) : oid{new_oid} {}

        friend bool operator==(const _bucket &a, const _bucket &b) {
            return a.oid == a.oid;
        }
        friend bool operator<=>(const _bucket &a, const _bucket &b) {
            return a.oid <=> a.oid;
        }

        bool is_empty() {
            if (used_slots == 0) return true;

            for (int i = 0; i < used_slots; ++i) {
                if (not rows[i].deleted) return false;
            }

            return true;
        }
    };

    oid_type last_oid = 0;

    oid_type get_next_oid() { return ++last_oid; }

    
    _bucket * bucket_head_ = nullptr;
    _bucket * bucket_tail_ = nullptr;


    _bucket * add_bucket() {
        auto oid = get_next_oid();

        auto * new_bucket = new _bucket(oid);

        if (bucket_head_) {
            bucket_tail_->next = new_bucket;
            new_bucket->previous = bucket_tail_;
            bucket_tail_ = new_bucket;
        } else {
            bucket_head_ = bucket_tail_ = new_bucket;
        }

        return new_bucket;
    }

    struct iterator {
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        // hopefully these works.
        using value_type = value_type;

        using pointer = value_type*;
        using reference = value_type&;

        static bool yes(const value_type& b) { return true; }
        iterator(_bucket * ptr, 
            size_type slot,
            predicate_type pred = yes) : ptr_{ptr}, slot_{slot}, predicate_{pred} {
        }

        reference operator*() const {
            return ptr_->rows[slot_].value;
        }

        pointer operator->() const { return &ptr_->rows[slot_].value; }

        // Prefix increment
        iterator & operator++() {
            // according to the standard
            // incrementing past the end() is "undefined"
            // So don't bother trying to catch anything.
            slot_ += 1;
            while (1) {
                if (slot_ >= ptr_->used_slots) {
                    ptr_ = ptr_->next;
                    if (ptr_ == nullptr) {
                        slot_ = rows_per_bucket + 1;
                        break;
                    }
                    slot_ = 0;
                } else if (ptr_->rows[slot_].deleted) {
                    slot_ += 1;
                } else if (not predicate_(ptr_->rows[slot_].value)) {
                    slot_ += 1;
                } else {
                    break;
                }
            }

            return *this;
        }

        // Postfix increment
        iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }


        friend bool operator== (const iterator& a, const iterator& b) { 
            return a.ptr_ == b.ptr_ and a.slot_ == b.slot_; 
        };
        friend bool operator!= (const iterator& a, const iterator& b) { 
            return not(a == b);
        };

    private :
        _bucket * ptr_;
        size_type slot_ = 0;
        predicate_type predicate_;

    };


/******************************************************
 * Public Interface
 ******************************************************/
public :

    oid_type insert_row(const value_type &value) {

        _bucket * bucket = bucket_tail_;
        if (not bucket or bucket->used_slots >= rows_per_bucket) {
            bucket = add_bucket();
        }

        auto oid = get_next_oid();
        bucket->rows[bucket->used_slots] = {oid, value};
        bucket->used_slots += 1;

        return oid;

    }

    void delete_row(const oid_type row_num) {
        _bucket * bucket = bucket_head_;
        while (bucket) {
            for (int i = 0; i < bucket->used_slots; ++i) {
                auto &r = bucket->rows[i];
                if (r.oid == row_num) {
                    r.deleted = true;
                    return;
                }
            }
            bucket = bucket->next;
        }
    }

    

    size_type count() {
        size_type retval = 0;
        _bucket * bucket = bucket_head_;
        while (bucket) {
            for (int i = 0; i < bucket->used_slots; ++i) {
                retval += not bucket->rows[i].deleted;
            }
            bucket = bucket->next;
        }

        return retval;

    }

    auto select(predicate_type p) {
        return iterator(
            bucket_head_,
            0,
            p           
        );

    }

    auto begin() {
        return iterator(
            bucket_head_,
            0);
    }

    auto end() {
        return iterator(
            nullptr,
            rows_per_bucket+1);
    }



};



/**************************************/
}



#endif