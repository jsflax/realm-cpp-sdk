#include <cpprealm/sdk.hpp>

struct Dog: realm::object {
    realm::persisted<std::string> name;
    realm::persisted<int> age;

    using schema = realm::schema<"Dog",
                                 realm::property<"name", &Dog::name>,
                                 realm::property<"age", &Dog::age>>;
};

struct Person: realm::object {
    realm::persisted<std::string> name;
    realm::persisted<int> age;
    realm::persisted<std::optional<Dog>> dog;

    using schema = realm::schema<"Person",
                                 realm::property<"name", &Person::name>,
                                 realm::property<"age", &Person::age>,
                                 realm::property<"dog", &Person::dog>>;
};

struct AllTypesObject: realm::object {
    enum Enum {
        one, two
    };

    realm::persisted<int> _id;
    realm::persisted<Enum> enum_col;

    using schema = realm::schema<
        "AllTypesObject",
        realm::property<"_id", &AllTypesObject::_id, true>>;;
};

static auto success_count = 0;
static auto fail_count = 0;
template <typename T, typename V>
bool assert_equals(const T& a, const V& b)
{
    if (a == b) { success_count += 1; }
    else { fail_count += 1; }
    return a == b;
}

#define assert_equals(a, b) \
if (!assert_equals(a, b)) {\
std::cout<<__FILE__<<"L"<<__LINE__<<":"<<#a<<" did not equal "<<#b<<std::endl;\
}

realm::task<void> testAll() {
    auto realm = realm::open<Person, Dog>();

    auto person = Person();
    person.name = "John";
    person.age = 17;
    person.dog = Dog{.name = "Fido"};

    realm.write([&realm, &person] {
        realm.add(person);
    });

    assert_equals(*person.name, "John");
    assert_equals(*person.age, 17);
    auto dog = **person.dog;
    assert_equals(*dog.name, "Fido");

    auto token = person.observe<Person>([](auto&& change) {
        assert_equals(change.property.name, "age");
        assert_equals(std::any_cast<int>(*change.property.new_value), 19);
    });

    realm.write([&person] {
        person.age += 2;
    });

    assert_equals(*person.age, 19);

    auto persons = realm.objects<Person>();
    assert_equals(persons.size(), 1);

    std::vector<Person> people;
    std::copy(persons.begin(), persons.end(), std::back_inserter(people));
    for (auto& person:people) {
        realm.write([&person, &realm]{
            realm.remove(person);
        });
    }

    assert_equals(persons.size(), 0);
    auto app = realm::App("car-wsney");
    auto user = co_await app.login(realm::App::Credentials::anonymous());

    auto tsr = co_await user.realm<AllTypesObject>("foo");
    auto synced_realm = tsr.resolve();
    synced_realm.write([&synced_realm]() {
        synced_realm.add(AllTypesObject{._id=1});
    });

    assert_equals(*synced_realm.object<AllTypesObject>(1)._id, 1);

    co_return;
}

realm::task<void> testThreadSafeReference() {
    auto realm = realm::open<Person, Dog>();

    auto person = Person { .name = "John", .age = 17 };
    person.dog = Dog {.name = "Fido"};

    realm.write([&realm, &person] {
        realm.add(person);
    });

    auto tsr = realm::thread_safe_reference<Person>(person);
    std::condition_variable cv;
    std::mutex cv_m;
    bool done;
    auto t = std::thread([&cv, &tsr, &done]() {
        auto realm = realm::open<Person, Dog>();
        auto person = realm.resolve(std::move(tsr));
        assert_equals(*person.age, 17);
        realm.write([&] { realm.remove(person); });
    });
    t.join();
    co_return;
}

struct Foo: realm::object {
    realm::persisted<int> bar;
    Foo() = default;
    Foo(const Foo&) = delete;
    using schema = realm::schema<"Foo", realm::property<"bar", &Foo::bar>>;
};

int main() {
    try {
        auto tasks = { testAll(), testThreadSafeReference() };
        {
            while (std::transform_reduce(tasks.begin(), tasks.end(), true,
                                         [](bool done1, bool done2) -> bool { return done1 && done2; },
                                         [](const realm::task<void>& task) -> bool { return task.handle.done(); }) == false) {
            };
        }
    } catch (const std::exception &e) {
        std::cout<<e.what()<<std::endl;;
    }
    std::cout<<success_count<<"/"<<success_count + fail_count<<" checks completed successfully."<<std::endl;
    return 0;
}

//@end
