#include <TGUI/Widgets/Button.hpp>
#include <states/StateStack.hpp>
#include <states/MapState.hpp>
#include <Scene.hpp>
#include <components.hpp>

MapState::MapState(StateStack& stack, Scene& scene):
    AbstractState{stack},
    _scene{scene} {
}

tgui::Widget::Ptr MapState::buildGui() {
    for (EntityId id : _scene.view<MapElement>()) {
        MapElement& mapElement{_scene.getComponent<MapElement>(id)};
        tgui::Picture::Ptr icon{tgui::Picture::copy(mapElement.icon)};
        icon->setUserData(id);
        _mapIcons->add(icon);
    }
    return _mapIcons;
}

void MapState::update(sf::Time) {
    const Body& playerBody{_scene.getComponent<Body>(_scene.findUnique<Player>())};
    const Vector2f playerPos{playerBody.position};
    const Vector2f mapSize{_mapIcons->getSize()};

    for (tgui::Widget::Ptr icon : _mapIcons->getWidgets()) {
        EntityId id{icon->getUserData<EntityId>()};
        const Body& body{_scene.getComponent<Body>(id)};
        MapElement& mapElement{_scene.getComponent<MapElement>(id)};
        // Compute the position of the map element on the screen. Note that we
        // don't rotate the map icon.
        Vector2f screenPos{(body.position - playerPos) / _scale};
        screenPos += mapSize / 2.f;
        icon->setPosition(static_cast<tgui::Vector2f>(screenPos));
        // Except for ship icons
        if (mapElement.type == MapElementType::Ship) {
            icon->setRotation(radToDeg(body.rotation));
        }
    }
}

bool MapState::handleTriggerAction(const TriggerAction& actionPair) {
    auto& [action, start] = actionPair;
    if (action == Action::ToggleMap and start) {
        _stack.popState();
        return true;
    }
    return false;
}

bool MapState::handleContinuousAction(const Action& action, sf::Time dt) {
    bool consumeEvent{true};
    switch (action) {
        case Action::ZoomIn:
            _scale *= std::pow(_zoomSpeed, dt.asSeconds());
            break;
        case Action::ZoomOut:
            _scale /= std::pow(_zoomSpeed, dt.asSeconds());
            break;
        default:
            consumeEvent = false;
            break;
    }
    _scale = std::clamp(_scale, _minScale, _maxScale);
    return consumeEvent;
}
