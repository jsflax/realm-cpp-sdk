#ifndef Header_h
#define Header_h

#include <utility>
#include <realm/object-store/impl/object_accessor_impl.hpp>
#include <realm/object-store/shared_realm.hpp>
#include <realm/object-store/object.hpp>
#include <filesystem>
#include <iostream>
#include <concepts>

namespace realm {
struct Obj;
struct Realm;

using CoreRealm = Realm;

namespace sdk {

// MARK: Persistable
template <typename T>
struct persisted_type {};

template <typename T>
concept IntPersistable = requires(T a) {
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
concept StringPersistable = requires(T a) {
    { static_cast<realm::String>(a) };
};
template <StringPersistable T>
struct persisted_type<T> { using type = StringData; };

// MARK: Object
struct Object {
    std::shared_ptr<Realm> m_realm = nullptr;
//private:
    std::shared_ptr<Obj> m_obj = nullptr;
};

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
concept ObjectPersistable = std::is_base_of_v<Object, T> && requires(T a) {
    { T::schema() };
};
template <ObjectPersistable T>
struct persisted_type<T> { using type = realm::ObjKey; };

template <typename T>
concept PrimitivePersistable = IntPersistable<T> || BoolPersistable<T> || StringPersistable<T>;
template <typename T>
concept NonOptionalPersistable = IntPersistable<T> || BoolPersistable<T> || StringPersistable<T> || ObjectPersistable<T>;
template <typename T>
concept OptionalObjectPersistable = Optional<T> && ObjectPersistable<typename T::value_type>;
template <typename T>
concept OptionalPersistable = Optional<T> && NonOptionalPersistable<typename T::value_type>;
template <OptionalPersistable T>
struct persisted_type<T> { using type = util::Optional<persisted_type<typename T::value_type>>; };
template <ObjectPersistable T>
struct persisted_type<std::optional<T>> { using type = realm::ObjKey; };

template <typename T>
concept Persistable = NonOptionalPersistable<T> || OptionalPersistable<T>;

template <PrimitivePersistable T>
constexpr typename persisted_type<T>::type convert_if_required(const T& a)
{
    return a;
}
template <OptionalObjectPersistable T>
constexpr typename persisted_type<T>::type convert_if_required(const T& a)
{
    if (a) { return a->m_obj ? a->m_obj->get_key() : ObjKey{}; }
    else { return ObjKey{}; }
}
template <typename T>
static constexpr PropertyType property_type();

template<> constexpr PropertyType property_type<int>() { return PropertyType::Int; }
template<> constexpr PropertyType property_type<std::string>() { return PropertyType::String; }
template<ObjectPersistable T> static constexpr PropertyType property_type() { return PropertyType::Object | PropertyType::Nullable; }
template<OptionalPersistable T> static constexpr PropertyType property_type() {
    return property_type<typename T::value_type>() | PropertyType::Nullable;
}

// MARK: Persisted
template <Persistable T>
struct Persisted {
    using type = typename persisted_type<T>::type;

    Persisted() {
//        unmanaged = T();
    }
    Persisted(const T& value) {
        unmanaged = value;
    }
    Persisted(T&& value) {
        unmanaged = std::move(value);
    }
    template <typename S>
    requires (StringPersistable<T>) && std::is_same_v<S, const char*>
    Persisted(S value) {
        unmanaged = value;
    }
    template <typename S>
    requires (ObjectPersistable<T>)
    Persisted(std::optional<S> value) {
        unmanaged = value;
    }
    ~Persisted()
    {
        if constexpr (realm::sdk::property_type<T>() == PropertyType::String) {
            using std::string;
            if (!m_obj)
                unmanaged.~string();
        }
        if (m_obj) {
            m_obj.reset();
        }
    }
    Persisted(const Persisted& o) {
        *this = o;
    }
    Persisted(Persisted&& o) {
        *this = std::move(o);
    }
    template <typename S>
    requires (StringPersistable<T>) && std::is_same_v<S, const char*>
    Persisted& operator=(S o) {
        if (m_obj) {
            m_obj->set<type>(managed, o);
        } else {
            unmanaged = o;
        }
        return *this;
    }
    Persisted& operator=(const T& o) {
        if (m_obj) {
            // if parent is managed...
            if constexpr (OptionalObjectPersistable<T>) {
                // if object...
                if (auto link = o) {
                    // if non null object is being assigned...
                    if (link->m_obj) {
                        // if object is managed, we will to set the link
                        // to the new target's key
                        m_obj->set<type>(managed, link->m_obj->get_key());
                    } else {
                        // else new unmanaged object is being assigned.
                        // we must assign the values to this object's fields
                        // TODO:
                    }
                } else {
                    // else null link is being assigned to this field
                    // e.g., `person.dog = std::nullopt;`
                    // set the parent column to null and unset the co
                    m_obj->set_null(managed);
                }
            } else {
                m_obj->set<type>(managed, o);
            }
        } else {
            unmanaged = o;
        }
        return *this;
    }
    Persisted& operator=(const Persisted& o) {
        if (o.m_obj) {
            m_obj = o.m_obj;
            managed = o.managed;
        } else {
            unmanaged = o.unmanaged;
        }
        return *this;
    }
    Persisted& operator=(Persisted&& o) {
        if (o.m_obj) {
            m_obj = std::move(o.m_obj);
            managed = std::move(o.managed);
        } else {
            unmanaged = std::move(o.unmanaged);
        }
        return *this;
    }
    T operator ->()
    {
        if (m_obj) {
            if constexpr (OptionalObjectPersistable<T>) {
                return T::value_type::schema().create(m_obj->get_linked_object(managed), nullptr);
            } else {
                return m_obj->get<type>(managed);
            }
        }
        else { return unmanaged; }
    }
    T operator *()
    {
        if (m_obj) {
            if constexpr (OptionalPersistable<T>) {
                if constexpr (ObjectPersistable<typename T::value_type>) {
                    return T::value_type::schema().create(m_obj->get_linked_object(managed), nullptr);
                } else {
                    auto value = m_obj->get<type>(managed);
                    // convert optionals
                    if (value) {
                        return T(*value);
                    } else {
                        return T();
                    }
                }
            } else {
                return m_obj->get<type>(managed);
            }
        } else {
            return unmanaged;
        }
    }

    type as_core_type() const
    {
        if constexpr (ObjectPersistable<T>) {
            if (m_obj && (*m_obj) != Obj{}) { return m_obj->get<type>(managed); }
            else {
                assert(false);
                return ObjKey{}; /* should never happen */
            }
        } else {
            if (m_obj && (*m_obj) != Obj{}) {
                return m_obj->get<type>(managed);
            } else {
                return convert_if_required<T>(unmanaged);
            }
        }
    }
    union {
        T unmanaged;
        realm::ColKey managed;
    };
    std::shared_ptr<Obj> m_obj;
};

struct Property {
    const char* name;
};

// MARK: Property
template<typename Class, typename Result>
struct property {
    constexpr property(const char* name,
                       Persisted<Result> Class::*ptr,
                       bool is_primary_key = false)
    : ptr(ptr)
    , name(name)
    , type(property_type<Result>())
    , is_primary_key(is_primary_key)
    {
    }

    explicit operator property() {
        return property();
    }

    explicit operator realm::Property() const {
        if constexpr (OptionalPersistable<Result>) {
            if constexpr (ObjectPersistable<typename Result::value_type>) {
                return realm::Property(name, type, Result::value_type::schema().name);
            } else {
                return realm::Property(name, type);
            }
        } else {
            return realm::Property(name, type, is_primary_key);
        }
    }

    void assign(Class& object, ColKey col_key, SharedRealm realm) {
        (object.*ptr).m_obj = object.m_obj;
        (object.*ptr).managed = col_key;
    }

    const char * name;
    Persisted<Result> Class::*ptr;
    PropertyType type;
    bool is_primary_key;
};

struct ObjectSchema;

template<typename Class, typename ...Result>
struct Model {
    constexpr Model(const char* name, property<Class, Result>&& ...properties)
    : name(name)
    , n(sizeof...(Result))
    , props{std::move(properties)...}
    {
    }

    const std::size_t n;
    std::tuple<property<Class, Result>...> props;
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

    void initialize(Class& cls, Obj&& obj, SharedRealm realm)
    {
        cls.m_obj = std::make_shared<Obj>(std::move(obj));
        cls.m_realm = realm;
        std::apply([&cls, &realm](auto&&... props) {
            (props.assign(cls, cls.m_obj->get_table()->get_column_key(props.name), realm), ...);
        }, props);
    }

    Class create(Obj&& obj, SharedRealm realm)
    {
        Class cls;
        initialize(cls, std::move(obj), realm);
        return cls;
    }

    template <OptionalObjectPersistable T>
    static void push_back_field(std::vector<FieldValue>& values, TableRef& table, ColKey key, Persisted<T>& field,
                                property<Class, T> property, Class& cls)
    {
        if (*field) {
            if (field.m_obj) {
                values.push_back({key, field.as_core_type()});
            } else {
                auto target_table = table->get_link_target(key);
                auto model = T::value_type::schema();
                auto target_cls = *field;
                auto obj = target_table->create_object(ObjKey{}, model.to_persisted_values(*target_cls, target_table));
                property.assign(cls, key, nullptr);
                values.push_back({key, obj.get_key()});
            }
            values.push_back({key, field.as_core_type()});
        }
    }

    template <PrimitivePersistable T>
    static void push_back_field(std::vector<FieldValue>& values, TableRef&, ColKey key, Persisted<T>& field, property<Class, T>, Class&)
    {
        values.push_back({key, field.as_core_type()});
    }

    std::vector<FieldValue> to_persisted_values(Class& cls, TableRef& table) {
        std::vector<FieldValue> values;
        std::apply([&cls, &values, &table](auto&&... props) {
            (push_back_field(values, table, table->get_column_key(props.name), cls.*props.ptr, props, cls), ...);
        }, props);
        return values;
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
            return !(*this == other);
        }

        bool operator==(const iterator& other) const
        {
            return (m_parent == other.m_parent) && (m_idx == other.m_idx);
        }

        reference operator*() noexcept
        {
            auto obj = m_parent->m_parent.template get<Obj>(m_idx);
            value = std::move(T::schema().create(std::move(obj), m_parent->m_parent.get_realm()));
            return value;
        }

        pointer operator->() const noexcept
        {
            auto obj = m_parent->m_parent.template get<Obj>(m_idx);
            this->value = T::schema().create(std::move(obj), m_parent->m_parent.get_realm());
            return &value;
        }

        iterator& operator++()
        {
            m_idx++;
            return *this;
        }

        iterator operator++(int i)
        {
            m_idx += i;
            return *this;
        }
    private:
        iterator(size_t idx, Results<T>* parent)
        : m_idx(idx)
        , m_parent(parent)
        {
        }

        size_t m_idx;
        Results<T>* m_parent;
        T value;

        template <typename>
        friend class Results;
    };

    iterator begin()
    {
        return iterator(0, this);
    }

    iterator end()
    {
        return iterator(m_parent.size(), this);
    }

    size_t size()
    {
        return m_parent.size();
    }
private:
    template <ObjectPersistable...>
    friend struct Realm;
    Results(realm::Results&& parent)
    : m_parent(std::move(parent))
    {
    }
    realm::Results m_parent;
};

// MARK: Realm
template <ObjectPersistable ...Ts>
struct Realm {
    struct Config {
        std::string path = std::filesystem::current_path().append("default.realm");
        std::vector<ObjectSchema> schema;
    };

    Realm()
    {
        std::vector<realm::ObjectSchema> schema;

        (schema.push_back(static_cast<realm::ObjectSchema>(Ts::schema())), ...);

        m_realm = CoreRealm::get_shared_realm({
            .path = this->config.path,
            .schema = realm::Schema(schema),
            .schema_version = 0
        });
    }

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

    template <ObjectPersistable T>
    void add(T& object)
    {
        CppContext ctx;
        auto schema = T::schema();
        auto actual_schema = *m_realm->schema().find(schema.name);
        auto& group = m_realm->read_group();
        auto table = group.get_table(actual_schema.table_key);
        auto values = schema.to_persisted_values(object, table);
        auto managed = table->create_object(ObjKey{}, values);
        schema.initialize(object, std::move(managed), m_realm);
    }


    template <ObjectPersistable T>
    void remove(T& object)
    {
        CppContext ctx;
        auto schema = T::schema();
        auto actual_schema = *m_realm->schema().find(schema.name);
        std::vector<FieldValue> dict;
        auto& group = m_realm->read_group();
        auto table = group.get_table(actual_schema.table_key);
        table->remove_object(object.m_obj->get_key());
    }

    template <ObjectPersistable T>
    Results<T> objects()
    {
        return Results<T>(realm::Results(m_realm, m_realm->read_group().get_table(ObjectStore::table_name_for_object_type(T::schema().name))));
    }

    Config config;
private:
    std::shared_ptr<CoreRealm> m_realm;
};

} // namespace sdk

} // namespace realm

#endif /* Header_h */
