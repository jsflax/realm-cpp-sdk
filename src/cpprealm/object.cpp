#include <cpprealm/object.hpp>

namespace realm {

void object::write(std::function<void()> fn)
{
    m_realm->begin_transaction();
    fn();
    m_realm->commit_transaction();
}

}
