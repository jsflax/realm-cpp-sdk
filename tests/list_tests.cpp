#include "test_utils.hpp"
#include "test_objects.hpp"
#include <cpprealm/notifications.hpp>

using namespace realm;

TEST(list) {
    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    auto obj = AllTypesObject{};
    obj.list_int_col.push_back(42);
    CHECK_EQUALS(obj.list_int_col[0], 42);

    obj.list_obj_col.push_back(AllTypesObjectLink{.str_col="Fido"});
    CHECK_EQUALS(obj.list_obj_col[0].str_col, "Fido");
    CHECK_EQUALS(obj.list_int_col.size(), 1);
    for (auto& i : obj.list_int_col) {
        CHECK_EQUALS(i, 42);
    }
    realm.write([&realm, &obj]() {
        realm.add(obj);
    });

    CHECK_EQUALS(obj.list_int_col[0], 42);
    CHECK_EQUALS(obj.list_obj_col[0].str_col, "Fido");

    realm.write([&obj]() {
        obj.list_int_col.push_back(84);
        obj.list_obj_col.push_back(AllTypesObjectLink{._id=1, .str_col="Rex"});
    });
    size_t idx = 0;
    for (auto& i : obj.list_int_col) {
        CHECK_EQUALS(i, obj.list_int_col[idx]);
        ++idx;
    }
    CHECK_EQUALS(obj.list_int_col.size(), 2);
    CHECK_EQUALS(obj.list_int_col[0], 42);
    CHECK_EQUALS(obj.list_int_col[1], 84);
    CHECK_EQUALS(obj.list_obj_col[0].str_col, "Fido");
    CHECK_EQUALS(obj.list_obj_col[1].str_col, "Rex");
    co_return;
}

TEST(list_insert_remove_primitive) {
    auto obj = AllTypesObject();
    CHECK_EQUALS(obj.is_managed(), false);

    // unmanaged
    obj.list_int_col.push_back(1);
    obj.list_int_col.push_back(2);
    obj.list_int_col.push_back(3);
    CHECK_EQUALS(obj.list_int_col.size(), 3);

    obj.list_int_col.pop_back();
    CHECK_EQUALS(obj.list_int_col.size(), 2);
    obj.list_int_col.erase(0);
    CHECK_EQUALS(obj.list_int_col.size(), 1);
    obj.list_int_col.clear();
    CHECK_EQUALS(obj.list_int_col.size(), 0);
    obj.list_int_col.push_back(2);
    obj.list_int_col.push_back(4);
    CHECK_EQUALS(obj.list_int_col.find(4), 1);
    CHECK_EQUALS(obj.list_int_col[1], 4);

    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    realm.write([&realm, &obj] {
        realm.add(obj);
    });

    // ensure values exist
    CHECK_EQUALS(obj.is_managed(), true);
    CHECK_EQUALS(obj.list_int_col.size(), 2);

    CHECK_THROWS([&obj] { obj.list_int_col.push_back(1); });

    realm.write([&realm, &obj] {
        obj.list_int_col.push_back(1);
    });
    CHECK_EQUALS(obj.list_int_col.size(), 3);
    CHECK_EQUALS(obj.list_int_col.find(1), 2);
    CHECK_EQUALS(obj.list_int_col[2], 1);

    realm.write([&realm, &obj] {
        obj.list_int_col.pop_back();
    });
    CHECK_EQUALS(obj.list_int_col.size(), 2);
    CHECK_EQUALS(obj.list_int_col.find(1), UINTMAX_MAX);

    realm.write([&realm, &obj] {
        obj.list_int_col.erase(0);
    });
    CHECK_EQUALS(obj.list_int_col.size(), 1);

    realm.write([&realm, &obj] {
        obj.list_int_col.clear();
    });
    CHECK_EQUALS(obj.list_int_col.size(), 0);

    co_return;
}

TEST(list_insert_remove_object) {
    auto obj = AllTypesObject();
    CHECK_EQUALS(obj.is_managed(), false);

    auto o1 = AllTypesObjectLink();
    o1._id = 1;
    o1.str_col = "foo";
    auto o2 = AllTypesObjectLink();
    o2._id = 2;
    o2.str_col = "bar";
    auto o3 = AllTypesObjectLink();
    o3._id = 3;
    o3.str_col = "baz";
    auto o4 = AllTypesObjectLink();
    o4._id = 4;
    o4.str_col = "foo baz";
    auto o5 = AllTypesObjectLink();
    o5._id = 5;
    o5.str_col = "foo bar";

    // unmanaged
    obj.list_obj_col.push_back(o1);
    obj.list_obj_col.push_back(o2);
    obj.list_obj_col.push_back(o3);
    CHECK_EQUALS(obj.list_obj_col.size(), 3);

    obj.list_obj_col.pop_back();
    CHECK_EQUALS(obj.list_obj_col.size(), 2);
    obj.list_obj_col.erase(0);
    CHECK_EQUALS(obj.list_obj_col.size(), 1);
    obj.list_obj_col.clear();
    CHECK_EQUALS(obj.list_obj_col.size(), 0);
    obj.list_obj_col.push_back(o1);
    obj.list_obj_col.push_back(o2);
    obj.list_obj_col.push_back(o3);
    obj.list_obj_col.push_back(o4);
//    size_t index = obj.list_obj_col.find(o5);
//    CHECK_EQUALS(obj.list_obj_col.find(o5), 0);
//    CHECK_EQUALS(obj.list_obj_col[1], o5); operands issue

    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    realm.write([&realm, &obj] {
        realm.add(obj);
    });

    // ensure values exist
    CHECK_EQUALS(obj.is_managed(), true);
    CHECK_EQUALS(obj.list_obj_col.size(), 4);

    CHECK_THROWS([&] { obj.list_obj_col.push_back(o5); });

    CHECK_EQUALS(o5.is_managed(), false);
    realm.write([&obj, &o5] {
        obj.list_obj_col.push_back(o5);
    });
    CHECK_EQUALS(o5.is_managed(), true);

    CHECK_EQUALS(obj.list_obj_col.size(), 5);
    CHECK_EQUALS(obj.list_obj_col.find(o5), 4);
//    CHECK_EQUALS(obj.list_obj_col[2], o1); //operands issue

    realm.write([&obj] {
        obj.list_obj_col.pop_back();
    });
    CHECK_EQUALS(obj.list_obj_col.size(), 4);
    CHECK_EQUALS(obj.list_obj_col.find(o5), UINTMAX_MAX);

    realm.write([&realm, &obj] {
        obj.list_obj_col.erase(0);
    });
    CHECK_EQUALS(obj.list_obj_col.size(), 3);

    realm.write([&realm, &obj] {
        obj.list_obj_col.clear();
    });
    CHECK_EQUALS(obj.list_obj_col.size(), 0);

    co_return;
}

TEST(notifications_insertions) {
    auto obj = AllTypesObject();

    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    realm.write([&realm, &obj] {
        realm.add(obj);
    });

    bool did_run = false;

    CollectionChange change;

    int callback_count = 0;

    auto require_change = [&] {
        auto token = obj.list_int_col.observe([&](auto& collection, CollectionChange c, std::exception_ptr) {
            callback_count++;
            change = std::move(c);
        });
        return token;
    };

    auto token = require_change();
    realm.write([&realm, &obj] {
        obj.list_int_col.push_back(456);
    });

    realm.write([] { });

    CHECK_EQUALS(change.insertions.size(), 1);

    realm.write([&realm, &obj] {
        obj.list_int_col.push_back(456);
    });

    realm.write([] { });

    CHECK_EQUALS(change.insertions.size(), 1);
    CHECK_EQUALS(callback_count, 3);

    co_return;
}

TEST(notifications_deletions) {
    auto obj = AllTypesObject();

    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    realm.write([&realm, &obj] {
        realm.add(obj);
        obj.list_int_col.push_back(456);
    });

    bool did_run = false;

    CollectionChange change;

    auto require_change = [&] {
        auto token = obj.list_int_col.observe([&](auto& collection, CollectionChange c, std::exception_ptr) {
            did_run = true;
            change = std::move(c);
        });
        return token;
    };

    auto token = require_change();
    realm.write([&realm, &obj] {
        obj.list_int_col.erase(0);
    });
    realm.write([] { });
    CHECK_EQUALS(change.deletions.size(), 1);
    CHECK_EQUALS(did_run, true);

    co_return;
}

TEST(notifications_modifications) {
    auto obj = AllTypesObject();

    auto realm = realm::open<AllTypesObject, AllTypesObjectLink, Dog>({.path=path});
    realm.write([&realm, &obj] {
        realm.add(obj);
        obj.list_int_col.push_back(123);
        obj.list_int_col.push_back(456);
    });

    bool did_run = false;

    CollectionChange change;

    auto require_change = [&] {
        auto token = obj.list_int_col.observe([&](auto& collection, CollectionChange c, std::exception_ptr) {
            did_run = true;
            change = std::move(c);
        });
        return token;
    };

    auto token = require_change();
    realm.write([&realm, &obj] {
        obj.list_int_col.set(1, 345);
    });
    realm.write([] { });

    CHECK_EQUALS(change.modifications.size(), 1);
    CHECK_EQUALS(change.modifications[0], 1);
    CHECK_EQUALS(did_run, true);

    co_return;
}
