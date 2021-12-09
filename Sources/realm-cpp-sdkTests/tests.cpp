#include <sdk.hpp>

struct Dog: realm::sdk::Object {
    realm::sdk::Persisted<std::string> name;
    realm::sdk::Persisted<int> age;

    using schema = realm::sdk::schema<"Dog",
                                      realm::sdk::property<"name", &Dog::name>,
                                      realm::sdk::property<"age", &Dog::age>>;
};

struct Person: realm::sdk::Object {
    realm::sdk::Persisted<std::string> name;
    realm::sdk::Persisted<int> age;
    realm::sdk::Persisted<std::optional<Dog>> dog;

    using schema = realm::sdk::schema<"Person",
                                      realm::sdk::property<"name", &Person::name>,
                                      realm::sdk::property<"age", &Person::age>,
                                      realm::sdk::property<"dog", &Person::dog>>;
};

struct AllTypesObject: realm::sdk::Object {
    realm::sdk::Persisted<int> id;

    using schema = realm::sdk::schema<
        "AllTypesObject",
        realm::sdk::property<"id", &AllTypesObject::id, true>>;
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

realm::sdk::task<void> run() {
    auto realm = realm::sdk::Realm<Person, Dog, AllTypesObject>();

    auto person = Person();
    person.name = "John";
    person.age = 17;
    person.dog = Dog{.name = "Fido"};
    person.age -= 2;
    realm.write([&realm, &person] {
        realm.add(person);
    });

    assert_equals(*person.name, "John");
    assert_equals(*person.age, 15);
    auto dog = **person.dog;
    assert_equals(*dog.name, "Fido");

    auto token = realm::sdk::observe<Person>(person, [](const Person& person,
                                                        std::vector<std::string> property_names,
                                                        std::vector<std::any> old_values,
                                                        std::vector<std::any> new_values,
                                                        std::exception_ptr error) {
        assert_equals(std::count(property_names.begin(), property_names.end(), "age"), 1);
        assert_equals(property_names.size(), 1);

        assert_equals(new_values.size(), 1);
        assert_equals(std::any_cast<int>(new_values[0]), 19);
    });

    realm.write([&person] {
        person.age = 21;
        person.age -= 2;
    });

    assert_equals(*person.age, 19);
    assert(person.age <= 19);
    auto persons = realm.objects<Person>();
    assert_equals(persons.size(), 1);

    realm.write([&realm]{
        realm.add(AllTypesObject{.id = 1});
    });

    std::vector<Person> people;
    std::copy(persons.begin(), persons.end(), std::back_inserter(people));
    for (auto& person:people) {
        realm.write([&person, &realm]{
            realm.remove(person);
        });
    }

    assert_equals(persons.size(), 0);
    auto app = realm::sdk::App("todo-cqenc");
    auto user = co_await app.login(realm::sdk::App::Credentials::anonymous());
    assert(!user.access_token().empty());
    auto synced_realm = co_await user.realm<Person, Dog>("foo");
    synced_realm.write([&synced_realm]() {
        synced_realm.add(Person{.name="Zoe"});
    });

    assert_equals(*realm.object<AllTypesObject>(1).id, 1);
    co_return;
}

int main() {
    auto task = run();
    auto coro = task.p->coro();
    coro.resume();
    task.await_ready();

    std::cout<<success_count<<"/"<<success_count + fail_count<<" checks completed successfully."<<std::endl;
    return 0;
}

//@end
