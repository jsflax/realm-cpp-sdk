#include <sdk.hpp>
#import <XCTest/XCTest.h>


struct Person: realm::Object {
    realm::sdk::Persisted<std::string> name;
    realm::sdk::Persisted<int> age;

    static constexpr auto schema() {
        return realm::sdk::Model {
            "Person",
            realm::sdk::property2 {"name", &Person::name},
            realm::sdk::property2 {"age", &Person::age}
        };
    }
};

constexpr auto _ = realm::sdk::make_schema(Person::schema());

@interface TestMain : XCTestCase
@end

@implementation TestMain
template<class TupType, size_t... I>
void print(const TupType& _tup, std::index_sequence<I...>)
{
    std::cout << "(";
    (..., (std::cout << (I == 0? "" : ", ") << std::get<I>(_tup)));
    std::cout << ")\n";
}

template<class... T>
void print (const std::tuple<T...>& _tup)
{
    print(_tup, std::make_index_sequence<sizeof...(T)>());
}
- (void)testAll {
    realm::sdk::default_schema = realm::sdk::make_schema(Person::schema());
//    print(realm::sdk::default_schema);
//    auto realm = realm::sdk::Realm({ .schema = {Person::schema()} });
//
//    auto person = Person();
//    person.name = "John";
//
//    realm.write([&realm, &person] {
//        realm.add(person);
//    });
}

@end

int main() {
    auto realm = realm::sdk::Realm({ .schema = {Person::schema()} });

    auto person = Person();
    person.name = "John";

    realm.write([&realm, &person] {
        realm.add(person);
    });
}
