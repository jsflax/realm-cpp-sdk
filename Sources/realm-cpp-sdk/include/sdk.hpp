#ifndef Header_h
#define Header_h

#include <any>
#include <utility>
#include <filesystem>
#include <iostream>
#include <concepts>
#include <experimental/coroutine>

#include <realm/object-store/impl/object_accessor_impl.hpp>
#include <realm/object-store/shared_realm.hpp>
#include <realm/object-store/object.hpp>
#include <realm/object-store/object_schema.hpp>
#include <realm/object-store/schema.hpp>
#include <realm/object-store/sync/app.hpp>
#include <realm/object-store/sync/sync_user.hpp>
#include <realm/object-store/sync/impl/sync_client.hpp>
#include <realm/object-store/sync/async_open_task.hpp>
#include <realm/object-store/thread_safe_reference.hpp>

#ifdef QT_CORE_LIB
#include <QStandardPaths>
#endif

namespace realm {
class Obj;
class Realm;

using CoreRealm = Realm;

namespace sdk {

// MARK: Persistable
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

// MARK: Object
struct Object {
    template <typename T>
    friend NotificationToken observe(const T& cls);
    std::shared_ptr<Realm> m_realm = nullptr;
    std::optional<Obj> m_obj;
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
    { std::is_same_v<typename T::schema::Class, T> };
};
template <ObjectPersistable T>
struct persisted_type<T> { using type = realm::ObjKey; };

template <typename T>
concept PrimitivePersistable = IntPersistable<T> || BoolPersistable<T> || StringPersistable<T> || DoublePersistable<T>;
template <typename T>
concept NonOptionalPersistable = PrimitivePersistable<T> || ObjectPersistable<T>;
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
template<DoublePersistable T> static constexpr PropertyType property_type() {
    return PropertyType::Double;
}

// MARK: Persisted
template <Persistable T>
struct Persisted {
    using Result = T;
    using type = typename persisted_type<T>::type;

    Persisted() {
        new (&unmanaged) T();
    }
    Persisted(const T& value) {
        unmanaged = value;
    }
    Persisted(T&& value) {
        unmanaged = std::move(value);
    }
    Persisted(const char* value) requires StringPersistable<T> {
        new (&unmanaged) std::string(value);
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
        if (auto obj = m_obj) {
            obj->template set<type>(managed, o);
        } else {
            unmanaged = o;
        }
        return *this;
    }
    Persisted& operator=(const T& o) {
        if (auto obj = m_obj) {
            // if parent is managed...
            if constexpr (OptionalObjectPersistable<T>) {
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

    Persisted& operator=(const Persisted& o) {
        if (auto obj = o.m_obj) {
            m_obj = obj;
            new (&managed) ColKey(o.managed);
        } else {
            new (&unmanaged) T(o.unmanaged);
        }
        return *this;
    }
    Persisted& operator=(Persisted&& o) {
        if (o.m_obj) {
            m_obj = o.m_obj;
            new (&managed) ColKey(std::move(o.managed));
        } else {
            new (&unmanaged) T(std::move(o.unmanaged));
        }
        return *this;
    }
    T operator *() const
    {
        if (m_obj) {
            if constexpr (OptionalPersistable<T>) {
                if constexpr (ObjectPersistable<typename T::value_type>) {
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
                return m_obj->template get<type>(managed);
            }
        } else {
            return unmanaged;
        }
    }

    type as_core_type() const
    {
        if constexpr (ObjectPersistable<T>) {
            if (m_obj) {
                return m_obj->template get<type>(managed);
            } else {
                assert(false);
                return ObjKey{}; /* should never happen */
            }
        } else {
            if (m_obj) {
                return m_obj->template get<type>(managed);
            } else {
                return convert_if_required<T>(unmanaged);
            }
        }
    }

    void operator -=(const T& a) requires (IntPersistable<T> || DoublePersistable<T>) {
        if (m_obj) {
            m_obj->template set<type>(managed, *(*this) - a);
        } else {
            unmanaged -= a;
        }
    }
    void operator --() requires (IntPersistable<T> || DoublePersistable<T>) {
        *this -= 1;
    }
    T operator -() requires (IntPersistable<T> || DoublePersistable<T>) {
        return *this * -1;
    }
    void operator +=(const T& a) requires (AddAssignable<T>) {
        if (m_obj) {
            m_obj->template set<type>(managed, *(*this) + a);
        } else {
            unmanaged += a;
        }
    }
    T operator *(const T& a) requires (AddAssignable<T>) {
        return **this * a;
    }
    void operator ++() requires (AddAssignable<T>) {
        *this += 1;
    }
    bool operator <(const T& a) requires (Comparable<T>) {
        return **this < a;
    }
    bool operator >(const T& a) requires (Comparable<T>) {
        return **this > a;
    }
    bool operator <=(const T& a) requires (Comparable<T>) {
        return **this <= a;
    }
    bool operator >=(const T& a) requires (Comparable<T>) {
        return **this >= a;
    }
    union {
        T unmanaged;
        realm::ColKey managed;
    };
    void assign(const Obj& object, const ColKey& col_key) {
        m_obj = object;
        new (&managed) ColKey(col_key);
    }
    std::optional<Obj> m_obj;
};

static constexpr bool streq(char const *a, std::string_view b) {
    return std::string_view(a) == b;
}
// MARK: Property
template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    operator std::string() const { return value; }
    char value[N];
};


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

using IsPrimaryKey = bool;
template <StringLiteral Name, auto Ptr, IsPrimaryKey IsPrimaryKey = false>
struct property {
    using Result = typename ptr_type_extractor<Ptr>::member_type::Result;
    using Class = typename ptr_type_extractor<Ptr>::class_type;

    constexpr property()
    : type(property_type<Result>())
    {
    }

    explicit operator property() {
        return property();
    }

    explicit operator realm::Property() const {
        if constexpr (OptionalPersistable<Result>) {
            if constexpr (ObjectPersistable<typename Result::value_type>) {
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

    constexpr bool operator ==(std::string_view name) {
        return streq(this->name, name);
    }
    static constexpr const char* name = Name.value;
    static constexpr Persisted<Result> Class::*ptr = Ptr;
    PropertyType type;
    static constexpr bool is_primary_key = IsPrimaryKey;
};

// MARK: Schema
template <OptionalObjectPersistable T>
static void push_back_field(std::vector<FieldValue>& values, TableRef& table, ColKey key, Persisted<T>& field,
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

template <PrimitivePersistable T>
static void push_back_field(std::vector<FieldValue>& values, TableRef&, ColKey key, Persisted<T>& field, auto, auto&)
{
    values.push_back({key, field.as_core_type()});
}

template <typename T>
concept Propertyable = requires(T a) {
    { std::is_same_v<std::string, decltype(a.name)> };
    { std::is_same_v<typename T::Result T::Class::*, decltype(a.ptr)> };
    { std::is_same_v<PropertyType, decltype(a.type)> };
};

template <StringLiteral Name, Propertyable ...Properties>
struct schema {
    using Class = typename std::tuple_element_t<0, std::tuple<Properties...>>::Class;
    static constexpr const char* name = Name.value;
    static constexpr const std::tuple<Properties...> properties{};

    template <size_t N, Propertyable P>
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
            value = std::move(T::schema::create(std::move(obj), m_parent->m_parent.get_realm()));
            return value;
        }

        pointer operator->() const noexcept
        {
            auto obj = m_parent->m_parent.template get<Obj>(m_idx);
            this->value = T::schema::create(std::move(obj), m_parent->m_parent.get_realm());
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

// MARK: App

namespace {
#include <curl/curl.h>

class CurlGlobalGuard {
public:
    CurlGlobalGuard()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (++m_users == 1) {
            curl_global_init(CURL_GLOBAL_ALL);
        }
    }

    ~CurlGlobalGuard()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (--m_users == 0) {
            curl_global_cleanup();
        }
    }

    CurlGlobalGuard(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard(CurlGlobalGuard&&) = delete;
    CurlGlobalGuard& operator=(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard& operator=(CurlGlobalGuard&&) = delete;

private:
    static std::mutex m_mutex;
    static int m_users;
};

std::mutex CurlGlobalGuard::m_mutex = {};
int CurlGlobalGuard::m_users = 0;

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, std::string* response)
{
    REALM_ASSERT(response);
    size_t realsize = size * nmemb;
    response->append(ptr, realsize);
    return realsize;
}

size_t curl_header_cb(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* response_headers)
{
    REALM_ASSERT(response_headers);
    std::string combined(buffer, size * nitems);
    if (auto pos = combined.find(':'); pos != std::string::npos) {
        std::string key = combined.substr(0, pos);
        std::string value = combined.substr(pos + 1);
        while (value.size() > 0 && value[0] == ' ') {
            value = value.substr(1);
        }
        while (value.size() > 0 && (value[value.size() - 1] == '\r' || value[value.size() - 1] == '\n')) {
            value = value.substr(0, value.size() - 1);
        }
        response_headers->insert({key, value});
    }
    else {
        if (combined.size() > 5 && combined.substr(0, 5) != "HTTP/") { // ignore for now HTTP/1.1 ...
            std::cerr << "test transport skipping header: " << combined << std::endl;
        }
    }
    return nitems * size;
}
app::Response do_http_request(const app::Request& request)
{
    CurlGlobalGuard curl_global_guard;
    auto curl = curl_easy_init();
    if (!curl) {
        return app::Response{500, -1};
    }

    struct curl_slist* list = nullptr;
    auto curl_cleanup = util::ScopeExit([&]() noexcept {
        curl_easy_cleanup(curl);
        curl_slist_free_all(list);
    });

    std::string response;
    std::map<std::string, std::string> response_headers;

    /* First set the URL that is about to receive our POST. This URL can
     just as well be a https:// URL if that is what should receive the
     data. */
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());

    /* Now specify the POST data */
    if (request.method == app::HttpMethod::post) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::put) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::patch) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::del) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    else if (request.method == app::HttpMethod::patch) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_ms);

    for (auto header : request.headers) {
        auto header_str = util::format("%1: %2", header.first, header.second);
        list = curl_slist_append(list, header_str.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

    auto response_code = curl_easy_perform(curl);
    if (response_code != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed when sending request to '%s' with body '%s': %s\n",
                request.url.c_str(), request.body.c_str(), curl_easy_strerror(response_code));
    }
    int http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    return {
        http_code,
        0, // binding_response_code
        std::move(response_headers),
        std::move(response),
    };
}
class DefaultTransport : public app::GenericNetworkTransport {
public:
    void send_request_to_server(const app::Request request,
                                std::function<void(const app::Response)> completion_block) override
    {
        completion_block(do_http_request(request));
    }
};
} // anonymous namespace

struct task_promise_base {
    std::experimental::suspend_always initial_suspend() {
        return {};
    }
    auto final_suspend() noexcept {
        struct awaiter {
            bool await_ready() noexcept {
                return !next || next.done();
            }
            void await_suspend(std::experimental::coroutine_handle<>) noexcept {
                if (next) {
                    next();
                }
            }
            void await_resume() noexcept {
                __builtin_unreachable();
            }
            std::experimental::coroutine_handle<> next;
        };
        return awaiter{next};
    }

    void unhandled_exception() noexcept {
        err = std::current_exception();
    }

    std::exception_ptr err;
    std::experimental::coroutine_handle<> next;
};

// WARNING: this segfaults with gcc (but not clang). Best to avoid it for now. :(
template <typename T>
struct [[nodiscard]] task {
    struct promise_type : task_promise_base {
        auto get_return_object() {
            return task(this);
        }
        auto coro() {
            return std::experimental::coroutine_handle<promise_type>::from_promise(*this);
        }
        void return_value(T val) {
            result.emplace(std::move(val));
        }
        std::optional<T> result;
    };

    task(task&& source) : p(std::exchange(source.p, nullptr)) {}
    explicit task(promise_type* p) : p(p) {}
    ~task() {
        if (p)
            p->coro().destroy();
    }

    bool await_ready() noexcept {
        return p->coro().done();
    }
    std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> next) noexcept {
        assert(next != p->coro());
        p->next = next;
        return p->coro();
    }
    const T& await_resume() const& {
        uassertStatusOK(p->status);
        return *p->result;
    }
    T& await_resume() & {
//        uassertStatusOK(p->status);
        return *p->result;
    }
    T&& await_resume() && {
        uassertStatusOK(p->status);
        return std::move(*p->result);
    }

    promise_type* p;
};

template <>
struct [[nodiscard]] task<void> {
    struct promise_type : task_promise_base {
        auto get_return_object() {
            return task(this);
        }
        auto coro() {
            return std::experimental::coroutine_handle<promise_type>::from_promise(*this);
        }
        void return_void() {}
    };

    task(task&& source) : p(std::exchange(source.p, nullptr)) {}
    explicit task(promise_type* p) : p(p) {}
    ~task() {
        if (p)
            p->coro().destroy();
    }

    bool await_ready() noexcept {
        return p->coro().done();
    }
    std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> next) noexcept {
        assert(next != p->coro());
        p->next = next;
        return p->coro();
    }

    void await_resume() const {
        if (p->err) std::rethrow_exception(p->err);
    }

    promise_type* p;
};


#define FWD(x) static_cast<decltype(x)&&>(x)

inline void handle_result_args() {}

template <typename T>
inline T&& handle_result_args(T&& res) { return FWD(res); }

template <typename T>
inline T&& handle_result_args(T&& res, util::Optional<app::AppError> ec) {
    if (ec) throw std::system_error(ec->error_code);
    return FWD(res);
}
template <typename T>
inline T&& handle_result_args(T&& res, std::exception_ptr ec) {
    if (ec) throw ec;
    return FWD(res);
}
template <typename Res, typename F>
auto make_awaitable(F&& func) {
    struct Awaiter {
        Awaiter& operator=(Awaiter&&) = delete;
        bool await_ready() { return {}; }
        void await_suspend(std::experimental::coroutine_handle<> handle) {
            func([handle, this] (auto&&... args) mutable
                 {
                try {
                    result.emplace(handle_result_args(FWD(args)...));
                } catch (...) {
                    error.emplace(std::current_exception());
                }
                handle.resume();
            });
        }
        Res await_resume() {
            if (result)
                return std::move(*result);
            std::rethrow_exception(*error);
        }
        std::optional<Res> result;
        std::optional<std::exception_ptr> error;
        F& func;
    };
    return Awaiter{.func = func};
}

template <ObjectPersistable ...Ts>
struct Realm;

// MARK: User
struct User {
    std::string access_token() const
    {
        return m_user->access_token();
    }

    std::string refresh_token() const
    {
        return m_user->refresh_token();
    }
    template <ObjectPersistable ...Ts, typename T>
    task<Realm<Ts...>> realm(const T& partition_value) const requires (StringPersistable<T> || IntPersistable<T>);
    std::shared_ptr<SyncUser> m_user;
};

class App {
    static std::unique_ptr<realm::util::Logger> defaultSyncLogger(realm::util::Logger::Level level) {
        struct SyncLogger : public realm::util::RootLogger {
            void do_log(Level, const std::string& message) override {
                std::cout<<"sync: " + message<<std::endl;
            }
        };
        auto logger = std::make_unique<SyncLogger>();
        logger->set_level_threshold(level);
        return std::move(logger);
    }
public:
    App(const std::string& app_id)
    {
        SyncClientConfig config;
        bool should_encrypt = !getenv("REALM_DISABLE_METADATA_ENCRYPTION");
        config.logger_factory = defaultSyncLogger;
        #if REALM_DISABLE_METADATA_ENCRYPTION
        config.metadata_mode = SyncManager::MetadataMode::NoEncryption;
        #else
        config.metadata_mode = SyncManager::MetadataMode::Encryption;
        #endif
        #ifdef QT_CORE_LIB
        auto qt_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        if (!std::filesystem::exists(qt_path)) {
            std::filesystem::create_directory(qt_path);
        }
        config.base_file_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        #else
        config.base_file_path = std::filesystem::current_path();
        #endif
        config.user_agent_binding_info = "RealmCpp/0.0.1";
        config.user_agent_application_info = app_id;

        m_app = realm::app::App::get_shared_app(realm::app::App::Config{
            app_id,
            .platform="Realm Cpp",
            .platform_version="?",
            .sdk_version="0.0.1",
            .transport = std::make_shared<DefaultTransport>()
        }, config);
    }

    struct Credentials {
        static Credentials anonymous()
        {
            return Credentials(app::AppCredentials::anonymous());
        }
    private:
        Credentials(app::AppCredentials&& credentials)
        : m_credentials(credentials)
        {
        }
        friend class App;
        app::AppCredentials m_credentials;
    };

    task<User> login(const Credentials& credentials) {
        auto user = co_await make_awaitable<std::shared_ptr<SyncUser>>([&] (auto cb) {
            m_app->log_in_with_credentials(credentials.m_credentials, cb);
        });
        co_return User{user};
    }
private:
    std::shared_ptr<realm::app::App> m_app;
};

// MARK: Realm

template <ObjectPersistable ...Ts>
struct Realm {

    struct Config {
        std::string path = std::filesystem::current_path().append("default.realm");

    private:
        friend struct User;
        template <ObjectPersistable ...>
        friend struct Realm;
        std::shared_ptr<SyncConfig> sync_config;
    };

    Realm(const Config& config = {})
    : config(std::move(config))
    {
        std::vector<realm::ObjectSchema> schema;

        (schema.push_back(Ts::schema::to_core_schema()), ...);

        m_realm = CoreRealm::get_shared_realm({
            .path = this->config.path,
            .schema = realm::Schema(schema),
            .schema_version = 0,
            .sync_config = this->config.sync_config
        });
    }
    Realm(Config&& config)
    : config(std::move(config))
    {
        std::vector<realm::ObjectSchema> schema;
        (schema.push_back(Ts::schema::to_core_schema()), ...);

        m_realm = CoreRealm::get_shared_realm({
            .path = this->config.path,
            .schema = realm::Schema(schema),
            .schema_version = 0,
            .sync_config = this->config.sync_config
        });
    }

    static task<Realm> open(Config&& config) {
        std::vector<realm::ObjectSchema> schema;
        (schema.push_back(Ts::schema::to_core_schema()), ...);

        std::shared_ptr<AsyncOpenTask> async_open_task = CoreRealm::get_synchronized_realm({
            .path = config.path,
            .schema = realm::Schema(schema),
            .schema_version = 0,
            .sync_config = config.sync_config
        });
        auto tsr = co_await make_awaitable<ThreadSafeReference>([&async_open_task](auto cb) {
            async_open_task->start(cb);
        });
        auto ref = tsr.template resolve<std::shared_ptr<CoreRealm>>(nullptr);
        auto realm = Realm(std::move(config));
        ref.reset();
        co_return realm;
    }

    void write(std::function<void()>&& block) const
    {
        m_realm->begin_transaction();
        block();
        m_realm->commit_transaction();
    }

    template <ObjectPersistable T>
    void add(T& object) requires (std::is_same_v<T, Ts> || ...)
    {
        CppContext ctx;
        auto actual_schema = *m_realm->schema().find(T::schema::name);
        auto& group = m_realm->read_group();
        auto table = group.get_table(actual_schema.table_key);
        auto values = T::schema::to_persisted_values(object, table);
        Obj managed;
        if constexpr (T::schema::HasPrimaryKeyProperty) {
            auto pk = *(object.*T::schema::PrimaryKeyProperty::ptr);
            managed = table->create_object_with_primary_key(pk, std::move(values));
        } else {
            managed = table->create_object(ObjKey{}, values);
        }
        T::schema::initialize(object, std::move(managed), m_realm);
    }
    template <ObjectPersistable T>
    void add(T&& object) requires (std::is_same_v<T, Ts> || ...)
    {
        add(object);
    }

    template <ObjectPersistable T>
    void remove(T& object) requires (std::is_same_v<T, Ts> || ...)
    {
        CppContext ctx;
        auto actual_schema = *m_realm->schema().find(T::schema::name);
        std::vector<FieldValue> dict;
        auto& group = m_realm->read_group();
        auto table = group.get_table(actual_schema.table_key);
        table->remove_object(object.m_obj->get_key());
    }

    template <ObjectPersistable T>
    Results<T> objects() requires (std::is_same_v<T, Ts> || ...)
    {
        return Results<T>(realm::Results(m_realm,
                                         m_realm->read_group().get_table(ObjectStore::table_name_for_object_type(T::schema::name))));
    }

    template <ObjectPersistable T>
    T object(const typename T::schema::PrimaryKeyProperty::Result& primary_key) {
        auto table = m_realm->read_group().get_table(ObjectStore::table_name_for_object_type(T::schema::name));
        return T::schema::create(table->get_object_with_primary_key(primary_key),
                                 m_realm);
    }

    template <ObjectPersistable T>
    T* object_new(const typename T::schema::PrimaryKeyProperty::Result& primary_key) {
        auto table = m_realm->read_group().get_table(ObjectStore::table_name_for_object_type(T::schema::name));
        return T::schema::create_new(table->get_object_with_primary_key(primary_key),
                                 m_realm);
    }

    Config config;
private:
    std::shared_ptr<CoreRealm> m_realm;
};

// MARK: - Observation

template <ObjectPersistable T>
using ObjectNotificationCallback = std::function<void(const T&,
                                                      std::vector<std::string> property_names,
                                                      std::vector<std::any> old_values,
                                                      std::vector<std::any> new_values,
                                                      std::exception_ptr error)>;
struct NotificationToken {

private:
    template <ObjectPersistable T>
    friend NotificationToken observe(const T& obj, ObjectNotificationCallback<T> block);
    realm::Object m_object;
    SharedRealm m_realm;
    realm::NotificationToken m_token;
};

namespace {
template <ObjectPersistable T>
struct ObjectChangeCallbackWrapper {
    ObjectNotificationCallback<T> block;
    const T* object;

    std::optional<std::vector<std::string>> property_names = std::nullopt;
    std::optional<std::vector<std::any>> old_values = std::nullopt;
    bool deleted = false;

    void populateProperties(realm::CollectionChangeSet const& c) {
        if (property_names) {
            return;
        }
        if (!c.deletions.empty()) {
            deleted = true;
            return;
        }
        if (c.columns.empty()) {
            return;
        }

        // FIXME: It's possible for the column key of a persisted property
        // to equal the column key of a computed property.
        auto properties = std::vector<std::string>();
        TableRef table = object->m_realm->read_group().get_table(ObjectStore::table_name_for_object_type(T::schema::name));

        std::apply([&c, &table, &properties](auto&&... props) {
            (((c.columns.count(table->get_column_key(props.name).value)) ?
              properties.push_back(props.name) : void()), ...);
        }, T::schema::properties);
        if (!properties.empty()) {
            property_names = properties;
        }
    }

    std::optional<std::vector<std::any>> readValues(realm::CollectionChangeSet const& c) {
        if (c.empty()) {
            return std::nullopt;
        }
        populateProperties(c);
        if (!property_names) {
            return std::nullopt;
        }

        std::vector<std::any> values;
        for (auto& name : *property_names) {
            std::apply([&name, &values, this](auto&&... props) {
                ((name == props.name ? values.push_back(*(object->*props.ptr)) : void()), ...);
            }, T::schema::properties);
        }
        return values;
    }

    void before(realm::CollectionChangeSet const& c) {
        old_values = readValues(c);
    }

    void after(realm::CollectionChangeSet const& c) {
        auto new_values = readValues(c);
        if (deleted) {
            block(*object, {}, {}, {}, nullptr);
        } else if (new_values) {
            block(*object, *property_names, old_values ? *old_values : std::vector<std::any>{}, *new_values, nullptr);
        }
        property_names = std::nullopt;
        old_values = std::nullopt;
    }

    void error(std::exception_ptr err) {
        block(*object, {}, {}, {}, err);
    }
};
}
template <ObjectPersistable T>
NotificationToken observe(const T& obj, ObjectNotificationCallback<T> block)
{
    if (!obj.m_realm) {
        throw std::runtime_error("Only objects which are managed by a Realm support change notifications");
    }
    NotificationToken token;
    token.m_object = realm::Object(obj.m_realm, T::schema::to_core_schema(), *(obj.m_obj));
    token.m_realm = obj.m_realm;
    token.m_token = token.m_object.add_notification_callback(ObjectChangeCallbackWrapper<T>{block, &obj});
    return token;
}

template <ObjectPersistable ...Ts, typename T>
task<Realm<Ts...>> User::realm(const T& partition_value) const requires (StringPersistable<T> || IntPersistable<T>)
{
    SyncConfig sync_config(m_user, bson::Bson(partition_value));
    sync_config.error_handler = [](std::shared_ptr<SyncSession> session, SyncError error) {
        std::cout<<"sync error: "<<error.message<<std::endl;
    };
    typename sdk::Realm<Ts...>::Config config;
    config.sync_config = std::make_shared<SyncConfig>(sync_config);
    config.path = m_user->sync_manager()->path_for_realm(sync_config);
    auto realm = co_await sdk::Realm<Ts...>::open(std::move(config));
    co_return realm;
}

} // namespace sdk

} // namespace realm

#endif /* Header_h */
