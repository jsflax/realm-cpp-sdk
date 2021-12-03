#ifndef Header_h
#define Header_h

#include <utility>
#include <realm/object-store/impl/object_accessor_impl.hpp>
#include <realm/object-store/shared_realm.hpp>
#include <realm/object-store/object.hpp>
#include <filesystem>
#include <iostream>

namespace realm {
struct Obj;
struct Realm;

using CoreRealm = Realm;

namespace sdk {

namespace {
}


// MARK: Object

struct Object {
    std::shared_ptr<Realm> realm;
private:
    template <typename T>
    friend struct Persisted;
    Obj m_obj;
};

struct PersistedBase {
    Obj* m_obj;
};

template <typename T>
union PropertyStorage {
    T unmanaged;
    realm::ColKey managed;

    bool is_managed() const { return managed == realm::ColKey{}; };
    PropertyStorage() {}
    PropertyStorage(const PropertyStorage&) = default;
    PropertyStorage(PropertyStorage&&) = default;
    ~PropertyStorage() {}
};

template <>
union PropertyStorage<std::string> {
    std::string unmanaged;
    realm::ColKey managed;

    bool is_managed() const { return managed == realm::ColKey{}; };
    PropertyStorage() {}
    PropertyStorage(const PropertyStorage& o) {
        if (o.is_managed()) {
            unmanaged = o.unmanaged;
        } else {
            managed = o.managed;
        }
    }
    PropertyStorage& operator=(const PropertyStorage& o) {
        if (o.is_managed()) {
            unmanaged = o.unmanaged;
        } else {
            managed = o.managed;
        }
        return *this;
    }
//    PropertyStorage(PropertyStorage&& o) {
//        if (o.is_managed()) {
//            unmanaged = std::move(o.unmanaged);
//        } else {
//            managed = std::move(o.managed);
//        }
//    }
    PropertyStorage(const std::string& value) : unmanaged(value) {}
    PropertyStorage(std::string&& value) : unmanaged(std::move(value)) {}
    PropertyStorage(const char* value) : unmanaged(value) {}
    ~PropertyStorage() {
    }
};

template <typename T>
struct type_map {
    using U = void;
};
template<> struct type_map<int> { using U = Int; };

template <typename T>
struct Persisted: PersistedBase {
    using mapped_type = typename type_map<T>::U;

    Persisted() {}
    Persisted(const T& value) { storage.unmanaged = value; }
    Persisted(T&& value) { storage.unmanaged = std::move(value); }
    T operator ->()
    {
        if (storage.is_managed()) { return m_obj->get<mapped_type>(storage.managed); }
        else { return storage.unmanaged; }
    }
    T operator *()
    {
        if (storage.is_managed()) { return m_obj->get<mapped_type>(storage.managed); }
        else { return storage.unmanaged; }
    }

    mapped_type as_core_type() const
    {
        if (storage.is_managed()) { return m_obj->get<mapped_type>(storage.managed); }
        else { return storage.unmanaged; }
    }

    PropertyStorage<T> storage;
};

template<> struct Persisted<std::string>: PersistedBase {
    Persisted() {}
    Persisted(const Persisted&) = default;
//    Persisted(Persisted&&) = default;

    Persisted(const std::string& value)
    : storage(value)
    {
    }
    Persisted(std::string&& value)
    : storage(std::move(value))
    {
    }
    Persisted(const char* value)
    : storage(value)
    {
    }
    std::string operator ->()
    {
        if (storage.is_managed()) { return m_obj->get<StringData>(storage.managed); }
        else { return storage.unmanaged; }
    }
    std::string operator *()
    {
        if (storage.is_managed()) { return m_obj->get<StringData>(storage.managed); }
        else { return storage.unmanaged; }
    }
    std::string as_core_type() const
    {
        if (storage.is_managed()) { return m_obj->get<StringData>(storage.managed); }
        else { return storage.unmanaged; }
    }

    PropertyStorage<std::string> storage;
};

//namespace {
template<typename Class, typename Result>
struct property2;

struct property {
//    template <typename Class, typename Result>
//    constexpr property(property2<Class, Result>&& p) {
//
//    }
};

struct anyprop {
//    virtual void assign(Object&, Obj&) = 0;
};

struct Property {
    const char* name;
};

template <typename T>
struct PropertyAccessor {
    static void initialize(Property property, Object parent) {

    }
};

template <typename T>
PropertyType property_type();

template<> inline PropertyType property_type<int>() { return PropertyType::Int; }
template<> inline PropertyType property_type<std::string>() { return PropertyType::String; }

template<typename Class, typename Result>
struct property2: anyprop {
    constexpr property2(const char* name,
                        Persisted<Result> Class::*ptr)
    : ptr(ptr)
    , name(name)
    , type(property_type<Result>())
    {
    }

    explicit operator property() {
        return property();
    }

    explicit operator realm::Property() const {
        return realm::Property(name, type);
    }

    void assign(Class& object, Obj&& obj) {
        (object.*ptr).m_obj = &obj;
    }

    const char * name;
    Persisted<Result> Class::*ptr;
    PropertyType type;
};

struct ObjectSchema;

template <typename ...T>
static constexpr auto make_schema(T&&... types)
{
    return std::make_tuple(types...);
}

constexpr static auto default_schema = std::tuple<>{};

template<typename Class, typename ...Result>
struct Model {
    constexpr Model(const char* name, property2<Class, Result>&& ...properties)
    : name(name)
    , n(sizeof...(Result))
    , props{std::move(properties)...}
    {

    }


    const std::size_t n;
    std::tuple<property2<Class, Result>...> props;
    const char * name;
    explicit operator realm::ObjectSchema() const
    {
        realm::ObjectSchema schema;
        schema.name = name;
        std::apply([&schema](auto&&... props){
            (schema.persisted_properties.push_back(static_cast<realm::Property>(props)) , ...);
        }, props);
        return schema;
    }
};

struct ObjectSchema {
    template<typename Class, typename ...Result>
    constexpr ObjectSchema(Model<Class, Result...>&& model)
    : m_schema(static_cast<realm::ObjectSchema>(model))
    {
    }

    explicit operator realm::ObjectSchema() const
    {
        return m_schema;
    }
private:
    realm::ObjectSchema m_schema;
};

// MARK: Results
template <typename T>
struct Results {
    class iterator {
    public:
        using difference_type = size_t;
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::input_iterator_tag;

        bool operator!=(const iterator& other) const
        {
            return !(this == other);
        }

        bool operator==(const iterator& other) const
        {
            return (m_parent == other.m_parent) || (m_idx == other.m_idx);
        }

        reference operator*() const noexcept
        {
            return m_parent->m_parent->get(m_idx);
        }

        pointer operator->() const noexcept
        {
            return &m_parent->m_parent->get(m_idx);
        }

        iterator& operator++();
        iterator operator++(int);
    private:
        size_t m_idx;
        const Results<T>* m_parent;
    };
private:
    realm::Results m_parent;
};

// MARK: Realm
struct Realm {
    struct Config {
        std::initializer_list<ObjectSchema> schema;
        std::string path = std::filesystem::current_path().append("default.realm");
    };

    Realm(const Config& config)
    : config(std::move(config))
    , m_realm(CoreRealm::get_shared_realm({ .path = this->config.path }))
    {
    }
    Realm(Config&& config)
    : config(std::move(config))

    {
        std::vector<realm::ObjectSchema> schema;
        for (auto& s : this->config.schema)
        {
            schema.push_back(static_cast<realm::ObjectSchema>(s));
        }
//        std::transform(this->config.schema.begin(),
//                       this->config.schema.end(),
//                       schema.begin(), [](auto schema) {
//            auto s = static_cast<realm::ObjectSchema>(schema);
//            return s;
//        });
        m_realm = CoreRealm::get_shared_realm({
            .path = this->config.path,
            .schema = realm::Schema(schema),
            .schema_version = 0
        });
    }

    void write(std::function<void()>&& block) const
    {
        m_realm->begin_transaction();
        block();
        m_realm->commit_transaction();
    }

    template <typename O>
    void add(O &object)
    {
        CppContext ctx;
        auto schema = O::schema();
        AnyDict dict;
        std::apply([&dict, &object](auto&&... props) {
            (dict.insert({props.name, (object.*props.ptr).as_core_type()}), ...);
        }, schema.props);

        realm::Object managed = realm::Object::create(ctx, this->m_realm,
                                                      *m_realm->schema().find(schema.name),
                                                      static_cast<util::Any>(dict));
        std::apply([&object, &managed](auto&&... props) {
            (props.assign(object, std::move(managed.obj())), ...);
        }, schema.props);
    }

    Config config;
private:
    std::shared_ptr<CoreRealm> m_realm;
};

} // namespace sdk

} // namespace realm

#endif /* Header_h */
