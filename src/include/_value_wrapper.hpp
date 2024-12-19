
template<class K, class V>
struct ValueWrapper {
    ValueWrapper *previous = nullptr;
    ValueWrapper *next = nullptr;

    struct kvpair {
        K key;
        V value;
    } kv;

    bool deleted = false;

    ValueWrapper(K inkey, V invalue) : kv{inkey, invalue} {}
};