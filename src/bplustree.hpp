#pragma once

#ifndef _bplustree_include_guard__
#define _bplustree_include_guard__

#include <cstddef>
#include <memory>
#include <array>
#include <variant>
#include <format>
#include <cassert>
#include <bitset>
#include <iterator>

//#include <iostream>


namespace BPT {
/**************************************/

#include "include/_value_wrapper.hpp"
#include "include/_tree_node.hpp"

constexpr static std::size_t DEFAULT_FAN_OUT = 20;

template<class Key>
class set;

template <typename T>
concept equal_and_less = requires(T a, T b) {
    {a < b} -> std::convertible_to<bool>;
    {a == b} -> std::convertible_to<bool>;
};

template<typename K, class V, std::size_t FO = DEFAULT_FAN_OUT>
requires (FO > 3) && equal_and_less<K>
class BPlusTree {

public :

    using key_type = K;
    using mapped_type = V;
    constexpr static std::size_t fan_out = FO;

    using value_wrapper_type = ValueWrapper<key_type, mapped_type>;
    using value_type = typename value_wrapper_type::kvpair;
    using tree_node_type = TreeNode<key_type, mapped_type, fan_out>;

    BPlusTree() : root_node_(new tree_node_type(LeafNode)) {
    }

    BPlusTree(BPlusTree &other) : root_node_(new tree_node_type(LeafNode)) {
        operator=(other);
    }


    void swap(BPlusTree &other) {
        std::swap(root_node_, other.root_node_);
        std::swap(values_head_, other.values_head_);
        std::swap(values_tail_, other.values_tail_);
    }

    BPlusTree(BPlusTree &&other) : root_node_(new tree_node_type(LeafNode)) {
        swap(other);
    }

    BPlusTree &operator=(BPlusTree const &other) {
        clear();
        for (auto const & it : std::as_const(other)) {
            insert(it.key, it.value);
        }

        return *this;
    }

    BPlusTree &operator=(BPlusTree &&other) {
        swap(other);
    }

    ~BPlusTree() {
        _clear_all(true, false);
    }

    tree_node_type *get_root_ptr() const { return root_node_; }
    value_wrapper_type *get_values() const { return values_head_; }

    struct FindResults {
        bool found;
        tree_node_type *node;
        std::size_t index;

        FindResults(bool f, tree_node_type *ptr, std::size_t i = 0) :
            found(f),node(ptr), index(i)
            {}

    };

private:

    /*********************************************************************
     * Private interface
     ********************************************************************/
    template<bool REVR=false>
    struct _iterator_base {

        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        // hopefully these works.
        using value_type = value_type;
        using value_wrapper_type = value_wrapper_type;

        using pointer = value_type*;
        using reference = value_type&;

        _iterator_base(value_wrapper_type *ptr) : ptr_{ptr} {

            while (ptr_ and ptr_->deleted) {
                if constexpr (REVR)
                    ptr_ = ptr_->previous;
                else
                    ptr_ = ptr_->next;
            }
        }

        reference operator*() const {
            return ptr_->kv;
        }

        pointer operator->() const { return &(ptr_->kv); }

        // Prefix increment
        _iterator_base & operator++() {
            // according to the standard
            // incrementing past the end() is "undefined"
            // So don't bother trying to catch anything.
            do {
                if constexpr (REVR)
                    ptr_ = ptr_->previous;
                else
                    ptr_ = ptr_->next;
            } while(ptr_ and ptr_->deleted );

            return *this;
        }

        // Postfix increment
        _iterator_base operator++(int) { _iterator_base tmp = *this; ++(*this); return tmp; }


        friend bool operator== (const _iterator_base& a, const _iterator_base& b) { return a.ptr_ == b.ptr_; };
        friend bool operator!= (const _iterator_base& a, const _iterator_base& b) { return a.ptr_ != b.ptr_; };

    private :
        value_wrapper_type *ptr_;

    };

    struct const_iterator : _iterator_base<false> {

        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        // hopefully these works.
        using value_type = value_type;
        using value_wrapper_type = value_wrapper_type;

        using pointer = value_type const *;
        using reference = value_type const &;

        using _iterator_base<false>::_iterator_base;

    };

    struct reverse_iterator : _iterator_base<true> {

        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        // hopefully these works.
        using value_type = value_type;
        using value_wrapper_type = value_wrapper_type;

        using pointer = value_type const *;
        using reference = value_type const &;

        using _iterator_base<true>::_iterator_base;

    };

    /**************** END of iterators ******************* */

    bool _is_less(key_type const &a, key_type const & b) const {
        return a < b;
    }

    bool _equivalent(key_type const &a, key_type const & b) const {
        return not _is_less(a, b) and not _is_less(b, a);
    }

    /**********************************
     * _intranode_leaf_search
     * For leaf node, we ae only interested in equality.
     **********************************/
    int _intranode_leaf_search(key_type const &key, tree_node_type const *node) const {
        assert(node->is_leaf());
        int found_index = -1;

        std::size_t bottom = 0, top = node->num_keys;

        while(top != bottom) {
            std::size_t mid = (top + bottom)/2;

            if (mid == bottom) {
                if (_equivalent(key,node->keys[mid])) {
                    found_index = mid;
                }
                break;
            }
             
            if (_is_less(key, node->keys[mid])) {
                top = mid;
            } else if (_is_less(node->keys[mid], key)) {
                bottom = mid;
            } else {
                found_index = mid;
                break;
            }
        }

        //std::cout << "_intranode_leaf_search : returning " << found_index << " for " << key << "\n";

        return found_index;
    }

    /**********************************
     * _intranode_internal_search
     * For internal node, we need to differenciate between "<" and ">=".
     **********************************/
    int _intranode_internal_search(key_type const &key, tree_node_type const *node) const {
        assert(node->is_internal());

        int found_index = -1;

        std::size_t bottom = 0, top = node->num_keys;

        while(top != bottom) {
            std::size_t mid = (top + bottom)/2;

            if (mid == bottom) {
                // We've run out of room.
                if (_is_less(key, node->keys[mid])) {
                    found_index = mid;
                } else {
                    // new key >= to current key
                    found_index = mid +1;
                }
                break;
            }
             
            if (_is_less(key, node->keys[mid])) {
                top = mid;
            } else if (_is_less(node->keys[mid], key)) {
                bottom = mid;
            } else {
                // the "equivalent" pointer is to the right.
                found_index = mid+1;
                break;
            }
        }

        //std::cout << "_intranode_internal_search : returning " << found_index << " for " << key << "\n";

        return found_index;
    }


    /**********************************
     * _find
     * Returns the leaf the new key should be inserted to.
     * Note : returns "true" even if the key has been deleted.
     * It is up to the caller to decide if that is good or bad.
     **********************************/
    FindResults _find(key_type const & key) const {

        auto *current_node_ptr = get_root_ptr();
        bool found = false;
        int found_index = -1;

        while (1) {
            if (current_node_ptr->is_leaf()) {

                auto retval = _intranode_leaf_search(key, current_node_ptr);
                if (retval != -1) {
                    found = true;
                    found_index = retval;
                }
                // We are at a leaf so no further recursion possible.
                // either we found it in the loop or we did not.
                break; // out of while-loop

            } else if (current_node_ptr->is_internal()) {

                auto retval = _intranode_internal_search(key, current_node_ptr);
                found_index = retval;
                current_node_ptr = (tree_node_type *)(current_node_ptr->child_ptrs[retval]);

            } else {
                throw std::runtime_error(std::format("Unknown node type {}", int(current_node_ptr->ntype)));
            }
        }

        return FindResults(found, current_node_ptr, found_index);
    }

    /**********************************
     * _clear_all
     **********************************/
    void _clear_all(bool clear_values = true, bool new_root = true) {

        if (new_root == false or root_node_->is_internal() or root_node_->num_keys > 0 ) { 
            _clear_tree(root_node_);
            if (new_root) {
                root_node_ = new tree_node_type(LeafNode);
            } else {
                root_node_ = nullptr;
            }
        }

        if (clear_values and values_head_) {
            auto *current = values_head_;

            while (current) {
                auto *next = current->next;
                delete current;
                current = next;
            }

            values_head_ = nullptr;
            values_tail_ = nullptr;
        }

    }


    /**********************************
     * _clear_tree
     * Only clears the tree structure not the value list.
     **********************************/
    void _clear_tree(tree_node_type *node) {

        if (node->is_internal()) {
            int max = node->num_keys;
            for(int i = 0; i <= max; ++i) {
                _clear_tree((tree_node_type *)node->child_ptrs[i]);
            }
        }

        delete node;
    }

    /**********************************
     * _split_internal
     * Returns the new node that is created.
     **********************************/
    tree_node_type * _split_internal(tree_node_type *old_node, const key_type &new_key, tree_node_type *new_child ) {
        assert(old_node != nullptr);
        assert(new_child != nullptr);
        assert(old_node->is_internal());
        assert(old_node->is_full());

        //std::cout << std::format("_split_internal : start new_key = {}\n", new_key);

        std::size_t promoted_index = old_node->key_limit / 2 - 1;
        key_type promoted_key = old_node->keys[promoted_index];
        bool new_key_promoted = false;
        //std::cout << std::format("_split_internal : (1) promoted_key = {} promoted_index = {}\n", promoted_key, promoted_index);


        if (_is_less(promoted_key,new_key) ) {
            promoted_index += 1;
            promoted_key = old_node->keys[promoted_index];

            //std::cout << std::format("_split_internal : (2) promoted_key = {} promoted_index = {}\n", promoted_key, promoted_index);

            if (_is_less(new_key,promoted_key)) {
                promoted_key = new_key;
                new_key_promoted = true;
            }

        }

        auto *new_node = new tree_node_type(old_node->ntype);

        // if new_key_promoted then the promoted_index must be copied
        // to the new node.
        // If NOT new_key_promoted, then the position to the right of
        // promoted_index needs to be copied.
        std::size_t copy_min = promoted_index + 1 - new_key_promoted;
        std::size_t new_index = 0;
        std::size_t old_index = copy_min;

        //std::cout << std::format("_split_internal : (3) promoted_key = {} promoted_index = {} copy_min = {}\n", promoted_key, promoted_index, copy_min);


        for (; 
                old_index < old_node->keys.size(); 
                ++old_index, ++new_index) {

            new_node->keys[new_index] = old_node->keys[old_index];
            new_node->child_ptrs[new_index] = old_node->child_ptrs[old_index];

            ((tree_node_type *)(new_node->child_ptrs[new_index]))->parent = new_node;

        }

        new_node->child_ptrs[new_index] = old_node->child_ptrs[old_index];
        ((tree_node_type *)(new_node->child_ptrs[new_index]))->parent = new_node;

        new_node->num_keys = tree_node_type::key_limit - copy_min;
        if (new_key_promoted) {
            old_node->num_keys = copy_min;

            new_node->child_ptrs[0] = new_child;
            new_child->parent = new_node;
            // switch back
            ((tree_node_type *)(old_node->child_ptrs[copy_min]))->parent = old_node;

        } else {
            old_node->num_keys = copy_min - 1;

            if (_is_less(new_key, promoted_key)) {
                _insert_into_node(old_node, new_key, new_child);
                new_child->parent = old_node;
            } else {
                _insert_into_node(new_node, new_key, new_child);
                new_child->parent = new_node;
            }

        }
        
        
        if (old_node->parent) {
            if (not old_node->parent->is_full()) {
                _insert_into_node(old_node->parent, promoted_key, new_node);
                new_node->parent = old_node->parent;
            } else {
                _split_node(old_node->parent, promoted_key, new_node);
            }
        } else {
            // Must be at root, so create fresh node and jam lowest key in new
            // leaf into it.
            auto * new_parent = new tree_node_type(InternalNode);

            new_parent->keys[0] = promoted_key;
            new_parent->child_ptrs[0] = old_node;
            new_parent->child_ptrs[1] = new_node;
            new_parent->num_keys = 1;

            old_node->parent = new_node->parent = new_parent;
            root_node_ = new_parent;
        }

        return new_node;

    }

    /**********************************
     * _split_node
     * Returns the new node that is created.
     **********************************/
    tree_node_type * _split_node(tree_node_type *old_node, const key_type &new_key, void *child_ptr ) {
        assert(old_node != nullptr);
        assert(child_ptr != nullptr);

        if (old_node->is_internal()) {
            return _split_internal(old_node, new_key, reinterpret_cast<tree_node_type *>(child_ptr));
        }

        //std::cout << "splitting : " << old_node->is_full() << "\n";

        auto *new_node = new tree_node_type(old_node->ntype);

        // copy over half the key/values from the old leaf
        int new_index = 0;
        const int split_index = (tree_node_type::key_limit/2);
        for (int old_index = split_index; 
                old_index < tree_node_type::key_limit; 
                ++old_index, ++new_index) {
            new_node->keys[new_index] = old_node->keys[old_index];
            new_node->child_ptrs[new_index] = old_node->child_ptrs[old_index];
        }


        //std::cout << "copy done\n";

        new_node->num_keys = tree_node_type::key_limit - split_index;
        old_node->num_keys = split_index;


        // need to integrate this into the loop above. this does more shuffling.

        if (_is_less(new_key, new_node->min_key())) {
            //std::cout << "split : inserting into old_node\n";
            _insert_into_node(old_node, new_key, child_ptr);
        } else {
            //std::cout << "split : inserting into new_node\n";
            _insert_into_node(new_node, new_key, child_ptr);
        }

        if (old_node->parent) {
            auto min_key = new_node->min_key();
            if (not old_node->parent->is_full()) {
                _insert_into_node(old_node->parent, min_key, new_node);
                new_node->parent = old_node->parent;
            } else {
                _split_node(old_node->parent, min_key, new_node);
            }
        } else {
            // Must be at root, so create fresh node and jam lowest key in new
            // leaf into it.
            auto * new_parent = new tree_node_type(InternalNode);

            new_parent->keys[0] = new_node->keys[0];
            new_parent->child_ptrs[0] = old_node;
            new_parent->child_ptrs[1] = new_node;
            new_parent->num_keys = 1;

            old_node->parent = new_node->parent = new_parent;
            root_node_ = new_parent;
        }

        return new_node;

    }

    /**********************************
     * _insert_into_node
     **********************************/

    void _insert_into_node(tree_node_type *node, key_type const &new_key, void* new_child ) {
        assert(node != nullptr);
        assert(new_child != nullptr);
        assert(not node->is_full());

        //std::cout << "_insert : (" << node->is_full() << "), key = " << new_key << "\n";

        int insert_index = node->num_keys;

        auto * keys_ptr = node->keys.data();
        auto ** child_ptr = node->child_ptrs.data();

        if (node->is_empty()) {
            //std::cout << "_insert : node is empty\n";

            // Only happens if this is the first insert into the tree.
            // the node will be a leaf node.
            assert(node->is_leaf());
            assert(values_head_ == nullptr);

            child_ptr[0] = new_child;
            keys_ptr[0] = new_key;
            node->num_keys = 1;

            values_head_ = (value_wrapper_type *)new_child;
            values_tail_ = values_head_;
    
            return;

        }
        

        for (int check_index = insert_index-1;
                insert_index >= 0;
                --check_index, --insert_index) {
            
            //std::cout << std::format("_insert : loop - check_index = {}, insert_index = {}\n", check_index, insert_index);
            if (check_index < 0 or _is_less(keys_ptr[check_index], new_key) ) {
                keys_ptr[insert_index] = new_key;

                if (node->is_internal()) {
                    child_ptr[insert_index+2] = child_ptr[insert_index+1];
                    child_ptr[insert_index+1] = new_child;
                } else {
                    node->deleted[insert_index]  = false;
                    child_ptr[insert_index] = new_child;
                    auto *new_value_ptr = (value_wrapper_type *)new_child;
                    auto ** value_ptr = (value_wrapper_type **)child_ptr;
                    if (check_index >= 0) {
                        auto *new_next_ptr = new_value_ptr->next = value_ptr[check_index]->next;
                        value_ptr[check_index]->next = new_value_ptr;

                        new_value_ptr->previous = value_ptr[check_index];
                        if (new_next_ptr) {
                            new_next_ptr->previous = new_value_ptr;
                        } else {
                            values_tail_ = new_value_ptr;
                        }
                    } else {
                        // look to the right
                        new_value_ptr->next = value_ptr[insert_index + 1];
                        new_value_ptr->previous = value_ptr[insert_index + 1]->previous;

                        if (new_value_ptr->previous == nullptr) {
                            values_head_ = new_value_ptr;
                        } else {
                            new_value_ptr->previous->next = new_value_ptr;
                        }

                        if (new_value_ptr->next == nullptr) {
                            values_tail_ = new_value_ptr;
                        } else {
                            new_value_ptr->next->previous = new_value_ptr;
                        }

                    }
  
                }

                break; // for loop

            } else {
                // shove the current resident up one.
                keys_ptr[insert_index] = keys_ptr[check_index];

                if (node->is_internal()) {
                    child_ptr[insert_index+2] = child_ptr[insert_index+1];
                } else {
                    child_ptr[insert_index] = child_ptr[check_index];
                    node->deleted[insert_index] = node->deleted[check_index];

                }
            }
        }

        //std::cout << "_insert : updating num_keys\n";
        node->num_keys += 1;

    }

public:
    /*********************************************************************
     * Public interface
     ********************************************************************/

    /**********************************
     * INSERT
     **********************************/
    std::pair<const_iterator, bool> insert(const key_type &key, mapped_type value) {

        auto find_results = _find(key);

        //std::cout << "insert : find " << key << "(" << find_results.found << ")\n";

        if (find_results.found) {
            auto * value_ptr = find_results.node->get_value_ptr(find_results.index);
            if (find_results.node->deleted[find_results.index]) {
                // deleted - update the value and "undelete"
                value_ptr->kv.value = value;
                value_ptr->deleted = false;
                find_results.node->deleted[find_results.index] = false;
                return {{value_ptr}, true};

            } else {
                // future to do - support multimap
                // Support insert_or_assign
                return {{value_ptr}, false};
            }
        }

        auto *leaf_ptr = find_results.node;

        //std::cout << "insert : just before action\n";

        auto *value_ptr = new value_wrapper_type(key, value);

        if (leaf_ptr->num_keys == tree_node_type::key_limit ) {
            _split_node(leaf_ptr, key, value_ptr);
        } else {
            _insert_into_node(leaf_ptr, key, value_ptr);

        }


        return {{value_ptr}, true};
    }

    std::pair<const_iterator, bool> insert(std::pair<key_type, mapped_type> new_pair) {
        return insert(new_pair.first, new_pair.second);
    }

    /**********************************
     * REMOVE
     **********************************/
    bool remove(const key_type &key) {
        auto results = _find(key);

        if (results.found) {
            if (results.node->deleted[results.index]) {
                return false;
            }
            results.node->deleted.set(results.index, true);
            auto * value_ptr = results.node->get_value_ptr(results.index);
            value_ptr->deleted = true;
        }

        return results.found;
    }

    /*********************************
     * FIND
     *********************************/
    const_iterator find(const key_type &key) const {

        auto results = _find(key);

        if (results.found) {
            if (results.node->deleted[results.index]) {
                return cend();
            } else {
                auto * value_ptr = (value_wrapper_type *)results.node->child_ptrs[results.index];
                return const_iterator{value_ptr};
            }
        }

        return cend();

    }
    /*********************************
     * CLEAR
     *********************************/
    
    void clear() {
        _clear_all();
    }

    /*********************************
     * BEGIN
     *********************************/
    //iterator begin() { return iterator(values_head_); }

    /*********************************
     * END
     *********************************/
    //iterator end() { return iterator(nullptr); }


    /*********************************
     * CBEGIN
     *********************************/
    const_iterator cbegin() const { return const_iterator(values_head_); }
    const_iterator begin() const { return const_iterator(values_head_); }

    reverse_iterator crbegin() const { return reverse_iterator(values_tail_); }
    reverse_iterator rbegin() const { return reverse_iterator(values_tail_); }

    /*********************************
     * CEND
     *********************************/
    const_iterator cend() const { return const_iterator(nullptr); }
    const_iterator end() const { return const_iterator(nullptr); }

    reverse_iterator crend() const { return reverse_iterator(nullptr); }
    reverse_iterator rend() const { return reverse_iterator(nullptr); }

    /*********************************
     * COMPUTE_SIZE
     * 
     * Adding a size attribute to the tree would create a contention hotspot
     * when we start adding support for concurrency.
     *********************************/
    std::size_t compute_size() const {
        std::size_t size = 0;

        for (auto const & it : *this) {
            size += 1;
        }

        return size;
    }

    bool contains(const key_type & key) const {
        return (find(key) != cend());
    }


    /*******************************************************
     * INTERFACE for C++20
     ******************************************************/
    /**********************************
     * at
     **********************************/
    mapped_type & at(const key_type &key) {
        auto results = _find(key);

        if (results.found) {
            auto * value_ptr = (value_wrapper_type *)results.node->child_ptrs[results.index];
            return value_ptr->kv.value;
        } else {
            throw std::out_of_range("Could not find key");
        }
    }

private :
    tree_node_type* root_node_;
    value_wrapper_type* values_head_ = nullptr;
    value_wrapper_type* values_tail_ = nullptr;

    friend class set<K>;


};


template<class K, class V, std::size_t FO>
void swap(BPlusTree<K, V, FO> &a, BPlusTree<K, V, FO> &b) {
    a.swap(b);
}

/**************************************/
}

#endif

