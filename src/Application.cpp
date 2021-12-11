#include <Application.hpp>
#include <format.hpp>

Application::Application(const std::string& setupFile):
        _window{sf::VideoMode(1200, 600), "Physics Simulation"},
        _gui{_window},
        _model{setupFile} {
    buildGui();
}

void Application::run() {
    sf::Clock clock;

    while (_window.isOpen()) {
        // Update
        sf::Time elapsedTime{clock.restart()};
        _model.updateTime(elapsedTime);
        _simulationCanvas->update(elapsedTime);
        updateDisplays();

        // Handle events
        sf::Event event;
        while (_window.pollEvent(event)) {
            _gui.handleEvent(event);

            if (event.type == sf::Event::Closed) {
                _window.close();
            }
        }

        // Draw graphics
        _window.clear(sf::Color::White);
        _simulationCanvas->clear(sf::Color::Black);
        _simulationCanvas->draw(_model);
        _simulationCanvas->display();
        _gui.draw();
        _window.display();
    }
}

void Application::buildGui() {
	_gui.loadWidgetsFromFile(_guiFile);

    // Spin controls. Do not work with GUI text file import for some reason.
    auto timeSpeedControl = tgui::SpinControl::create(-100, 100, 1, 2, 0.1);
    timeSpeedControl->setPosition("timeSpeedLabel.right", "timeSpeedLabel.top");
    timeSpeedControl->setSize(124, 16);
    timeSpeedControl->onValueChange([this] (float value) {
        if(not _paused) {
            _model.setTimeScale(value);
        }
    });
    _gui.get<tgui::ChildWindow>("controlsPanel")->add(timeSpeedControl, "timeSpeedcontrol");

    // Other bindings
    _gui.get<tgui::Button>("stepBackButton")->onPress([this] () {
        pauseTime();
        _model.updateSteps(-1);
    });
    _gui.get<tgui::Button>("stepForwardButton")->onPress([this] () {
        pauseTime();
        _model.updateSteps(1);
    });
    auto pauseButton = _gui.get<tgui::Button>("pauseButton");
    _gui.get<tgui::Button>("pauseButton")->onPress([this, pauseButton, timeSpeedControl] () {
        if (not _paused) {
            pauseTime();
        } else {
            // Start time again
            pauseButton->getRenderer()->setTexture("resources/pause.png");
            pauseButton->getRenderer()->setTextureHover("resources/pause_hover.png");
            _paused = false;
            _model.setTimeScale(timeSpeedControl->getValue());
        }
    });

    // Simulation canvas
    _simulationCanvas = SimulationCanvas::create(_model);
    _simulationCanvas->setPosition(0, 0);
    _simulationCanvas->setSize({"100%", "100%"});
    _gui.add(_simulationCanvas, "simulationCanvas");
    _simulationCanvas->moveToBack();
}

void Application::updateDisplays() {
    _gui.get<tgui::Label>("timeSecondsDisplay")->setText(formatTime(_model.getElapsedTime()));
    _gui.get<tgui::Label>("timeStepsDisplay")->setText(std::to_string(_model.getStepCounter()));
}

void Application::pauseTime() {
    auto renderer = _gui.get<tgui::Button>("pauseButton")->getRenderer();
    renderer->setTexture("resources/play.png");
    renderer->setTextureHover("resources/play_hover.png");
    _model.setTimeScale(0);
    _paused = true;
}
