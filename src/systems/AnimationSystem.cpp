#include <SFML/System/Time.hpp>
#include <systems/AnimationSystem.hpp>
#include <Scene.hpp>
#include <Animation.hpp>
#include <SoundSettings.hpp>
#include <GameEvent.hpp>
#include <components/Animations.hpp>
#include <components/components.hpp>

AnimationSystem::AnimationSystem(Scene& scene, const SoundSettings& soundSettings):
    _scene{scene},
    _soundSettings{soundSettings} {
}

void AnimationSystem::update(sf::Time dt) {
    for (auto& [id, animations] : _scene.view<Animations>()) {
        for (auto& [action, animationData] : animations) {
            animationData.animation.setVolume(_soundSettings.mainVolume * _soundSettings.effectsVolume / 100);
            if (not animationData.animation.isStopped()) {
                animationData.animation.update(dt);
            }
        }
    }
}

void AnimationSystem::handleTriggerEvent(const GameEvent& event, bool start) {
    Animations& animations{_scene.getComponent<Animations>(event.entity)};
    auto animationIt = animations.find(event.type);
    if (animationIt != animations.end()) {
        if (start) {
            animationIt->second.animation.start();
        } else {
            animationIt->second.animation.stop();
        }
    }
}
