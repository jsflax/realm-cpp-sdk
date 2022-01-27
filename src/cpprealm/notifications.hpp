//
//  notifications.hpp
//  
//
//  Created by Lee Maguire on 24/01/2022.
//

#ifndef notifications_hpp
#define notifications_hpp

#include <realm/object-store/list.hpp>
#include <realm/object-store/object.hpp>
#include <realm/object-store/object_store.hpp>
#include <realm/object-store/shared_realm.hpp>

#include <any>

namespace realm {

    struct Realm;
    struct NotificationToken;
    struct object;

/**
 A token which is returned from methods which subscribe to changes to a `realm::object`.
 */
    struct notification_token {
    public:
        List m_list;
        realm::Object m_object;
        realm::NotificationToken m_token;
    };


// MARK: PropertyChange
/**
 Information about a specific property which changed in an `realm::object` change notification.
 */
    struct PropertyChange {
        /**
         The name of the property which changed.
        */
        std::string name;

        /**
         Value of the property before the change occurred. This is not supplied if
         the change happened on the same thread as the notification and for `List`
         properties.

         For object properties this will give the object which was previously
         linked to, but that object will have its new values and not the values it
         had before the changes. This means that `previousValue` may be a deleted
         object, and you will need to check `isInvalidated` before accessing any
         of its properties.
        */
        std::optional<std::any> old_value;

        /**
         The value of the property after the change occurred. This is not supplied
         for `List` properties and will always be nil.
        */
        std::optional<std::any> new_value;
    };

    struct CollectionChange {

        std::vector<uint64_t> deletions;
        std::vector<uint64_t> insertions;
        std::vector<uint64_t> modifications;

        // This flag indicates whether the underlying object which is the source of this
        // collection was deleted. This applies to lists, dictionaries and sets.
        // This enables notifiers to report a change on empty collections that have been deleted.
        bool collection_root_was_deleted = false;

        bool empty() const noexcept {
            return deletions.empty() && insertions.empty() && modifications.empty() &&
            !collection_root_was_deleted;
        }
    };

} // namespace realm

#endif /* notifications_hpp */
