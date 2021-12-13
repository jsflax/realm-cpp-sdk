#ifndef realm_persisted_hpp
#define realm_persisted_hpp

#include <cpprealm/type_info.hpp>

namespace realm {

struct FieldValue;

/// `realm::persisted` is used to declare properties on `realm::object` subclasses which should be
/// managed by Realm.
///
/// Example of usage:
/// ```cpp
/// class MyModel: realm::object {
///     // A basic property declaration. A property with no
///     // default value supplied will default to `nil` for
///     // Optional types, zero for numeric types, false for Bool,
///     // an empty string/data, and a new random value for UUID
///     // and ObjectID.
///     realm::persisted<int> basic_int_property;
///
///     // Custom default values can be specified with the
///     // standard c++ syntax
///     realm::persisted<int> int_with_custom_default = 5;
///
///     // Primary key properties can be picked in schema.
///     realm::persisted<int> var _id;
///
///     // Properties which are not marked with `persisted` will
///     // be ignored entirely by Realm.
///     bool ignoredProperty = true
///
///     using schema = realm::schema<"MyModel",
///                             realm::property<"basic_int_property", &MyModel::basic_int_property>,
///                             realm::property<"int_with_custom_default", &MyModel::int_with_custom_default>,
///                             realm::property<"_id", &MyModel::_id, true>>; // primary key property
/// }
/// ```
///
///  A property can be set as the class's primary key by passing `true`
///  into its schema property. Compound primary keys are not supported, and setting
///  more than one property as the primary key will throw an exception at
///  runtime. Only Int, String, UUID and ObjectID properties can be made the
///  primary key, and when using MongoDB Realm, the primary key must be named
///  `_id`. The primary key property can only be mutated on unmanaged objects,
///  and mutating it on an object which has been added to a Realm will throw an
///  exception.
///
///  Properties can optionally be given a default value using the standard C++20
///  syntax. If no default value is given, a value will be generated on first
///  access: `nil` for all Optional types, zero for numeric types, false for
///  Bool, an empty string/data, and a new random value for UUID and ObjectID.
///  Doing so will work, but will result in worse performance when accessing
///  objects managed by a Realm. Similarly, ObjectID properties *should not* be initialized to
///  `ObjectID.generate()`, as doing so will result in extra ObjectIDs being
///  generated and then discarded when reading from a Realm.
template <realm::type_info::Persistable T>
struct persisted {
    using Result = T;
    using type = typename realm::type_info::persisted_type<T>::type;

    persisted(const T& value);
    persisted(T&& value);
    persisted(const char* value) requires realm::type_info::StringPersistable<T>;
    template <typename S>
    requires (type_info::ObjectPersistable<T>)
    persisted(std::optional<S> value);

    persisted();
    ~persisted();
    persisted(const persisted& o);
    persisted(persisted&& o);

    template <typename S>
    requires (type_info::StringPersistable<T>) && std::is_same_v<S, const char*>
    persisted& operator=(S o);
    persisted& operator=(const T& o);
    persisted& operator=(const persisted& o);
    persisted& operator=(persisted&& o);
    T operator *() const;

    void operator -=(const T& a) requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>);
    void operator --() requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>);
    T operator -() requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>);
    void operator +=(const T& a) requires (type_info::AddAssignable<T>);
    T operator *(const T& a) requires (type_info::AddAssignable<T>);
    void operator ++() requires (type_info::AddAssignable<T>);
    bool operator <(const T& a) requires (type_info::Comparable<T>);
    bool operator >(const T& a) requires (type_info::Comparable<T>);
    bool operator <=(const T& a) requires (type_info::Comparable<T>);
    bool operator >=(const T& a) requires (type_info::Comparable<T>);
    union {
        T unmanaged;
        realm::ColKey managed;
    };

private:
    template <StringLiteral Name, auto Ptr, bool>
    friend struct property;
    template <StringLiteral, type_info::Propertyable ...Properties>
    friend struct schema;

    type as_core_type() const;
    void assign(const Obj& object, const ColKey& col_key);
    std::optional<Obj> m_obj;
};

// MARK: Implementation

template <realm::type_info::Persistable T>
persisted<T>::persisted() {
    new (&unmanaged) T();
}
template <realm::type_info::Persistable T>
persisted<T>::persisted(const T& value) {
    unmanaged = value;
}
template <realm::type_info::Persistable T>
persisted<T>::persisted(T&& value) {
    unmanaged = std::move(value);
}
template <realm::type_info::Persistable T>
persisted<T>::persisted(const char* value) requires realm::type_info::StringPersistable<T> {
    new (&unmanaged) std::string(value);
}
template <realm::type_info::Persistable T>
template <typename S>
requires (type_info::ObjectPersistable<T>)
persisted<T>::persisted(std::optional<S> value) {
    unmanaged = value;
}
template <realm::type_info::Persistable T>
persisted<T>::~persisted()
{
    if constexpr (realm::type_info::property_type<T>() == PropertyType::String) {
        using std::string;
        if (!m_obj)
            unmanaged.~string();
    }
}
template <realm::type_info::Persistable T>
persisted<T>::persisted(const persisted& o) {
    *this = o;
}
template <realm::type_info::Persistable T>
persisted<T>::persisted(persisted&& o) {
    *this = std::move(o);
}
template <realm::type_info::Persistable T>
template <typename S>
requires (type_info::StringPersistable<T>) && std::is_same_v<S, const char*>
persisted<T>& persisted<T>::operator=(S o) {
    if (auto obj = m_obj) {
        obj->template set<type>(managed, o);
    } else {
        unmanaged = o;
    }
    return *this;
}

template <realm::type_info::Persistable T>
persisted<T>& persisted<T>::operator=(const T& o) {
    if (auto obj = m_obj) {
        // if parent is managed...
        if constexpr (type_info::OptionalObjectPersistable<T>) {
            // if object...
            if (auto link = o) {
                // if non null object is being assigned...
                if (link->m_obj) {
                    // if object is managed, we will to set the link
                    // to the new target's key
                    obj->template set<type>(managed, link->m_obj->get_key());
                } else {
                    // else new unmanaged object is being assigned.
                    // we must assign the values to this object's fields
                    // TODO:
                }
            } else {
                // else null link is being assigned to this field
                // e.g., `person.dog = std::nullopt;`
                // set the parent column to null and unset the co
                obj->set_null(managed);
            }
        } else {
            obj->template set<type>(managed, o);
        }
    } else {
        new (&unmanaged) T(o);
    }
    return *this;
}

template <realm::type_info::Persistable T>
persisted<T>& persisted<T>::operator=(const persisted& o) {
    if (auto obj = o.m_obj) {
        m_obj = obj;
        new (&managed) ColKey(o.managed);
    } else {
        new (&unmanaged) T(o.unmanaged);
    }
    return *this;
}

template <realm::type_info::Persistable T>
persisted<T>& persisted<T>::operator=(persisted&& o) {
    if (o.m_obj) {
        m_obj = o.m_obj;
        new (&managed) ColKey(std::move(o.managed));
    } else {
        new (&unmanaged) T(std::move(o.unmanaged));
    }
    return *this;
}

template <realm::type_info::Persistable T>
T persisted<T>::operator *() const
{
    if (m_obj) {
        if constexpr (type_info::OptionalPersistable<T>) {
            if constexpr (type_info::ObjectPersistable<typename T::value_type>) {
                return T::value_type::schema::create(m_obj->get_linked_object(managed), nullptr);
            } else {
                auto value = m_obj->template get<type>(managed);
                // convert optionals
                if (value) {
                    return T(*value);
                } else {
                    return T();
                }
            }
        } else {
            return static_cast<T>(m_obj->template get<type>(managed));
        }
    } else {
        return unmanaged;
    }
}

template <realm::type_info::Persistable T>
typename persisted<T>::type persisted<T>::as_core_type() const
{
    if constexpr (type_info::ObjectPersistable<T>) {
        if (m_obj) {
            return m_obj->template get<type>(managed);
        } else {
            REALM_ASSERT(false);
            return ObjKey{}; /* should never happen */
        }
    } else {
        if (m_obj) {
            return m_obj->template get<type>(managed);
        } else {
            return type_info::convert_if_required<T>(unmanaged);
        }
    }
}

template <realm::type_info::Persistable T>
void persisted<T>::operator -=(const T& a) requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>) {
    if (m_obj) {
        m_obj->template set<type>(managed, *(*this) - a);
    } else {
        unmanaged -= a;
    }
}

template <realm::type_info::Persistable T>
void persisted<T>::operator --() requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>) {
    *this -= 1;
}

template <realm::type_info::Persistable T>
T persisted<T>::operator -() requires (type_info::IntPersistable<T> || type_info::DoublePersistable<T>) {
    return *this * -1;
}

template <realm::type_info::Persistable T>
void persisted<T>::operator +=(const T& a) requires (type_info::AddAssignable<T>) {
    if (m_obj) {
        m_obj->template set<type>(managed, *(*this) + a);
    } else {
        unmanaged += a;
    }
}

template <realm::type_info::Persistable T>
T persisted<T>::operator *(const T& a) requires (type_info::AddAssignable<T>) {
    return **this * a;
}

template <realm::type_info::Persistable T>
void persisted<T>::operator ++() requires (type_info::AddAssignable<T>) {
    *this += 1;
}
template <realm::type_info::Persistable T>
bool persisted<T>::operator <(const T& a) requires (type_info::Comparable<T>) {
    return **this < a;
}
template <realm::type_info::Persistable T>
bool persisted<T>::operator >(const T& a) requires (type_info::Comparable<T>) {
    return **this > a;
}
template <realm::type_info::Persistable T>
bool persisted<T>::operator <=(const T& a) requires (type_info::Comparable<T>) {
    return **this <= a;
}
template <realm::type_info::Persistable T>
bool persisted<T>::operator >=(const T& a) requires (type_info::Comparable<T>) {
    return **this >= a;
}

template <realm::type_info::Persistable T>
void persisted<T>::assign(const Obj& object, const ColKey& col_key) {
    m_obj = object;
    new (&managed) ColKey(col_key);
}

}

#endif /* Header_h */
