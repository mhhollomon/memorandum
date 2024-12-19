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
