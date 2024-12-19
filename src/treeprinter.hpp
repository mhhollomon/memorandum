#pragma once

#include "bplustree.hpp"

#include <queue>
#include <iostream>
#include <map>

namespace BPT {

template<class K, class V, std::size_t FO>
class tree_printer {

    using tree_type = BPT::BPlusTree<K, V, FO>;
    using node_ptr_type = BPT::BPlusTree<K, V, FO>::tree_node_type *;

    const tree_type & tree_;
    std::queue<std::pair<int, node_ptr_type>> queue_;

    std::map<void *, int> ptr_map_;
    int numbers = 1;


public:
    tree_printer(const tree_type & tree) : tree_{tree} {
        //map nullptr to 0 for ease of reading.
        ptr_map_.insert({nullptr, 0});
    }

    void print(bool print_values = true) {
        queue_.push({0, tree_.get_root_ptr()});

        int current_level = -1;
        std::cout << "TREE -- ";

        while (not queue_.empty()) {
            auto curr_pair = queue_.front();
            queue_.pop();
            int level = curr_pair.first;
            if (level != current_level) {
                std::cout << "\n" << level << ": ";
                current_level = level;
            } else {
                std::cout << ' ';
            }
            print_node(current_level+1, curr_pair.second);
        }

        std::cout << "\n";

        if (print_values) {
            std::cout << "VALUES -- \n";
            auto * value_ptr = tree_.get_values();

            while (value_ptr != nullptr) {
                std::cout << "(" << get_alias(value_ptr) << ") " << 
                    (value_ptr->deleted ? 'D' : ' ') << value_ptr->kv.key << ", " << 
                    value_ptr->kv.value << "\n";
                value_ptr = value_ptr->next;
            }
        }
/* 
        for (auto const &it : ptr_map_ ) {
            std::cout << it.first << " => " << it.second << "\n";
        } 
*/


    }

    int get_alias(void * node_ptr) {
        int alias;

        if (auto search = ptr_map_.find(node_ptr); search != ptr_map_.end()) {
            alias = search->second;
        } else {
            alias = numbers++;
            ptr_map_.insert(std::make_pair(node_ptr, alias));
        }

        return alias;

    }

    void print_node(int child_level, node_ptr_type node_ptr) {
        int alias = get_alias(node_ptr);

        if (node_ptr) {
            int parent = get_alias(node_ptr->parent);

            std::cout << "<" << alias << ">[" <<parent << ":" << node_ptr->num_keys << ":" << (node_ptr->is_internal() ? 'I' : 'L');

            for (int i = 0; i < node_ptr->num_keys; ++i) {
                if (node_ptr->is_internal()) {
                    std::cout << " (" << get_alias(node_ptr->child_ptrs[i]) << ")"  << ' ' << node_ptr->keys[i];
                } else {
                    std::cout << ' ' << node_ptr->keys[i] <<
                        (node_ptr->deleted[i] ? "D" : "") <<
                        "/" << get_alias(node_ptr->child_ptrs[i]);
                }
            }

            if (node_ptr->is_internal()) {
                std::cout << " (" << get_alias(node_ptr->child_ptrs[node_ptr->num_keys]) << ")";

            }

            std::cout << " ]";

            if (node_ptr->is_internal()) {
                for (int i = 0; i < node_ptr->num_keys+1; ++i) {
                    queue_.push(std::make_pair(child_level, 
                        (node_ptr_type)node_ptr->child_ptrs[i]));
                }                
            }
        } else {
            std::cout << "<0>[ ?? ]";

        }

    }


};



}