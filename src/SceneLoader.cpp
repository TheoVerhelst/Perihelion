#include <fstream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <SFML/Graphics.hpp>
#include <json.hpp>
#include <serializers.hpp>
#include <DebugInfo.hpp>
#include <SceneLoader.hpp>

using nlohmann::json;

void loadScene(Scene& scene, const std::string& setupFile,
        const ResourceManager<sf::Font>& fontManager,
        const ResourceManager<sf::Texture>& textureManager) {
    std::ifstream file{setupFile};
	json j;
	try	{
	    j = json::parse(file);
	} catch (json::parse_error& ex) {
	    std::cerr << "parse error in " << setupFile << " at byte " << ex.byte << std::endl;
	}
	for (json& components : j.at("entities")) {
        EntityId id{scene.createEntity()};

        bool hasBody{false};
        for (auto& [key, value] : components.items()) {
            if (key == "body") {
                Body& body{scene.assignComponent<Body>(id)};
                value.get_to(body);
                hasBody = true;
                scene.assignComponent<DebugInfo>(id, fontManager.get("debugFont"));
            }
        }

        for (auto& [key, value] : components.items()) {
            if (key == "circle") {
                CircleBody& circle{scene.assignComponent<CircleBody>(id)};
                value.get_to(circle);
                if (hasBody) {
                    setupCircle(scene, id);
                }
            } else if (key == "convex") {
                ConvexBody& convex{scene.assignComponent<ConvexBody>(id)};
                value.get_to(convex);
                if (hasBody) {
                    setupConvex(scene, id);
                }
            } else if (key == "sprite") {
                sf::Sprite& sprite{scene.assignComponent<sf::Sprite>(id)};
                std::string texture{value.at("texture").get<std::string>()};
                sf::IntRect textureRect{value.at("textureRect").get<sf::IntRect>()};
                sprite.setTexture(textureManager.getRef(texture));
                sprite.setTextureRect(textureRect);
                if (hasBody) {
                    sprite.setOrigin(static_cast<Vector2f>(scene.getComponent<Body>(id).centerOfMass));
                }
            }
        }
    }
}

void setupCircle(Scene& scene, EntityId id) {
    CircleBody& circle{scene.getComponent<CircleBody>(id)};
    Body& body{scene.getComponent<Body>(id)};

    Trace& trace{scene.assignComponent<Trace>(id)};
    // Fill the trace with points at the body position, to avoid
    // undefined traces because of the unitialized vector.
    trace.trace = sf::VertexArray(sf::Lines, trace.traceLength * 2);
    for (std::size_t i{0}; i < trace.traceLength * 2; ++i) {
        trace.trace[i] = sf::Vertex(static_cast<Vector2f>(body.position), sf::Color::White);
    }

    // Physical constants
    body.centerOfMass = {circle.radius, circle.radius};
    body.momentOfInertia =  body.mass * circle.radius * circle.radius / 2.;

    // Collider
    Collider& collider{scene.assignComponent<Collider>(id)};
    collider.supportFunction = [&circle, &body](const Vector2d& direction) {
        return body.localToWorld(direction * circle.radius / norm(direction));
    };
}

void setupConvex(Scene& scene, EntityId id) {
    Body& body{scene.getComponent<Body>(id)};
    ConvexBody& convex{scene.getComponent<ConvexBody>(id)};

    // Physical constants
    body.centerOfMass = computeCenterOfMass(convex.vertices);
    // Shift the vertices so they represent correct local coordinates around the
    // center of mass
    for (auto& vertex : convex.vertices) {
        vertex -= body.centerOfMass;
    }
    body.momentOfInertia =  computeMomentOfInertia(body.mass, {0., 0.}, convex.vertices);

    // Collider
    Collider& collider{scene.assignComponent<Collider>(id)};
    collider.supportFunction = std::bind(convexSupportFunction, std::ref(body), std::ref(convex), std::placeholders::_1);
}

Vector2d convexSupportFunction(const Body& body, const ConvexBody& convex, const Vector2d& direction) {
	double largestProd{-std::numeric_limits<double>::max()};
	Vector2d furthestPoint;
	for (Vector2d vertex : convex.vertices) {
		// Rotate the point around the center of mass to account for rotation
		Vector2d worldPoint{body.localToWorld(vertex)};

		double product{dot(direction, worldPoint)};
		if (product > largestProd) {
			furthestPoint = worldPoint;
			largestProd = product;
		}
	}
    return furthestPoint;
}

Vector2d computeCenterOfMass(const std::vector<Vector2d>& vertices) {
	if (vertices.size() < 3) {
		throw std::runtime_error("Invalid shape");
	}
	std::vector<double> triangleAreas;
	std::vector<Vector2d> triangleCenters;
	for (std::size_t i{0}; i < vertices.size(); ++i) {
		Vector2d B{vertices[i]};
		Vector2d C{vertices[(i + 1) % vertices.size()]};
		triangleAreas.push_back(std::abs(cross(B, C)) / 2.);
		triangleCenters.push_back((B + C) / 3.);
	}
	double totalArea{std::accumulate(triangleAreas.begin(), triangleAreas.end(), 0.)};
	Vector2d centerOfMass{std::inner_product(triangleCenters.begin(),
		triangleCenters.end(), triangleAreas.begin(), Vector2d(0, 0))};
	return centerOfMass / totalArea;
}

double computeMomentOfInertia(double mass, const Vector2d& centerOfMass, const std::vector<Vector2d>& vertices) {
	if (vertices.size() < 3) {
		throw std::runtime_error("Invalid shape");
	}
    std::vector<double> areas;
    for (std::size_t i{0}; i < vertices.size(); ++i) {
        Vector2d B{vertices[i]};
        Vector2d C{vertices[(i + 1) % vertices.size()]};
        areas.push_back(std::abs(cross(B, C)) / 2.);
    }
    double totalArea{std::accumulate(areas.begin(), areas.end(), 0.)};
    double momentOfInertia{0};
    for (std::size_t i{0}; i < vertices.size(); ++i) {
        Vector2d B{vertices[i]};
        Vector2d C{vertices[(i + 1) % vertices.size()]};
        double signedArea{cross(B, C) / 2.};
        double triangleArea{areas[i]};
        double triangleMass{mass * triangleArea / totalArea};
        momentOfInertia += triangleMass * (signedArea / triangleArea) * (norm2(B) + norm2(C) + dot(B, C));
    }
    return (momentOfInertia / 6.) - mass * norm2(centerOfMass);
}
