# Memorandum::Table

```cpp
template<typename ValueType>
class Table
```
## Constructors

```cpp
// default
Table<T> table;
```

## Methods

```cpp
oid_type insert_row(const value_type &value);

void delete_row(const oid_type row_num);

size_type count();

iterator select(predicate_type p);

iterator begin();
iterator end();
```

### Indexes

```cpp
template<typename IT>
index<IT> & create_index(index<IT>::accessor_type key_function);
```

`IT` is the type of the key.

`key_function` takes a value (from the table) and computes the key.

example :

```cpp

struct employee {
    std::string name;
    int id;
};

auto table = Table<employee>{};

auto &id_index = table.create_index<int>("by_id", [&](const employee &e) { return e.id; });

table.insert_row({"Mary", 12});

auto iter id_index.find(12);

std::cout << "Id " << iter->value.id << " is " << iter->value.name << "\n";
```

The `key_function` can compute whatever it likes, however, it must be stable.
That is, given the same value, it must compute the same key.

The `Table<>` object owns the index and will clean it up during distruction.

You can retrieve an index by name:

```cpp
// continued from above

auto iter = table.index<int>("by_id").find(12);

std::cout << "Id " << iter->value.id << " is " << iter->value.name << "\n";
```
