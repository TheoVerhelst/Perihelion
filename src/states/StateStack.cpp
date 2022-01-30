#include <states/StateStack.hpp>


StateStack::StateStack(tgui::BackendGui& gui) :
    _gui{gui} {
}

void StateStack::popState() {
    _actionQueue.push([this] {
        _gui.remove(_stack.back().widget);
        _stack.pop_back();
    });
}

void StateStack::update(sf::Time dt) {
    while (not _actionQueue.empty()) {
        _actionQueue.front()();
        _actionQueue.pop();
    }
    for (auto it = _stack.rbegin(); it != _stack.rend(); ++it) {
        if (it->state->update(dt)) {
            break;
        }
    }
}

void StateStack::handleTriggerAction(const TriggerAction& action) {
    for (auto it = _stack.rbegin(); it != _stack.rend(); ++it) {
        if (it->state->handleTriggerAction(action)) {
            break;
        }
    }
}

void StateStack::handleContinuousAction(const Action& action, sf::Time dt) {
    for (auto it = _stack.rbegin(); it != _stack.rend(); ++it) {
        if (it->state->handleContinuousAction(action, dt)) {
            break;
        }
    }
}

bool StateStack::isEmpty() const {
    return _stack.empty();
}