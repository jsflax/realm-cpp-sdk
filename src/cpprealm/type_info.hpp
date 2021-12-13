#ifndef realm_type_info_hpp
#define realm_type_info_hpp

#include <optional>
#include <concepts>

#include <realm/object-store/property.hpp>
#include <realm/obj.hpp>

namespace realm::type_info {

template <typename T>
struct persisted_type {};

template <typename T>
concept AddAssignable = requires (T a) {
    a += a;
};
template <typename T>
concept Comparable = requires (T a) {
    a < a;
    a > a;
    a <= a;
    a >= a;
};
template <typename T>
concept IntPersistable = std::is_integral<T>::value && requires(T a) {
    { static_cast<realm::Int>(a) };
};
template <IntPersistable T>
struct persisted_type<T> { using type = realm::Int; };
template <typename T>
concept BoolPersistable = std::is_same_v<T, bool> && requires(T a) {
    { static_cast<realm::Bool>(a) };
};
template <BoolPersistable T>
struct persisted_type<T> { using type = realm::Bool; };
template <typename T>
concept EnumPersistable = std::is_enum_v<T> && requires(T a) {
    { static_cast<realm::Int>(a) };
};
template <EnumPersistable T>
struct persisted_type<T> { using type = realm::Int; };
template <typename T>
concept StringPersistable = requires(T a) {
    { static_cast<realm::String>(a) };
};
template <StringPersistable T>
struct persisted_type<T> { using type = StringData; };

template <typename T>
concept DoublePersistable = std::is_floating_point_v<T> && requires(T a) {
    { static_cast<realm::Double>(a) };
};
template <DoublePersistable T>
struct persisted_type<T> { using type = double; };

template <class T>
struct is_optional : std::false_type {
    using type = std::false_type::value_type;
    static constexpr bool value = std::false_type::value;
};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {
    using type = std::true_type::value_type;
    static constexpr bool value = std::true_type::value;
};

template <typename T>
concept Optional = is_optional<T>::value;

template <typename T>
concept ObjectPersistable = requires(T a) {
    { std::is_same_v<typename T::schema::Class, T> };
};
template <ObjectPersistable T>
struct persisted_type<T> { using type = realm::ObjKey; };

template <typename T>
concept PrimitivePersistable = IntPersistable<T> || BoolPersistable<T> || StringPersistable<T> || EnumPersistable<T> || DoublePersistable<T>;

template <typename T>
concept NonOptionalPersistable = PrimitivePersistable<T> || ObjectPersistable<T>;

// MARK: ListPersistable
template <typename T>
concept ListPersistable = NonOptionalPersistable<typename T::value_type> && std::is_same_v<std::vector<typename T::value_type>, T> && requires(T a) {
    typename T::value_type;
    typename T::size_type;
    typename T::allocator_type;
    typename T::iterator;
    typename T::const_iterator;
    { a.size() };
    { a.begin() };
    { a.end() };
    { a.cbegin() };
    { a.cend() };
};
template <ListPersistable T>
struct persisted_type<T> { using type = std::vector<typename persisted_type<typename T::value_type>::type>; };

template <typename T>
concept OptionalObjectPersistable = Optional<T> && ObjectPersistable<typename T::value_type>;
template <typename T>
concept OptionalPersistable = Optional<T> && NonOptionalPersistable<typename T::value_type>;
template <OptionalPersistable T>
struct persisted_type<T> { using type = util::Optional<persisted_type<typename T::value_type>>; };
template <ObjectPersistable T>
struct persisted_type<std::optional<T>> { using type = realm::ObjKey; };
template <typename T>
concept NonContainerPersistable = NonOptionalPersistable<T> || OptionalPersistable<T>;
template <typename T>
concept Persistable = NonOptionalPersistable<T> || OptionalPersistable<T> || ListPersistable<T>;

template <PrimitivePersistable T>
constexpr typename persisted_type<T>::type convert_if_required(const T& a)
{
    return a;
}
template <OptionalObjectPersistable T>
static constexpr typename persisted_type<T>::type convert_if_required(const T& a)
{
    if (a) { return a->m_obj ? a->m_obj->get_key() : ObjKey{}; }
    else { return ObjKey{}; }
}
template <ListPersistable T>
static constexpr typename persisted_type<T>::type convert_if_required(const T& a)
{
    typename persisted_type<T>::type v;
    std::transform(a.begin(), a.end(), std::back_inserter(v), [](auto& value) {
        if constexpr (ObjectPersistable<typename T::value_type>) {
            return value.m_obj->get_key();
        } else {
            return static_cast<typename persisted_type<typename T::value_type>::type>(value);
        }
    });
    return v;
}
template <typename T>
static constexpr PropertyType property_type();

template<> constexpr PropertyType property_type<int>() { return PropertyType::Int; }
template<> constexpr PropertyType property_type<std::string>() { return PropertyType::String; }
template<ObjectPersistable T> static constexpr PropertyType property_type() { return PropertyType::Object | PropertyType::Nullable; }
template<OptionalPersistable T> static constexpr PropertyType property_type() {
    return property_type<typename T::value_type>() | PropertyType::Nullable;
}
template<DoublePersistable T> static constexpr PropertyType property_type() {
    return PropertyType::Double;
}
template<EnumPersistable T> static constexpr PropertyType property_type() {
    return PropertyType::Int;
}
template<ListPersistable T> static constexpr PropertyType property_type() {
    return PropertyType::Array | property_type<typename T::value_type>();
}
template<ListPersistable T> static constexpr PropertyType property_type()
requires (ObjectPersistable<typename T::value_type>) {
    return PropertyType::Array | PropertyType::Object;
}
template <typename T>
concept Propertyable = requires(T a) {
    { std::is_same_v<std::string, decltype(a.name)> };
    { std::is_same_v<typename T::Result T::Class::*, decltype(a.ptr)> };
    { std::is_same_v<PropertyType, decltype(a.type)> };
};
} // namespace realm::type_info

namespace realm {

template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    operator std::string() const { return value; }
    char value[N];
};

}
#endif /* realm_type_info_hpp */
