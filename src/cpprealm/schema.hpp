#ifndef realm_schema_hpp
#define realm_schema_hpp

#include <cpprealm/persisted.hpp>

#include <realm/object-store/object_schema.hpp>
#include <realm/object-store/shared_realm.hpp>

namespace realm {

namespace {

template <typename T>
struct ptr_type_extractor_base;
template <typename Result, typename Class>
struct ptr_type_extractor_base<Result Class::*>
{
    using class_type = Class;
    using member_type = Result;
};

template <auto T>
struct ptr_type_extractor : ptr_type_extractor_base<decltype(T)> {
};

}

// MARK: property
using IsPrimaryKey = bool;
template <StringLiteral Name, auto Ptr, IsPrimaryKey IsPrimaryKey = false>
struct property {
    using Result = typename ptr_type_extractor<Ptr>::member_type::Result;
    using Class = typename ptr_type_extractor<Ptr>::class_type;

    constexpr property()
    : type(type_info::property_type<Result>())
    {
    }

    explicit operator property() {
        return property();
    }

    explicit operator realm::Property() const {
        if constexpr (type_info::OptionalPersistable<Result>) {
            if constexpr (type_info::ObjectPersistable<typename Result::value_type>) {
                return realm::Property(name, type, Result::value_type::schema::name);
            } else {
                return realm::Property(name, type);
            }
        } else {
            return realm::Property(name, type, is_primary_key);
        }
    }

    static void assign(Class& object, ColKey col_key, SharedRealm realm) {
        (object.*Ptr).assign(*object.m_obj, col_key);
    }

    static constexpr const char* name = Name.value;
    static constexpr persisted<Result> Class::*ptr = Ptr;
    PropertyType type;
    static constexpr bool is_primary_key = IsPrimaryKey;
};

// MARK: schema
template <StringLiteral Name, type_info::Propertyable ...Properties>
struct schema {
    using Class = typename std::tuple_element_t<0, std::tuple<Properties...>>::Class;
    static constexpr const char* name = Name.value;
    static constexpr const std::tuple<Properties...> properties{};

    template <size_t N, type_info::Propertyable P>
    static constexpr auto primary_key(P&)
    {
        if constexpr (P::is_primary_key) {
            return P();
        } else {
            if constexpr (N + 1 == sizeof...(Properties)) {
                return;
            } else {
                return primary_key<N + 1>(std::get<N + 1>(properties));
            }
        }
    }
    static constexpr auto primary_key() {
        return primary_key<0>(std::get<0>(properties));
    }


    using PrimaryKeyProperty = decltype(primary_key());
    static constexpr bool HasPrimaryKeyProperty = !std::is_void_v<PrimaryKeyProperty>;

    static std::vector<FieldValue> to_persisted_values(Class& cls, TableRef& table) {
        std::vector<FieldValue> values;
        (push_back_field(values, table, table->get_column_key(Properties::name), cls.*Properties::ptr, Properties(), cls), ...);
        return values;
    }

    static realm::ObjectSchema to_core_schema()
    {
        realm::ObjectSchema schema;
        schema.name = Name;
        (schema.persisted_properties.push_back(static_cast<realm::Property>(Properties())) , ...);
        if constexpr (HasPrimaryKeyProperty) {
            schema.primary_key = PrimaryKeyProperty::name;
        }
        return schema;
    }

    static void initialize(Class& cls, Obj&& obj, SharedRealm realm)
    {
        cls.m_obj = std::move(obj);
        cls.m_realm = realm;
        (Properties::assign(cls, cls.m_obj->get_table()->get_column_key(Properties::name), realm), ...);
    }

    static Class create(Obj&& obj, SharedRealm realm)
    {
        Class cls;
        initialize(cls, std::move(obj), realm);
        return cls;
    }
    static Class* create_new(Obj&& obj, SharedRealm realm)
    {
        auto cls = new Class();
        initialize(*cls, std::move(obj), realm);
        return cls;
    }
    static std::unique_ptr<Class> create_unique(Obj&& obj, SharedRealm realm)
    {
        auto cls = std::make_unique<Class>();
        initialize(*cls, std::move(obj), realm);
        return cls;
    }
private:
    template <type_info::OptionalObjectPersistable T>
    static void push_back_field(std::vector<FieldValue>& values,
                                TableRef& table, ColKey key, persisted<T>& field,
                                auto property, auto& cls)
    {
        if (*field) {
            if (field.m_obj) {
                values.push_back({key, field.as_core_type()});
            } else {
                auto target_table = table->get_link_target(key);
                auto target_cls = *field;

                Obj obj;
                if constexpr (T::value_type::schema::HasPrimaryKeyProperty) {
                    obj = target_table->create_object_with_primary_key((**field).*T::value_type::schema::PrimaryKeyProperty::ptr, T::value_type::schema::to_persisted_values(*target_cls, target_table));
                } else {
                    obj = target_table->create_object(ObjKey{}, T::value_type::schema::to_persisted_values(*target_cls, target_table));
                }

                property.assign(cls, key, nullptr);
                values.push_back({key, obj.get_key()});
            }
        }
    }

    template <type_info::PrimitivePersistable T>
    static void push_back_field(std::vector<FieldValue>& values,
                                TableRef&, ColKey key, persisted<T>& field, auto, auto&)
    {
        values.push_back({key, field.as_core_type()});
    }
};

}

#endif /* realm_schema_hpp */
