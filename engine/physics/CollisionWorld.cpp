#include "physics/CollisionWorld.h"

namespace engine::physics {

void CollisionWorld::clear()
{
    bodies_.clear();
}

void CollisionWorld::add(BodyId id, const Aabb &box, Flags flags)
{
    bodies_.append(Body{id, flags, box});
}

bool CollisionWorld::remove(BodyId id)
{
    const int before = bodies_.size();
    bodies_.removeIf([id](const Body &body) { return body.id == id; });
    return bodies_.size() != before;
}

const CollisionWorld::Body *CollisionWorld::find(BodyId id) const
{
    for (const Body &body : bodies_) {
        if (body.id == id)
            return &body;
    }
    return nullptr;
}

} // namespace engine::physics
