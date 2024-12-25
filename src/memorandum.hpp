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

    static constexpr size_type rows_per_bucket_ = 100;

public :
    using value_type = ValueType;
    using predicate_type = std::function<bool(const value_type&)>;

private :

    /****************************************************
     * Private Structures
     ****************************************************/
    #pragma region
    struct _row {
        struct _kv {
            oid_type oid;
            value_type value;
        } kv;

        bool deleted = false;

        _row() = default;
        _row(oid_type o, const value_type &v) : kv{o, v} {}
    };

    struct _bucket {
        _bucket * next = nullptr;
        _bucket * previous = nullptr;

        oid_type oid;
        size_type used_slots = 0;
        std::array<_row, rows_per_bucket_> rows;

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

    struct iterator {
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        // hopefully these works.
        using value_type = value_type;

        using iterator_return_type = _row::_kv;

        using pointer = value_type*;
        using reference = value_type&;

        static bool yes(const value_type& b) { return true; }
        iterator(_bucket * ptr, 
            size_type slot,
            predicate_type pred = yes) : ptr_{ptr}, slot_{slot}, predicate_{pred} {
        }

        iterator_return_type & operator*() const {
            return ptr_->rows[slot_].kv;
        }

        iterator_return_type *operator->() const { return &operator*(); }

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
                        slot_ = rows_per_bucket_ + 1;
                        break;
                    }
                    slot_ = 0;
                } else if (ptr_->rows[slot_].deleted) {
                    slot_ += 1;
                } else if (not predicate_(ptr_->rows[slot_].kv.value)) {
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

    struct _index_base {

        virtual void add(oid_type rowid, const ValueType &v) = 0;
        virtual void remove(oid_type rowid, const ValueType &v)  = 0;
    };

    struct _row_ref {
        _bucket * ptr = nullptr;
        size_type slot = 0;

        _row_ref(_bucket * p, size_type s) : ptr{p}, slot{s} {}

        _row &get_row() {
            return ptr->rows[slot];
        }

    };

    struct _index_ref {
        _index_base *idx;
        bool is_multi;

        _index_ref(_index_base *i, bool m) : idx{i}, is_multi{m} {}
    };

#pragma endregion

    /****************************************************
     * Private Data
     ****************************************************/

    oid_type last_oid_ = 0;

    _bucket * bucket_head_ = nullptr;
    _bucket * bucket_tail_ = nullptr;

    std::map<oid_type, _row_ref> row_map_;

    std::map<std::string, _index_ref> index_map_;


    /****************************************************
     * Private Methods
     ****************************************************/
    #pragma region

    oid_type get_next_oid() { return ++last_oid_; }

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

    iterator find_(oid_type rowid) {

        auto iter = row_map_.find(rowid);

        if (iter == row_map_.end()) {
            return end();
        } else {
            return iterator(iter->second.ptr, iter->second.slot);
        }
    }
#pragma endregion

/******************************************************
 * Public Interface
 ******************************************************/
public :

    iterator insert_row(const value_type &value) {

        _bucket * bucket = bucket_tail_;
        if (not bucket or bucket->used_slots >= rows_per_bucket_) {
            bucket = add_bucket();
        }

        auto oid = get_next_oid();
        auto this_slot = bucket->used_slots;
        bucket->rows[this_slot] = {oid, value};
        bucket->used_slots += 1;

        row_map_.insert({oid, {bucket, this_slot}});

        for(auto &idx : index_map_) {
            idx.second.idx->add(oid, value);
        }

        return iterator{bucket, this_slot};

    }

    void delete_row(const oid_type row_num) {

        auto iter = row_map_.find(row_num);

        if (iter == row_map_.end()) {
            return;
        }

        auto &r = iter->second.get_row();

        for(auto &idx : index_map_) {
            idx.second.idx->remove(row_num, r.kv.value);
        }

        r.deleted = true;
        row_map_.erase(row_num);

    }

    

    size_type count() const {
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
            rows_per_bucket_+1);
    }

    Table() = default;

    ~Table() {
        if (bucket_head_) {
            auto * ptr = bucket_head_;
            while (ptr) {
                auto * next = ptr->next;
                delete(ptr);
                ptr = next;
            }

            bucket_head_ = bucket_tail_ = nullptr;
        }

        for (auto value : index_map_) {
            delete value.second.idx;
        }
    }

    template<typename IndexType>
    struct table_index : public _index_base {
        using accessor_type = std::function<IndexType(const ValueType &)>;

        table_index(accessor_type accessor, Table *t) : table_{t}, accessor_{accessor} {}

        size_type count() const { return index_data_map_.size(); }

        iterator find(IndexType const &idx) {

            auto iter = index_data_map_.find(idx);
            if (iter == index_data_map_.end()) {
                return table_->end();
            } else {
                return table_->find_(iter->second);
            }
        }

        private :
            accessor_type accessor_;
            Table * table_;

            std::map<IndexType, oid_type> index_data_map_;

            void add(oid_type rowid, const ValueType &v) {
                index_data_map_.insert({accessor_(v), rowid});
            }

            virtual void remove(oid_type rowid, const ValueType &v) {
                index_data_map_.erase(accessor_(v));
            }            

    };


    template<typename IT>
    table_index<IT> & create_index(std::string name, table_index<IT>::accessor_type a) {
        auto * idx = new table_index<IT>(a, this);
        index_map_.insert({name, {idx, false}});
        
        for (auto & iter : *this) {
            dynamic_cast<_index_base *>(idx)->add(iter.oid, iter.value);
        }

        return *idx;
    }

    template<typename IndexType>
    struct table_multi_index : public _index_base {
        using accessor_type = std::function<IndexType(const ValueType &)>;

        table_multi_index(accessor_type accessor, Table *t) : table_{t}, accessor_{accessor} {}

        size_type count() const { return index_data_map_.size(); }

        iterator find(IndexType const &idx) {

            auto iter = index_data_map_.find(idx);
            if (iter == index_data_map_.end()) {
                return table_->end();
            } else {
                return table_->find_(iter->second);
            }
        }

        private :
            accessor_type accessor_;
            Table * table_;

            std::multimap<IndexType, oid_type> index_data_map_;

            void add(oid_type rowid, const ValueType &v) {
                index_data_map_.insert({accessor_(v), rowid});
            }

            virtual void remove(oid_type rowid, const ValueType &v) {

                // multiple rows may map to the same key.
                // So we need to find the one that has the same rowid.

                auto [start, end] = index_data_map_.equal_range(accessor_(v));

                while(start != end) {
                    if (start->second == rowid) {
                        index_data_map_.erase(start);
                        break;
                    }
                    ++start;
                }
            }            

    };


    template<typename IT>
    table_multi_index<IT> & create_multi_index(std::string name, table_index<IT>::accessor_type a) {
        auto * idx = new table_multi_index<IT>(a, this);
        index_map_.insert({name, {idx, true}});

        for (auto & iter : *this) {
            dynamic_cast<_index_base *>(idx)->add(iter.oid, iter.value);
        }

        return *idx;
    }

    template<typename IT>
    table_index<IT> &index(std::string name) {
        auto iter = index_map_.find(name);
        if (iter == index_map_.end()) {
            throw std::runtime_error("No index named '" + name + "'");
        }

        if (iter->second.is_multi) {
            throw std::runtime_error("Index named '" + name + "' is a multi index");
        }

        return *(dynamic_cast<table_index<IT> *>(iter->second.idx));
    }

    template<typename IT>
    table_multi_index<IT> &multi_index(std::string name) {
        auto iter = index_map_.find(name);
        if (iter == index_map_.end()) {
            throw std::runtime_error("No index named '" + name + "'");
        }

        if (not iter->second.is_multi) {
            throw std::runtime_error("Index named '" + name + "' is not a multi index");
        }

        return *(dynamic_cast<table_multi_index<IT> *>(iter->second.idx));
    }


// ------------- END of CLASS Table -------------
};



/******** END of NAMESPACE *******************/
}



#endif