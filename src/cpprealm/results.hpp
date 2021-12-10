#ifndef realm_results_hpp
#define realm_results_hpp

#include <cpprealm/type_info.hpp>
#include <realm/object-store/results.hpp>

namespace realm {

template <typename T>
struct results {
    class iterator {
    public:
        using difference_type = size_t;
        using value_type = T;
        using pointer = std::unique_ptr<value_type>;
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
            return T::schema::create_unique(std::move(obj), m_parent->m_parent.get_realm());
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
        iterator(size_t idx, results<T>* parent)
        : m_idx(idx)
        , m_parent(parent)
        {
        }

        size_t m_idx;
        results<T>* m_parent;
        T value;

        template <typename>
        friend class results;
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
    template <type_info::ObjectPersistable...>
    friend struct db;
    results(realm::Results&& parent)
    : m_parent(std::move(parent))
    {
    }
    realm::Results m_parent;
};

}
#endif /* realm_results_hpp */