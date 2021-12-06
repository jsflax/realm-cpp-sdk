#include <sdk.hpp>
#import <XCTest/XCTest.h>

struct Dog: realm::sdk::Object {
    realm::sdk::Persisted<std::string> name;
    realm::sdk::Persisted<int> age;

    static constexpr auto schema() {
        return realm::sdk::Model {
            "Dog",
            realm::sdk::property {"name", &Dog::name},
            realm::sdk::property {"age", &Dog::age}
        };
    }
};

struct Person: realm::sdk::Object {
    realm::sdk::Persisted<std::string> name;
    realm::sdk::Persisted<int> age;
    realm::sdk::Persisted<std::optional<Dog>> dog;

    static constexpr auto schema() {
        return realm::sdk::Model {
            "Person",
            realm::sdk::property {"name", &Person::name},
            realm::sdk::property {"age", &Person::age},
            realm::sdk::property {"dog", &Person::dog},
        };
    }
};

@interface TestMain : XCTestCase
@end

@implementation TestMain

- (void)testAll {
    auto realm = realm::sdk::Realm<Person, Dog>();

    auto person = Person();
    person.name = "John";
    person.age = 17;
    person.dog = Dog{.name = "Fido"};

    realm.write([&realm, &person] {
        realm.add(person);
    });

    XCTAssertEqual(*person.name, "John");
    XCTAssertEqual(*person.age, 17);
    auto dog = **person.dog;
    std::cout<<*dog.name<<std::endl;
    XCTAssertEqual(*dog.name, "Fido");

    realm.write([&person] {
        person.age = 21;
    });

    XCTAssertEqual(*person.age, 21);

    auto persons = realm.objects<Person>();
    XCTAssertEqual(persons.size(), 1);

    std::vector<Person> people;
    std::copy(persons.begin(), persons.end(), std::back_inserter(people));
    for (auto& person:people) {
        realm.write([&person, &realm]{
            realm.remove(person);
        });
    }

    XCTAssertEqual(persons.size(), 0);
}

@end

int main() {
//    auto realm = realm::sdk::Realm({ .schema = {Person::schema()} });
//
//    auto person = Person();
//    person.name = "John";
//
//    realm.write([&realm, &person] {
//        realm.add(person);
//    });
}
