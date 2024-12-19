#include <memorandum.hpp>

#include <catch2/catch_all.hpp>

using namespace Memorandum;


TEST_CASE("constructors", "[basic]") {

    Table<int> int_table;

    REQUIRE(sizeof(int_table) == 24);

}

TEST_CASE("insert/delete", "[basic]") {
    Table<int> int_table;
    int_table.insert_row(43);

    REQUIRE(int_table.count() == 1);

    auto oid = int_table.insert_row(43);
    REQUIRE(int_table.count() == 2);

    int_table.delete_row(oid);
    REQUIRE(int_table.count() == 1);


}

TEST_CASE("iterators", "[basic]") {
    Table<int> int_table;
    int_table.insert_row(43);

    auto iter = int_table.begin();
    REQUIRE(*iter == 43);
    REQUIRE(iter != int_table.end());

    ++iter;
    REQUIRE(iter == int_table.end());

    auto oid = int_table.insert_row(99);
    int_table.insert_row(77);

    int_table.delete_row(oid);

    REQUIRE(int_table.count() == 2);

    iter = int_table.begin();
    REQUIRE(*iter == 43);
    ++iter;
    REQUIRE(*iter == 77);

   

}

TEST_CASE("predicate", "[basic]") {
    Table<int> int_table;
    int_table.insert_row(43);
    auto oid = int_table.insert_row(99);
    int_table.insert_row(77);

    auto iter = int_table.select([&](const int &a) { return a < 99; });

    REQUIRE(*iter == 43);
    ++iter;
    REQUIRE(*iter == 77);
    ++iter;
    REQUIRE(iter == int_table.end());

}
