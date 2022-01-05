#include <stdexcept>
#include <iostream>
#include <CollisionSystem.hpp>

CollisionSystem::CollisionSystem(Scene& scene):
    _scene{scene} {
}

void CollisionSystem::update() {
    SceneView<Body, Collider> colliderScene{_scene.view<Body, Collider>()};
    SceneView<Body, Collider, CircleBody> circleScene{_scene.view<Body, Collider, CircleBody>()};
    std::vector<EntityId> colliderIds;
    std::vector<EntityId> circleIds;
    std::copy(colliderScene.begin(), colliderScene.end(), std::back_inserter(colliderIds));
    std::copy(circleScene.begin(), circleScene.end(), std::back_inserter(circleIds));
    std::sort(colliderIds.begin(), colliderIds.end());
    std::sort(circleIds.begin(), circleIds.end());
    std::vector<EntityId> notCircleIds;
    std::set_difference(colliderIds.begin(), colliderIds.end(),
            circleIds.begin(), circleIds.end(), std::inserter(notCircleIds, notCircleIds.begin()));

    for (std::size_t i{0}; i < circleIds.size(); ++i) {
        Body& bodyA{circleScene.getComponent<Body>(circleIds[i])};
        const Collider& colliderA{colliderScene.getComponent<Collider>(circleIds[i])};
        const CircleBody& circleA{circleScene.getComponent<CircleBody>(circleIds[i])};

        // Circle - circle collisions
        for (std::size_t j{i + 1}; j < circleIds.size(); ++j) {
            Body& bodyB{circleScene.getComponent<Body>(circleIds[j])};
            const CircleBody& circleB{circleScene.getComponent<CircleBody>(circleIds[j])};
            collideCircles(circleA, circleB, bodyA, bodyB);
        }

        // Circle - convex collision
        for (std::size_t j{0}; j < notCircleIds.size(); ++j) {
            Body& bodyB{circleScene.getComponent<Body>(notCircleIds[j])};
            const Collider& colliderB{colliderScene.getComponent<Collider>(notCircleIds[j])};
            collideBodies(colliderA, colliderB, bodyA, bodyB);
        }
    }

    // Convex - convex collisions
    for (std::size_t i{0}; i < notCircleIds.size(); ++i) {
        Body& bodyA{colliderScene.getComponent<Body>(notCircleIds[i])};
        const Collider& colliderA{colliderScene.getComponent<Collider>(notCircleIds[i])};
        for (std::size_t j{i + 1}; j < notCircleIds.size(); ++j) {
            Body& bodyB{colliderScene.getComponent<Body>(notCircleIds[j])};
            const Collider& colliderB{colliderScene.getComponent<Collider>(notCircleIds[j])};
            collideBodies(colliderA, colliderB, bodyA, bodyB);
        }
    }
}

void CollisionSystem::collideBodies(const Collider& colliderA, const Collider& colliderB, Body& bodyA, Body& bodyB) {
    // Determine if the bodies collide with GJK
    std::optional<MinkowskyPolygon> collision{GJK(colliderA, colliderB)};
    if (collision.has_value()) {
        // Determine the collision info with EPA
        ContactInfo contactInfo{EPA(colliderA, colliderB, collision.value())};
        // Update the bodies' speed, position and rotation according to the collision
        collisionResponse(bodyA, bodyB, contactInfo);
    }
}

void CollisionSystem::collideCircles(const CircleBody& circleA, const CircleBody& circleB, Body& bodyA, Body& bodyB) {
    const Vector2d diff_x = bodyB.position - bodyA.position;
    const double dist{norm(diff_x)};
    const double overlap{circleA.radius + circleB.radius - dist};

    // Collision
    if (overlap > 0) {
        // Distance squared
        const Vector2d v_a{bodyA.velocity};
        const Vector2d v_b{bodyB.velocity};
        const double m_a{bodyA.mass};
        const double m_b{bodyB.mass};
        const Vector2d normal{diff_x / dist};
        const double restitution{bodyA.restitution * bodyB.restitution};
        // Based on stackoverflow.com/questions/35211114/2d-elastic-ball-collision-physics
        const Vector2d addedVel = dot(v_a - v_b, normal) * normal / (m_a + m_b);
        bodyA.velocity -= addedVel * (m_b + restitution * m_b);
        bodyB.velocity += addedVel * (m_a + restitution * m_a);

        // Move the bodies so that they just touch and don't overlap.
        // The displacement is proportional to the mass of the other body.
        bodyA.position -= m_b * overlap * diff_x / (dist * (m_a + m_b));
        bodyB.position += m_a * overlap * diff_x / (dist * (m_a + m_b));
    }
}

void CollisionSystem::collisionResponse(Body& bodyA, Body& bodyB, const ContactInfo& contactInfo) {
    // Vector going from the center of mass to the contact point
    const Vector2d R_A{contactInfo.C_A - bodyA.position};
    const Vector2d R_B{contactInfo.C_B - bodyB.position};
    const Vector2d v_A{bodyA.velocity};
    const Vector2d v_B{bodyB.velocity};
    const double m_A{bodyA.mass};
    const double m_B{bodyB.mass};
    const double I_A{bodyA.momentOfInertia};
    const double I_B{bodyB.momentOfInertia};
    const double w_A{bodyA.angularVelocity};
    const double w_B{bodyB.angularVelocity};
    // n is the normalized vector normal to the surface of contact
    const Vector2d n{contactInfo.normal / norm(contactInfo.normal)};
    const double R_A_n{cross(R_A, n)};
    const double R_B_n{cross(R_B, n)};
    const double restitution{bodyA.restitution * bodyB.restitution};


    // norm_J is the norm of the resulting impulse vector
    const double norm_J_elastic{2 *
        (dot(v_A - v_B, n) + cross(w_A * R_A - w_B * R_B, n)) /
        (1 / m_A + 1 / m_B + R_A_n * R_A_n / I_A + R_B_n * R_B_n / I_B)
    };
    const double norm_J_inelastic{m_A * m_B * dot(v_A - v_B, n) / (m_A + m_B)};
    const Vector2d J{n * (restitution * norm_J_elastic + (1 - restitution) * norm_J_inelastic)};

    bodyA.velocity -= J / m_A;
    bodyB.velocity += J / m_B;
    bodyA.angularVelocity -= cross(R_A, J) / I_A;
    bodyB.angularVelocity += cross(R_B, J) / I_B;

    // Shift the bodies out of collision
    bodyA.position -= contactInfo.normal * bodyB.mass / (bodyA.mass + bodyB.mass);
    bodyB.position += contactInfo.normal * bodyA.mass / (bodyA.mass + bodyB.mass);
}

void CollisionSystem::MinkowskyPolygon::pushBack(const Vector2d& A, const Vector2d& B) {
    insert(size(), A, B);
}

void CollisionSystem::MinkowskyPolygon::insert(std::size_t index, const Vector2d& A, const Vector2d& B) {
    _pointsA.insert(_pointsA.begin() + index, A);
    _pointsB.insert(_pointsB.begin() + index, B);
}

void CollisionSystem::MinkowskyPolygon::erase(std::size_t index) {
    _pointsA.erase(_pointsA.begin() + index);
    _pointsB.erase(_pointsB.begin() + index);
}

Vector2d CollisionSystem::MinkowskyPolygon::getDifference(std::size_t index) const {
    return getPointA(index) - getPointB(index);
}

Vector2d CollisionSystem::MinkowskyPolygon::getPointA(std::size_t index) const {
    return _pointsA.at(index);
}

Vector2d CollisionSystem::MinkowskyPolygon::getPointB(std::size_t index) const {
    return _pointsB.at(index);
}

std::size_t CollisionSystem::MinkowskyPolygon::size() const {
    return _pointsA.size();
}

std::optional<CollisionSystem::MinkowskyPolygon> CollisionSystem::GJK(
            const Collider& colliderA, const Collider& colliderB) {
    // GJK algorithm, see https://blog.winter.dev/2020/gjk-algorithm/
	// Or even better: https://youtu.be/ajv46BSqcK4
    Vector2d direction{1, 0};
    Vector2d supportA{colliderA.supportFunction(direction)};
    Vector2d supportB{colliderB.supportFunction(-direction)};
    Vector2d supportPoint{supportA - supportB};
    MinkowskyPolygon simplex;
    simplex.pushBack(supportA, supportB);

    direction = -supportPoint;
    std::size_t maxIter{50};
    for (std::size_t i{0}; i < maxIter; ++i) {
        supportA = colliderA.supportFunction(direction);
        supportB = colliderB.supportFunction(-direction);
        supportPoint = supportA - supportB;
        if (dot(supportPoint, direction) < 0) {
            // No collision
            return std::optional<MinkowskyPolygon>();
        }
        simplex.pushBack(supportA, supportB);
        if(nearestSimplex(simplex, direction)) {
            return simplex;
        }
    }
    return std::optional<MinkowskyPolygon>();
}

CollisionSystem::ContactInfo CollisionSystem::EPA(
        const Collider& colliderA, const Collider& colliderB,
        CollisionSystem::MinkowskyPolygon polygon) {
    if (polygon.size() != 3) {
        throw std::runtime_error("Invalid simplex size for EPA");
    }

    double eps{0.0001};
    double collisionGap{0.001};
    std::size_t maxIter{100};

    double minDistance;
    Vector2d minNormal;
    std::size_t minIndex;

    for (std::size_t t{0}; t < maxIter; ++t) {
        // Find the polygon edge closest to the origin, and the associated
        // normal vector pointing away from the origin
        minDistance = std::numeric_limits<double>::max();
        for (std::size_t i{0}; i < polygon.size(); ++i) {
            std::size_t j{(i + 1) % polygon.size()};
            Vector2d A{polygon.getDifference(i)}, B{polygon.getDifference(j)};
            Vector2d normal{perpendicular(B - A, A)};
            normal /= norm(normal);
            double distance{dot(normal, A)};
            if (distance < minDistance) {
                minDistance = distance;
                minNormal = normal;
                minIndex = i;
            }
        }

        // Find the support point in the direction of the normal, and check
        // if this point is further
        Vector2d supportA{colliderA.supportFunction(minNormal)};
        Vector2d supportB{colliderB.supportFunction(-minNormal)};
        double supportDistance{dot(supportA - supportB, minNormal)};

        // If the point is no further, we found the closest edge.
        // We also stop here if we are at the end of the loop
        if (std::abs(supportDistance - minDistance) <= eps or t == maxIter - 1) {
            // Find the four points in A and B which correspond to the current
            // edge of the the polygon
            std::size_t j{(minIndex + 1) % polygon.size()};
            Vector2d A_i{polygon.getPointA(minIndex)}, A_j{polygon.getPointA(j)};
            Vector2d B_i{polygon.getPointB(minIndex)}, B_j{polygon.getPointB(j)};
            Vector2d S_i{A_i - B_i}, S_j{A_j - B_j};
            // Alpha is the barycentric coordinate between S_i and S_j
            // of the vector projection of the origin onto the line S_i S_J.
            // So if alpha = 0, then the origin maps to S_i, and if alpha = 1,
            // it maps to S_j
            double alpha{dot(S_j - S_i, -S_i) / norm2(S_j - S_i)};

            ContactInfo info;
            info.normal = minNormal * (minDistance + collisionGap);
            info.C_A = A_i * (1 - alpha) + A_j * alpha;
            info.C_B = B_i * (1 - alpha) + B_j * alpha;
            return info;
        }
        // Otherwise, add the point to the polygon
        polygon.insert((minIndex + 1) % polygon.size(), supportA, supportB);
    }
    return ContactInfo();
}

bool CollisionSystem::nearestSimplex(CollisionSystem::MinkowskyPolygon& simplex, Vector2d& direction) {
    if (simplex.size() == 2) {
        return simplexLine(simplex, direction);
    } else if (simplex.size() == 3) {
        return simplexTriangle(simplex, direction);
    } else {
        throw std::runtime_error("Invalid simplex size");
    }
    return false;
}

bool CollisionSystem::simplexLine(CollisionSystem::MinkowskyPolygon& simplex, Vector2d& direction) {
    Vector2d A{simplex.getDifference(1)}, B{simplex.getDifference(0)};
    // Vector going from the new point to the old point in the simplex
    Vector2d AB{B - A};
    // New direction is perpendicular to this vector (towards origin)
    direction = perpendicular(AB, -A);
    return false;
}

bool CollisionSystem::simplexTriangle(CollisionSystem::MinkowskyPolygon& simplex, Vector2d& direction) {
    Vector2d A{simplex.getDifference(2)}, B{simplex.getDifference(1)}, C{simplex.getDifference(0)};
	Vector2d AC{C - A}, AB{B - A};
    // Check if we are outside the AC face. We go away from cb to stay outside
    // the simplex.
    Vector2d AC_perp{perpendicular(AC, -AB)};
    // If the origin is on this side
    if (dot(AC_perp, -A) > 0) {
		// Remove B from the simplex
        simplex.erase(1);
        direction = AC_perp;
        return false;
    }
    Vector2d AB_perp{perpendicular(AB, -AC)};
    if (dot(AB_perp, -A) > 0) {
		// Remove C from the simplex
        simplex.erase(0);
        direction = AB_perp;
        return false;
    }
    // Must be inside the simplex
    return true;
}