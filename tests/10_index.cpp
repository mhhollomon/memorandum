#include <memorandum.hpp>

#include <catch2/catch_all.hpp>

using namespace Memorandum;

struct test { 
    int a; int b;
    bool operator==(const test &a) const = default;
};

TEST_CASE("simple", "[index]") {

    Table<test> test_table{};

    auto &idx = test_table.create_index<int>([&](const test &o) { return o.a; });

    auto const &itr = test_table.insert_row({1, 2});

    REQUIRE(test_table.count() == 1);
    REQUIRE(idx.count() == 1);

    auto const &r2 = idx.find(1);

    REQUIRE(r2 != test_table.end());
    REQUIRE(r2->value == test{1, 2});

    test_table.delete_row(itr->oid);

    REQUIRE(test_table.count() == 0);
    REQUIRE(idx.count() == 0);

}