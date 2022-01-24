#include <Application.hpp>
#include <components.hpp>
#include <SceneLoader.hpp>

Application::Application(const std::string& setupFile):
    _window{sf::VideoMode::getDesktopMode(), "Aphelion",  sf::Style::Fullscreen},
    _gui{_window},
    _physicsSystem{_scene},
    _renderSystem{_scene, _shaderManager},
    _gameplaySystem{_scene},
    _lightSystem{_scene},
    _debugOverlay{_gui, _physicsSystem, _scene, _textureManager} {
    // Setup GUI
    _gui.loadWidgetsFromFile(_guiFile);
    _sceneCanvas = _gui.get<tgui::CanvasSFML>("sceneCanvas");
    _sceneCanvas->moveToBack();
    _debugOverlay.buildGui();
    _currentWindowSize = _window.getSize();
    updateView();
    // Load the game
    registerComponents();
    loadResources();
    loadScene(_scene, setupFile, _fontManager, _textureManager, _tguiTextureManager, _gui);
    // Give references to systems. This has to be done once the GUI is set up
    // and the resources loaded.
    _renderSystem.setRenderTarget(_sceneCanvas->getRenderTexture());
    _lightSystem.setRenderTarget(_sceneCanvas->getRenderTexture());
    _lightSystem.setShader(_shaderManager.get("light"));
    _gameplaySystem.setRenderTarget(_sceneCanvas->getRenderTexture());
}

void Application::run() {
    sf::Clock clock;
    while (_window.isOpen()) {
        // Handle events
        sf::Event event;
        while (_window.pollEvent(event)) {
            // TGUI needs to handle the events first in order for resizing
            // the window to work properly
            _gui.handleEvent(event);

            if (event.type == sf::Event::Closed) {
                _window.close();
            } else if (event.type == sf::Event::Resized) {
                updateView();
            }

            // Dispatch remaining events to various in-game UI
            if (_debugOverlay.handleEvent(event)) {
                continue;
            } else if (_inputManager.handleEvent(event)) {
                continue;
            }
        }

        // Update various systems
        sf::Time elapsedTime{clock.restart()};
        _physicsSystem.updateTime(elapsedTime);
        _renderSystem.update(elapsedTime);
        _debugOverlay.update();
        _gameplaySystem.handleTriggerActions(_inputManager.getTriggerActions());
        _gameplaySystem.handleContinuousActions(elapsedTime, _inputManager.getContinuousActions());
        _lightSystem.update();
        _musicManager.update();

        // Draw graphics
        _sceneCanvas->clear(sf::Color::Transparent);
        _sceneCanvas->draw(_renderSystem, &_shaderManager.get("light"));
        _sceneCanvas->draw(_debugOverlay);
        _sceneCanvas->display();
        _window.clear(sf::Color::Black);
        _window.draw(_backgroundSprite);
        _gui.draw();
        _window.display();
    }
}

void Application::registerComponents() {
    _scene.registerComponent<Body>();
    _scene.registerComponent<CircleBody>();
    _scene.registerComponent<ConvexBody>();
    _scene.registerComponent<Collider>();
    _scene.registerComponent<AnimationComponent>();
    _scene.registerComponent<LightSource>();
    _scene.registerComponent<Shadow>();
    _scene.registerComponent<Player>();
    _scene.registerComponent<DebugInfo>();
    _scene.registerComponent<MapElement>();
    _scene.registerComponent<sf::Sprite>();
    _scene.registerComponent<sf::CircleShape>();
    _scene.registerComponent<sf::ConvexShape>();
}

void Application::loadResources() {
    _fontManager.loadFromFile("resources/fonts/FreeSans.ttf", "debugFont");
    _tguiTextureManager.loadFromFile("resources/gui/play.png", "playButton");
    _tguiTextureManager.loadFromFile("resources/gui/playHover.png", "playHoverButton");
    _tguiTextureManager.loadFromFile("resources/gui/pause.png", "pauseButton");
    _tguiTextureManager.loadFromFile("resources/gui/pauseHover.png", "pauseHoverButton");
    _tguiTextureManager.loadFromFile("resources/gui/sunIcon.png", "sunIcon");
    _tguiTextureManager.loadFromFile("resources/gui/mercuryIcon.png", "mercuryIcon");
    _tguiTextureManager.loadFromFile("resources/gui/venusIcon.png", "venusIcon");
    _tguiTextureManager.loadFromFile("resources/gui/earthIcon.png", "earthIcon");
    _tguiTextureManager.loadFromFile("resources/gui/marsIcon.png", "marsIcon");
    _tguiTextureManager.loadFromFile("resources/gui/shipIcon.png", "shipIcon");
    _textureManager.loadFromFile("resources/textures/background.png", "background");
    _textureManager.loadFromFile("resources/textures/ship.png", "ship");
    _textureManager.loadFromFile("resources/textures/sun.png", "sun");
    _textureManager.loadFromFile("resources/textures/mercury.png", "mercury");
    _textureManager.loadFromFile("resources/textures/venus.png", "venus");
    _textureManager.loadFromFile("resources/textures/earth.png", "earth");
    _textureManager.loadFromFile("resources/textures/mars.png", "mars");
    _textureManager.loadFromFile("resources/textures/rcs.png", "rcs");
    _shaderManager.loadFromFile("resources/shaders/light.frag", "light", sf::Shader::Fragment);
    _musicManager.openFromFile("resources/musics/Aphelion.ogg");

    _backgroundSprite.setTexture(_textureManager.get("background"));
    // Center on an interesting region on the background
    _backgroundSprite.setOrigin(1500, 900);
}

void Application::updateView() {
    sf::View windowView{_window.getView()};
    Vector2f newWindowSize{static_cast<Vector2f>(_window.getSize())};
    windowView.setSize(newWindowSize);
    windowView.setCenter(newWindowSize / 2.f);
    _window.setView(windowView);

    sf::View canvasView{_sceneCanvas->getView()};
    float previousZoom{static_cast<float>(_currentWindowSize.x) / canvasView.getSize().x};
    canvasView.setSize(newWindowSize / previousZoom);
    _sceneCanvas->setView(canvasView);
    _currentWindowSize = _window.getSize();
}
