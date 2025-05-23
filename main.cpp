#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <iostream>
#include <random>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <windows.h>
#include <fstream>
#include <ctime>
using namespace std;

std::string getResourcePath(const std::string& filename) {
    static std::string currentDirectory;

    if (currentDirectory.empty()) {
        DWORD bufferSize = GetCurrentDirectory(0, NULL);
        if (bufferSize > 0) {
            std::vector<TCHAR> buffer(bufferSize);
            if (GetCurrentDirectory(bufferSize, buffer.data())) {
#ifdef UNICODE
                int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
                std::string converted(sizeNeeded, 0);
                WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, &converted[0], sizeNeeded, NULL, NULL);
                currentDirectory = converted.c_str();
#else
                currentDirectory = buffer.data();
#endif
                if (!currentDirectory.empty() && currentDirectory.back() == '\\') {
                    currentDirectory.pop_back();
                }
            }
        }
    }

    std::string fullPath = currentDirectory + "\\" + filename;
    return fullPath;
}

sf::VideoMode desktopMode = sf::VideoMode(1920, 1080);
const float maxWindowWidth = static_cast<float>(desktopMode.width);
const float maxWindowHeight = static_cast<float>(desktopMode.height);

int Width = 16;
int Height = 16;
int Mines = 10;
float globalVolume = 50.0f;
bool showFirstSafeCell = false;

class Cell {
public:
    uint8_t flags = 0;
    int8_t NMines = 0;

    bool isMine() const { return flags & 0x01; }
    bool isRevealed() const { return flags & 0x02; }
    bool isFlagged() const { return flags & 0x04; }
    bool isFirstSafe() const { return flags & 0x08; }

    void setMine(bool val) { val ? flags |= 0x01 : flags &= ~0x01; }
    void setRevealed(bool val) { val ? flags |= 0x02 : flags &= ~0x02; }
    void setFlagged(bool val) { val ? flags |= 0x04 : flags &= ~0x04; }
    void setFirstSafe(bool val) { val ? flags |= 0x08 : flags &= ~0x08; }
};

class Minesweeper {
private:
    std::vector<Cell> board;
    int boardWidth = 0;
    int boardHeight = 0;
    bool levelCompleted = false;

    inline size_t index(int x, int y) const { return y * boardWidth + x; }

public:
    void reset() {
        levelCompleted = false;
        boardWidth = Width;
        boardHeight = Height;
        board.assign(boardWidth * boardHeight, Cell());

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distX(0, boardWidth - 1);
        std::uniform_int_distribution<> distY(0, boardHeight - 1);

        for (int i = 0; i < Mines; ) {
            int x = distX(gen);
            int y = distY(gen);
            if (!board[index(x, y)].isMine()) {
                board[index(x, y)].setMine(true);
                i++;
            }
        }
        for (int y = 0; y < boardHeight; y++) {
            for (int x = 0; x < boardWidth; x++) {
                if (board[index(x, y)].isMine()) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < boardWidth && ny >= 0 && ny < boardHeight) {
                                board[index(nx, ny)].NMines++;
                            }
                        }
                    }
                }
            }
        }

        if (showFirstSafeCell) {
            for (int y = 0; y < boardHeight; y++) {
                for (int x = 0; x < boardWidth; x++) {
                    if (!board[index(x, y)].isMine() && board[index(x, y)].NMines == 0) {
                        board[index(x, y)].setFirstSafe(true);
                        return;
                    }
                }
            }
        }
    }

    void revealCell(int x, int y) {
        if (x < 0 || x >= boardWidth || y < 0 || y >= boardHeight ||
            board[index(x, y)].isRevealed() || board[index(x, y)].isFlagged())
            return;

        board[index(x, y)].setRevealed(true);
        board[index(x, y)].setFirstSafe(false);

        if (board[index(x, y)].NMines == 0) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    revealCell(x + dx, y + dy);
                }
            }
        }
    }

    void toggleFlag(int x, int y) {
        if (x < 0 || x >= boardWidth || y < 0 || y >= boardHeight || board[index(x, y)].isRevealed())
            return;
        board[index(x, y)].setFlagged(!board[index(x, y)].isFlagged());
    }

    const Cell& getCell(int x, int y) const { return board[index(x, y)]; }
    bool isMine(int x, int y) const { return board[index(x, y)].isMine(); }
    bool isLevelCompleted() const { return levelCompleted; }

    void checkLevelCompletion() {
        for (const auto& cell : board) {
            if (!cell.isMine() && !cell.isRevealed())
                return;
        }
        levelCompleted = true;
    }
};

class ResourceManager {
private:
    struct GameTextures {
        sf::Texture mine;
        sf::Texture flag;
        sf::Texture restartButton;
        sf::Texture safeCell;
        sf::Texture settingsBackground;
        sf::Texture menuBackground;
        sf::Texture title;
        sf::Texture gameBackground;
        sf::Texture wheelTexture;
        sf::Texture equipButton;
        sf::Texture mineExplosionTexture;
        sf::Texture metalDetector;
        sf::Texture bootTexture;
    };


    struct GameSounds {
        sf::SoundBuffer explosion;
        sf::SoundBuffer levelPassed;
    };

    GameTextures textures;
    GameSounds sounds;
    bool texturesLoaded = false;
    bool soundsLoaded = false;
    sf::Music menuMusic;
    sf::Music gameMusic;
    sf::Sound explosionSound;
    sf::Sound levelPassedSound;

public:
    bool loadEssentialResources() {
        if (!textures.menuBackground.loadFromFile(getResourcePath("images(textures,sprites)\\background_2.png")) ||
            !textures.title.loadFromFile(getResourcePath("images(textures,sprites)\\Title.png"))) {
            return false;
        }
        return true;
    }

    bool loadMenuResources() {
        if (!textures.settingsBackground.loadFromFile(getResourcePath("images(textures,sprites)\\settings_background_sprite.jpg"))) {
            return false;
        }
        return true;
    }

    bool loadGameResources() {
        if (!textures.mine.loadFromFile(getResourcePath("images(textures,sprites)\\mine_texture.png")) ||
            !textures.flag.loadFromFile(getResourcePath("images(textures,sprites)\\flag_sprite.png")) ||
            !textures.restartButton.loadFromFile(getResourcePath("images(textures,sprites)\\restart_button.png")) ||
            !textures.safeCell.loadFromFile(getResourcePath("images(textures,sprites)\\safe_cell.png")) ||
            !textures.gameBackground.loadFromFile(getResourcePath("images(textures,sprites)\\game_background.png")) ||
            !textures.wheelTexture.loadFromFile(getResourcePath("images(textures,sprites)\\wheel_spin.png")) ||
            !textures.equipButton.loadFromFile(getResourcePath("images(textures,sprites)\\equip_button.png")) ||
            !textures.mineExplosionTexture.loadFromFile(getResourcePath("images(textures,sprites)\\mine_explosion_texture.png")) ||
            !textures.metalDetector.loadFromFile(getResourcePath("images(textures,sprites)\\metal_detector.png")) ||
            !textures.bootTexture.loadFromFile(getResourcePath("images(textures,sprites)\\boot_texture.png"))) {
            return false;
        }
        texturesLoaded = true;
        return true;
    }

    bool loadSounds() {
        if (!sounds.explosion.loadFromFile(getResourcePath("music+sounds\\explosion_sound.wav")) ||
            !sounds.levelPassed.loadFromFile(getResourcePath("music+sounds\\level_passed_sound.mp3"))) {
            return false;
        }
        explosionSound.setBuffer(sounds.explosion);
        levelPassedSound.setBuffer(sounds.levelPassed);
        soundsLoaded = true;
        return true;
    }

    bool loadMenuMusic() {
        if (!menuMusic.openFromFile(getResourcePath("music+sounds\\menu_music.mp3"))) {
            return false;
        }
        menuMusic.setLoop(true);
        menuMusic.setVolume(globalVolume);
        return true;
    }

    bool loadGameMusic() {
        if (!gameMusic.openFromFile(getResourcePath("music+sounds\\main_game_soundtrack.wav"))) {
            return false;
        }
        gameMusic.setLoop(true);
        gameMusic.setVolume(globalVolume);
        return true;
    }

    void unloadGameResources() {
        textures.mine = sf::Texture();
        textures.flag = sf::Texture();
        textures.restartButton = sf::Texture();
        textures.safeCell = sf::Texture();
        textures.gameBackground = sf::Texture();

        sounds.explosion = sf::SoundBuffer();
        sounds.levelPassed = sf::SoundBuffer();

        explosionSound = sf::Sound();
        levelPassedSound = sf::Sound();

        gameMusic.stop();
        texturesLoaded = false;
        soundsLoaded = false;
    }

    GameTextures& getTextures() { return textures; }
    const GameTextures& getTextures() const { return textures; }
    const GameSounds& getSounds() const { return sounds; }
    sf::Music& getMenuMusic() { return menuMusic; }
    sf::Music& getGameMusic() { return gameMusic; }
    sf::Sound& getExplosionSound() { return explosionSound; }
    sf::Sound& getLevelPassedSound() { return levelPassedSound; }
    bool areTexturesLoaded() const { return texturesLoaded; }
    bool areSoundsLoaded() const { return soundsLoaded; }
};

class MusicController {
private:
    sf::Music* currentMusic = nullptr;
    sf::Music* nextMusic = nullptr;
    float currentVolume = globalVolume;
    float targetVolume = globalVolume;
    const float fadeSpeed = 80.0f;
    bool isFading = false;
    bool isSwitching = false;
    bool isPaused = false;
    sf::Time pausePosition;
    float storedVolume = globalVolume;

public:
    void update(float deltaTime) {
        if (isFading) {
            float volumeChange = fadeSpeed * deltaTime;

            if (currentVolume < targetVolume) {
                currentVolume = std::min(currentVolume + volumeChange, targetVolume);
            }
            else {
                currentVolume = std::max(currentVolume - volumeChange, targetVolume);
            }

            if (currentMusic) {
                currentMusic->setVolume(currentVolume);
            }

            if (std::abs(currentVolume - targetVolume) < 1.0f) {
                currentVolume = targetVolume;
                if (currentMusic) {
                    currentMusic->setVolume(currentVolume);
                }
                isFading = false;

                if (isSwitching && currentVolume <= 0.0f && nextMusic) {
                    if (currentMusic) {
                        currentMusic->pause();
                    }
                    currentMusic = nextMusic;
                    nextMusic = nullptr;
                    if (isPaused) {
                        currentMusic->play();
                        currentMusic->setPlayingOffset(pausePosition);
                        isPaused = false;
                    }
                    else {
                        currentMusic->play();
                    }
                    fadeIn();
                    isSwitching = false;
                }
                else if (currentVolume <= 0.0f && currentMusic) {
                    pausePosition = currentMusic->getPlayingOffset();
                    currentMusic->pause();
                    isPaused = true;
                }
            }
        }
    }

    void switchTo(sf::Music& newMusic) {
        if (currentMusic != &newMusic) {
            storedVolume = globalVolume;
            nextMusic = &newMusic;
            isSwitching = true;
            fadeOut();
        }
    }

    void fadeIn() {
        if (currentMusic) {
            targetVolume = globalVolume;
            isFading = true;
        }
    }

    void fadeOut() {
        if (currentMusic) {
            targetVolume = 0.0f;
            isFading = true;
        }
    }

    void play(sf::Music& music) {
        if (currentMusic != &music) {
            if (currentMusic) {
                currentMusic->stop();
            }
            currentMusic = &music;
            currentMusic->setVolume(globalVolume);
            currentMusic->play();
            isPaused = false;
        }
        else if (currentMusic && currentMusic->getStatus() != sf::Music::Playing) {
            if (isPaused) {
                currentMusic->play();
                currentMusic->setPlayingOffset(pausePosition);
                isPaused = false;
            }
            else {
                currentMusic->play();
            }
        }
    }

    void pause() {
        if (currentMusic && currentMusic->getStatus() == sf::Music::Playing) {
            pausePosition = currentMusic->getPlayingOffset();
            currentMusic->pause();
            isPaused = true;
        }
    }

    void resume() {
        if (currentMusic && isPaused) {
            currentMusic->play();
            currentMusic->setPlayingOffset(pausePosition);
            isPaused = false;
            fadeIn();
        }
    }
};

class AnimatedBackground {
private:
    std::vector<sf::Texture> frames;
    sf::Sprite currentFrame;
    int currentFrameIndex = 0;
    sf::Clock animationClock;
    float frameTime = 0.05f;
    bool loaded = false;

public:
    bool loadFrames() {
        if (loaded) return true;
        frames.clear();
        for (int i = 0; i <= 140; ++i) {
            sf::Texture texture;
            std::string path = getResourcePath("images(textures,sprites)\\frame");
            if (i < 10) path += "000";
            else if (i < 100) path += "00";
            else path += "0";
            path += std::to_string(i) + ".png";
            if (!texture.loadFromFile(path)) {
                frames.clear();
                return false;
            }
            frames.push_back(texture);
        }
        currentFrame.setTexture(frames[0]);
        loaded = true;
        return true;
    }

    void update() {
        if (!loaded) return;
        if (animationClock.getElapsedTime().asSeconds() >= frameTime) {
            currentFrameIndex = (currentFrameIndex + 1) % frames.size();
            currentFrame.setTexture(frames[currentFrameIndex]);
            animationClock.restart();
        }
    }

    void draw(sf::RenderWindow& window) {
        if (!loaded) return;
        float scaleX = static_cast<float>(window.getSize().x) / 480.0f;
        float scaleY = static_cast<float>(window.getSize().y) / 270.0f;
        currentFrame.setScale(scaleX, scaleY);
        window.draw(currentFrame);
    }

    void cleanup() {
        frames.clear();
        loaded = false;
    }
};

class GameBackground {
private:
    sf::Texture texture;
    std::vector<sf::IntRect> tileVariants;
    std::vector<std::vector<int>> tileMap;
    bool initialized = false;

public:
    bool loadTexture() {
        if (!texture.loadFromFile(getResourcePath("images(textures,sprites)\\game_background.png"))) {
            return false;
        }

        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                tileVariants.push_back(sf::IntRect(x * 48, y * 48, 48, 48));
            }
        }
        return true;
    }

    void initialize(int width, int height) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 8);

        tileMap.resize(height, std::vector<int>(width));
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                tileMap[y][x] = dist(gen);
            }
        }
        initialized = true;
    }

    void draw(sf::RenderWindow& window, float cellSize, float offsetX, float offsetY) {
        if (!initialized) return;

        sf::Sprite sprite;
        sprite.setTexture(texture);
        float scale = cellSize / 49.0f;

        for (int y = 0; y < tileMap.size(); y++) {
            for (int x = 0; x < tileMap[y].size(); x++) {
                sprite.setTextureRect(tileVariants[tileMap[y][x]]);
                sprite.setScale(scale, scale);
                sprite.setPosition(offsetX + x * cellSize, offsetY + y * cellSize);
                window.draw(sprite);
            }
        }
    }
};

class FadeEffect {
private:
    sf::RectangleShape overlay;
    float fadeAlpha;
    float fadeSpeed;
    bool isFadingIn;
    bool isActive;

public:
    FadeEffect() : fadeAlpha(255.0f), fadeSpeed(80.0f), isFadingIn(false), isActive(false) {
        overlay.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(fadeAlpha)));
    }

    void startFadeIn() {
        fadeAlpha = 255.0f;
        isFadingIn = true;
        isActive = true;
    }

    void startFadeOut() {
        fadeAlpha = 0.0f;
        isFadingIn = false;
        isActive = true;
    }

    void update(float deltaTime) {
        if (!isActive) return;

        if (isFadingIn) {
            fadeAlpha -= fadeSpeed * deltaTime;
            if (fadeAlpha <= 0.0f) {
                fadeAlpha = 0.0f;
                isActive = false;
            }
        }
        else {
            fadeAlpha += fadeSpeed * deltaTime;
            if (fadeAlpha >= 255.0f) {
                fadeAlpha = 255.0f;
                isActive = false;
            }
        }

        overlay.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(fadeAlpha)));
    }

    void draw(sf::RenderWindow& window) {
        if (!isActive) return;
        overlay.setSize(sf::Vector2f(window.getSize()));
        window.draw(overlay);
    }

    bool isComplete() const { return !isActive; }
};

class Button {
private:
    sf::RectangleShape shape;
    sf::Text text;
    sf::Vector2f originalSize;
    sf::Vector2f originalPosition;
    bool isHovered;
    float currentScale;
    float targetScale;
    float animationSpeed;

public:
    Button(const sf::Vector2f& size, const sf::Vector2f& position, const std::string& buttonText, const sf::Font& font)
        : originalSize(size), originalPosition(position), isHovered(false),
        currentScale(1.0f), targetScale(1.0f), animationSpeed(5.0f) {
        shape.setSize(size);
        shape.setPosition(position);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(2.f);
        shape.setOutlineColor(sf::Color::Transparent);
        text.setString(buttonText);
        text.setFont(font);
        text.setCharacterSize(60);
        text.setFillColor(sf::Color(255, 255, 255, 100));
        text.setOutlineColor(sf::Color(50, 50, 50, 170));
        sf::FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f,
            textRect.top + textRect.height / 2.0f);
        text.setPosition(position.x + size.x / 2.0f,
            position.y + size.y / 2.0f);
    }

    void update(const sf::Vector2f& mousePos, float deltaTime) {
        bool wasHovered = isHovered;
        isHovered = shape.getGlobalBounds().contains(mousePos);
        if (isHovered) {
            targetScale = 1.1f;
            text.setFillColor(sf::Color(255, 255, 0, 180));
            text.setOutlineColor(sf::Color(80, 80, 0, 220));
        }
        else {
            targetScale = 1.0f;
            text.setFillColor(sf::Color(255, 255, 255, 100));
            text.setOutlineColor(sf::Color(50, 50, 50, 170));
        }
        float scaleDiff = targetScale - currentScale;
        if (fabs(scaleDiff) > 0.01f) {
            currentScale += scaleDiff * animationSpeed * deltaTime;
        }
        else {
            currentScale = targetScale;
        }
        shape.setSize(originalSize * currentScale);
        shape.setPosition(originalPosition - (originalSize * (currentScale - 1.0f) / 2.0f));
        text.setScale(currentScale, currentScale);
    }

    void draw(sf::RenderWindow& window) const {
        window.draw(shape);
        window.draw(text);
    }
    void setPosition(const sf::Vector2f& position) {
        originalPosition = position;
        shape.setPosition(position);
        sf::FloatRect textRect = text.getLocalBounds();
        text.setPosition(position.x + shape.getSize().x / 2.0f,
            position.y + shape.getSize().y / 2.0f);
    }
    bool isClicked(const sf::Vector2f& mousePos, const sf::Event& event) const {
        return event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left &&
            shape.getGlobalBounds().contains(mousePos);
    }
};

class AnimatedTitle {
private:
    sf::Sprite sprite;
    sf::Texture texture;
    float originalScale;
    float angle;
    float scaleFactor;
    bool growing;
    float rotationSpeed;
    float scaleSpeed;

public:
    AnimatedTitle(const std::string& texturePath, const sf::Vector2f& position)
        : angle(0.f), scaleFactor(1.f), growing(true), rotationSpeed(5.f), scaleSpeed(0.1f) {
        texture.loadFromFile(texturePath);
        sprite.setTexture(texture);
        originalScale = 3.0f;
        sf::FloatRect bounds = sprite.getLocalBounds();
        sprite.setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        sprite.setPosition(position);
    }

    void update(float deltaTime) {
        angle += rotationSpeed * deltaTime;
        if (abs(angle) >= 5.f) {
            rotationSpeed = -rotationSpeed;
            angle = (angle > 0) ? 5.f : -5.f;
        }
        if (growing) {
            scaleFactor += scaleSpeed * deltaTime;
            if (scaleFactor >= 1.1f) {
                scaleFactor = 1.1f;
                growing = false;
            }
        }
        else {
            scaleFactor -= scaleSpeed * deltaTime;
            if (scaleFactor <= 0.9f) {
                scaleFactor = 0.9f;
                growing = true;
            }
        }
        sprite.setRotation(angle);
        sprite.setScale(originalScale * scaleFactor, originalScale * scaleFactor);
    }

    void draw(sf::RenderWindow& window) const {
        window.draw(sprite);
    }
};

class ScrollingBackground {
private:
    sf::Sprite sprite1;
    sf::Sprite sprite2;
    sf::Texture texture;
    float scrollSpeed;
    float windowWidth;
    float windowHeight;

public:
    ScrollingBackground(const std::string& texturePath, float speed = 50.f)
        : scrollSpeed(speed), windowWidth(1920.f), windowHeight(1080.f) {
        texture.loadFromFile(texturePath);
        sprite1.setTexture(texture);
        sprite2.setTexture(texture);
        float scaleX = windowWidth / texture.getSize().x;
        float scaleY = windowHeight / texture.getSize().y;
        sprite1.setScale(scaleX, scaleY);
        sprite2.setScale(scaleX, scaleY);
        sprite1.setPosition(0.f, 0.f);
        sprite2.setPosition(sprite1.getGlobalBounds().width, 0.f);
    }

    void update(float deltaTime) {
        float movement = scrollSpeed * deltaTime;
        sprite1.move(-movement, 0.f);
        sprite2.move(-movement, 0.f);
        if (sprite1.getPosition().x + sprite1.getGlobalBounds().width <= 0) {
            sprite1.setPosition(sprite2.getPosition().x + sprite2.getGlobalBounds().width, 0.f);
        }
        if (sprite2.getPosition().x + sprite2.getGlobalBounds().width <= 0) {
            sprite2.setPosition(sprite1.getPosition().x + sprite1.getGlobalBounds().width, 0.f);
        }
    }

    void draw(sf::RenderWindow& window) const {
        window.draw(sprite1);
        window.draw(sprite2);
    }
};

class GuideScreen {
private:
    sf::RectangleShape overlay;
    sf::Sprite guideSprite;
    std::vector<sf::Texture> guideTextures;
    std::vector<std::string> guideMessages;
    int currentFrame = 0;
    sf::Text guideText;
    sf::Text pageText;
    sf::Font font;
    Button nextButton;
    bool active = false;
public:
    GuideScreen(const sf::Font& buttonFont)
        : nextButton(sf::Vector2f(40.f, 70.f), sf::Vector2f(0.f, 0.f), "OK", buttonFont) {
        overlay.setFillColor(sf::Color(0, 0, 0, 180));

        if (!font.loadFromFile(getResourcePath("fonts\\Gamepixies.ttf"))) {
        }
        guideMessages = {
            "Welcome to Minesweeper Guide!\nTo reveal the square, you should click\n left mouse button on it.",
            "Number on square means how many mines\n in area around this square. Here you can see two\n it means 2 mines on empty squares near to it",
            "Here you see a situation in which \n it is not possible to accurately determine if \n the mine here",
            "or there",
            "In situations like these you should \n use items, but I recommend that you  find out how they work yourself. "
        };

        for (int i = 1; i <= 5; ++i) {
            sf::Texture texture;
            std::string path = getResourcePath("images(textures,sprites)\\guide_frame_" + std::to_string(i) + ".png");
            if (texture.loadFromFile(path)) {
                guideTextures.push_back(texture);
            }
        }

        guideText.setFont(font);
        guideText.setCharacterSize(24);
        guideText.setFillColor(sf::Color::White);
        guideText.setString(guideMessages[0]);

        pageText.setFont(font);
        pageText.setCharacterSize(20);
        pageText.setFillColor(sf::Color::White);
    }


    void show() {
        if (!guideTextures.empty()) {
            currentFrame = 0;
            active = true;
            updateDisplay();
        }
    }

    void hide() {
        active = false;
    }

    bool isActive() const {
        return active;
    }

    void update(const sf::Vector2f& mousePos, const sf::Event& event) {
        if (!active) return;

        nextButton.update(mousePos, 0.016f);

        if (nextButton.isClicked(mousePos, event)) {
            if (currentFrame < guideTextures.size() - 1) {
                currentFrame++;
                updateDisplay();
            }
            else {
                hide();
            }
        }
    }

    void updateDisplay() {
        if (currentFrame < guideTextures.size()) {
            guideSprite.setTexture(guideTextures[currentFrame]);
            guideText.setString(guideMessages[currentFrame]);
            pageText.setString("Page " + std::to_string(currentFrame + 1) + " of " + std::to_string(guideTextures.size()));
        }
    }

    void draw(sf::RenderWindow& window) {
        if (!active) return;

        sf::Vector2f windowSize = static_cast<sf::Vector2f>(window.getSize());
        overlay.setSize(windowSize);
        window.draw(overlay);

        float scale = std::min(windowSize.x * 0.6f / 326.f, windowSize.y * 0.6f / 320.f);
        sf::Vector2f spriteSize(326.f * scale, 320.f * scale);
        sf::Vector2f spritePos((windowSize.x - spriteSize.x) / 2.f, (windowSize.y - spriteSize.y) / 2.f - 50.f);

        guideSprite.setPosition(spritePos);
        guideSprite.setScale(scale, scale);
        window.draw(guideSprite);

        guideText.setPosition(windowSize.x / 2.f, spritePos.y + spriteSize.y + 30.f);
        guideText.setOrigin(guideText.getLocalBounds().width / 2.f, 0.f);
        window.draw(guideText);

        pageText.setPosition(windowSize.x / 2.f, spritePos.y + spriteSize.y + 170.0f);
        pageText.setOrigin(pageText.getLocalBounds().width / 2.f, 0.f);
        window.draw(pageText);

        nextButton.setPosition(sf::Vector2f(
            (windowSize.x - 100.f) / 2.f,
            spritePos.y + spriteSize.y + 120.f
        ));
        nextButton.draw(window);
    }

    void setGuideText(const std::string& text) {
        guideText.setString(text);
    }
};

class MainMenu {
private:
    ScrollingBackground background;
    AnimatedTitle title;
    std::vector<Button> buttons;
    sf::Font font;
    GuideScreen guideScreen;

public:
    MainMenu()
        : background(getResourcePath("images(textures,sprites)\\background_2.png")),
        title(getResourcePath("images(textures,sprites)\\Title.png"),
            sf::Vector2f(1920.f / 2.f, 200.f)),
        guideScreen(font) {
        font.loadFromFile(getResourcePath("fonts\\Gamepixies.ttf"));
        float buttonWidth = 300.f;
        float buttonHeight = 80.f;
        float startY = 400.f;
        float padding = 30.f;

        buttons.emplace_back(sf::Vector2f(buttonWidth, buttonHeight),
            sf::Vector2f((1920.f - buttonWidth) / 2.f, startY),
            "Start", font);
        buttons.emplace_back(sf::Vector2f(buttonWidth, buttonHeight),
            sf::Vector2f((1920.f - buttonWidth) / 2.f, startY + buttonHeight + padding),
            "Settings", font);
        buttons.emplace_back(sf::Vector2f(buttonWidth, buttonHeight),
            sf::Vector2f((1920.f - buttonWidth) / 2.f, startY + 2 * (buttonHeight + padding)),
            "Guide", font);
        buttons.emplace_back(sf::Vector2f(buttonWidth, buttonHeight),
            sf::Vector2f((1920.f - buttonWidth) / 2.f, startY + 3 * (buttonHeight + padding)),
            "Quit", font);
    }

    void update(float deltaTime, const sf::Vector2f& mousePos) {
        background.update(deltaTime);
        title.update(deltaTime);
        for (auto& button : buttons) {
            button.update(mousePos, deltaTime);
        }
        guideScreen.update(mousePos, sf::Event());
    }

    void draw(sf::RenderWindow& window) {
        background.draw(window);
        title.draw(window);
        for (const auto& button : buttons) {
            button.draw(window);
        }
        if (guideScreen.isActive()) {
            guideScreen.draw(window);
        }
    }

    int handleEvents(const sf::Event& event, const sf::Vector2f& mousePos) {
        if (guideScreen.isActive()) {
            guideScreen.update(mousePos, event);
            return 0;
        }

        if (buttons[0].isClicked(mousePos, event)) return 1;
        if (buttons[1].isClicked(mousePos, event)) return 2;
        if (buttons[2].isClicked(mousePos, event)) {
            guideScreen.setGuideText("This is the guide screen.\nHere you can learn how to play the game.");
            guideScreen.show();
            return 0;
        }
        if (buttons[3].isClicked(mousePos, event)) return 3;
        return 0;
    }
};


class SettingsMenu {
private:
    sf::Font font;
    sf::Sprite backgroundSprite;
    sf::Text backButton;
    sf::Text volumeText;
    sf::Text minesText;
    sf::Text widthText;
    sf::Text heightText;
    sf::Text safeCellText;
    sf::RectangleShape volumeBar;
    sf::RectangleShape volumeSlider;
    sf::RectangleShape minesBar;
    sf::RectangleShape minesSlider;
    sf::RectangleShape widthBar;
    sf::RectangleShape widthSlider;
    sf::RectangleShape heightBar;
    sf::RectangleShape heightSlider;
    sf::RectangleShape safeCellCheckbox;
    sf::RectangleShape safeCellCheckmark;
    bool sliderGrabbed = false;
    enum class SliderType { None, Volume, Mines, Width, Height } activeSlider = SliderType::None;
    float menuScale = 1.0f;
    sf::Vector2f menuOffset;

public:
    bool initialize(const sf::Texture& backgroundTexture) {
        font.loadFromFile(getResourcePath("fonts\\Gamepixies.ttf"));
        backgroundSprite.setTexture(backgroundTexture);
        setupButton(backButton, "Back", 400.0f, 650.0f);
        setupSlider(volumeText, volumeBar, volumeSlider, "Volume:", 150.0f);
        setupSlider(minesText, minesBar, minesSlider, "Mines:", 250.0f);
        setupSlider(widthText, widthBar, widthSlider, "Width:", 350.0f);
        setupSlider(heightText, heightBar, heightSlider, "Height:", 450.0f);
        safeCellText.setFont(font);
        safeCellText.setString("Show first safe cell:");
        safeCellText.setCharacterSize(30);
        safeCellText.setFillColor(sf::Color(255, 255, 255, 180));
        safeCellText.setOutlineColor(sf::Color(50, 50, 50, 220));
        safeCellText.setOutlineThickness(2.0f);
        safeCellText.setPosition(150.0f, 550.0f);
        safeCellCheckbox.setSize(sf::Vector2f(20.0f, 20.0f));
        safeCellCheckbox.setOutlineThickness(4.0f);
        safeCellCheckbox.setOutlineColor(sf::Color(36, 36, 36));
        safeCellCheckbox.setFillColor(sf::Color(211, 211, 211));
        safeCellCheckbox.setPosition(440.0f, 560.0f);
        safeCellCheckmark.setSize(sf::Vector2f(20.0f, 20.0f));
        safeCellCheckmark.setFillColor(sf::Color(128, 128, 128));
        safeCellCheckmark.setPosition(440.0f, 560.0f);
        return true;
    }

    void setupButton(sf::Text& button, const std::string& text, float centerX, float y) {
        button.setFont(font);
        button.setString(text);
        button.setCharacterSize(40);
        button.setFillColor(sf::Color(255, 255, 255, 180));
        button.setOutlineColor(sf::Color(50, 50, 50, 220));
        button.setOutlineThickness(2.0f);
        button.setOrigin(button.getLocalBounds().width / 2.0f, 0.0f);
        button.setPosition(centerX, y);
    }

    void setupSlider(sf::Text& text, sf::RectangleShape& bar, sf::RectangleShape& slider,
        const std::string& label, float centerY) {
        text.setFont(font);
        text.setString(label);
        text.setCharacterSize(30);
        text.setFillColor(sf::Color(255, 255, 255, 180));
        text.setOutlineColor(sf::Color(50, 50, 50, 220));
        text.setOutlineThickness(2.0f);
        text.setPosition(150.0f, centerY);
        bar.setSize(sf::Vector2f(300.0f, 10.0f));
        bar.setFillColor(sf::Color(255, 255, 255, 180));
        bar.setPosition(350.0f, centerY + 20.0f);
        slider.setSize(sf::Vector2f(10.0f, 30.0f));
        slider.setFillColor(sf::Color(255, 0, 0, 200));
        slider.setPosition(bar.getPosition().x, centerY + 5.0f);
    }

    void updateMenuView(sf::RenderWindow& window) {
        float windowWidth = static_cast<float>(window.getSize().x);
        float windowHeight = static_cast<float>(window.getSize().y);
        menuScale = std::min(windowWidth / 800.0f, windowHeight / 800.0f);
        menuOffset.x = (windowWidth - 800.0f * menuScale) / 2.0f;
        menuOffset.y = (windowHeight - 800.0f * menuScale) / 2.0f;
    }

    void draw(sf::RenderWindow& window) {
        updateMenuView(window);
        backgroundSprite.setScale(
            static_cast<float>(window.getSize().x) / static_cast<float>(backgroundSprite.getTexture()->getSize().x),
            static_cast<float>(window.getSize().y) / static_cast<float>(backgroundSprite.getTexture()->getSize().y)
        );
        window.draw(backgroundSprite);
        sf::View currentView = window.getView();
        sf::View menuView(sf::FloatRect(0.0f, 0.0f, 800.0f, 800.0f));
        menuView.setViewport(sf::FloatRect(
            menuOffset.x / static_cast<float>(window.getSize().x),
            menuOffset.y / static_cast<float>(window.getSize().y),
            (800.0f * menuScale) / static_cast<float>(window.getSize().x),
            (800.0f * menuScale) / static_cast<float>(window.getSize().y)
        ));
        window.setView(menuView);
        window.draw(backButton);
        window.draw(volumeText);
        window.draw(volumeBar);
        window.draw(volumeSlider);
        window.draw(minesText);
        window.draw(minesBar);
        window.draw(minesSlider);
        window.draw(widthText);
        window.draw(widthBar);
        window.draw(widthSlider);
        window.draw(heightText);
        window.draw(heightBar);
        window.draw(heightSlider);
        window.draw(safeCellText);
        window.draw(safeCellCheckbox);
        if (showFirstSafeCell) {
            window.draw(safeCellCheckmark);
        }
        window.setView(currentView);
    }

    bool isBackButtonClicked(sf::Vector2f mousePos) const {
        sf::Vector2f adjustedMousePos((mousePos.x - menuOffset.x) / menuScale,
            (mousePos.y - menuOffset.y) / menuScale);
        sf::FloatRect bounds = backButton.getGlobalBounds();
        bounds.left = 400.0f - backButton.getLocalBounds().width / 2.0f;
        bounds.top = 650.0f;
        bounds.width = backButton.getLocalBounds().width;
        bounds.height = backButton.getLocalBounds().height;
        return bounds.contains(adjustedMousePos);
    }

    bool isSafeCellCheckboxClicked(sf::Vector2f mousePos) const {
        sf::Vector2f adjustedMousePos((mousePos.x - menuOffset.x) / menuScale,
            (mousePos.y - menuOffset.y) / menuScale);
        sf::FloatRect bounds = safeCellCheckbox.getGlobalBounds();
        return bounds.contains(adjustedMousePos);
    }

    void handleMousePress(sf::Vector2f mousePos) {
        sf::Vector2f adjustedMousePos(
            (mousePos.x - menuOffset.x) / menuScale,
            (mousePos.y - menuOffset.y) / menuScale
        );
        if (!sliderGrabbed) {
            if (volumeSlider.getGlobalBounds().contains(adjustedMousePos)) {
                activeSlider = SliderType::Volume;
                sliderGrabbed = true;
            }
            else if (minesSlider.getGlobalBounds().contains(adjustedMousePos)) {
                activeSlider = SliderType::Mines;
                sliderGrabbed = true;
            }
            else if (widthSlider.getGlobalBounds().contains(adjustedMousePos)) {
                activeSlider = SliderType::Width;
                sliderGrabbed = true;
            }
            else if (heightSlider.getGlobalBounds().contains(adjustedMousePos)) {
                activeSlider = SliderType::Height;
                sliderGrabbed = true;
            }
        }
        else {
            activeSlider = SliderType::None;
            sliderGrabbed = false;
        }
    }

    void handleMouseMove(sf::Vector2f mousePos, sf::Music& music, sf::Sound& explosionSound) {
        if (sliderGrabbed) {
            float adjustedX = (mousePos.x - menuOffset.x) / menuScale;

            if (activeSlider == SliderType::Volume && adjustedX >= 350.0f && adjustedX <= 650.0f) {
                volumeSlider.setPosition(adjustedX, 170.0f);
                globalVolume = ((adjustedX - 350.0f) / 300.0f) * 100.0f;
                music.setVolume(globalVolume);
                explosionSound.setVolume(globalVolume);
            }
            else if (activeSlider == SliderType::Mines && adjustedX >= 350.0f && adjustedX <= 650.0f) {
                minesSlider.setPosition(adjustedX, 270.0f);
                Mines = std::max(1, static_cast<int>(((adjustedX - 350.0f) / 300.0f) * (Width * Height * 0.5f)));
                minesText.setString("Mines: " + std::to_string(Mines));
            }
            else if (activeSlider == SliderType::Width && adjustedX >= 350.0f && adjustedX <= 650.0f) {
                widthSlider.setPosition(adjustedX, 370.0f);
                Width = std::max(5, std::min(static_cast<int>(((adjustedX - 350.0f) / 300.0f) * (maxWindowWidth / 40.0f)), static_cast<int>(maxWindowWidth / 40.0f)));
                widthText.setString("Width: " + std::to_string(Width));
            }
            else if (activeSlider == SliderType::Height && adjustedX >= 350.0f && adjustedX <= 650.0f) {
                heightSlider.setPosition(adjustedX, 470.0f);
                Height = std::max(5, std::min(static_cast<int>(((adjustedX - 350.0f) / 300.0f) * (maxWindowHeight / 40.0f)), static_cast<int>(maxWindowHeight / 40.0f)));
                heightText.setString("Height: " + std::to_string(Height));
            }
        }
    }

    const sf::Vector2f& getMenuOffset() const { return menuOffset; }
    float getMenuScale() const { return menuScale; }
};

void drawCell(sf::RenderWindow& window, const Cell& cell, int x, int y,
    const sf::Sprite& Minesprite, const sf::Sprite& FlagSprite,
    const sf::Sprite& SafeCellSprite, float cellSize, float offsetX, float offsetY) {
    sf::RectangleShape rectangle(sf::Vector2f(cellSize, cellSize));
    rectangle.setPosition(offsetX + x * cellSize, offsetY + y * cellSize);
    rectangle.setOutlineColor(sf::Color::Black);
    rectangle.setOutlineThickness(1.0f);
    rectangle.setFillColor(cell.isRevealed() ? sf::Color(255, 255, 255, 100) : sf::Color(200, 200, 200, 100));
    window.draw(rectangle);

    if (cell.isFirstSafe() && !cell.isRevealed()) {
        sf::Sprite temp = SafeCellSprite;
        temp.setPosition(offsetX + x * cellSize, offsetY + y * cellSize);
        temp.setScale(
            cellSize / temp.getTexture()->getSize().x,
            cellSize / temp.getTexture()->getSize().y
        );
        window.draw(temp);
    }
    else if (cell.isRevealed() && cell.isMine()) {
        sf::Sprite temp = Minesprite;
        temp.setPosition(offsetX + x * cellSize, offsetY + y * cellSize);
        temp.setScale(
            cellSize / temp.getTexture()->getSize().x,
            cellSize / temp.getTexture()->getSize().y
        );
        window.draw(temp);
    }
    else if (cell.isRevealed() && cell.NMines > 0) {
        static sf::Font font;
        static bool fontLoaded = font.loadFromFile(getResourcePath("fonts\\Gamepixies.ttf"));

        sf::Text numberText;
        numberText.setFont(font);
        numberText.setString(std::to_string(cell.NMines));
        numberText.setCharacterSize(static_cast<unsigned int>(cellSize * 0.7f));
        numberText.setOutlineThickness(2.0f);
        numberText.setOutlineColor(sf::Color(0, 0, 0, 200));

        switch (cell.NMines) {
        case 1: numberText.setFillColor(sf::Color(100, 149, 237)); break;
        case 2: numberText.setFillColor(sf::Color(102, 205, 170)); break;
        case 3: numberText.setFillColor(sf::Color(205, 92, 92)); break;
        case 4: numberText.setFillColor(sf::Color(138, 43, 226)); break;
        case 5: numberText.setFillColor(sf::Color(210, 105, 30)); break;
        case 6: numberText.setFillColor(sf::Color(0, 139, 139)); break;
        case 7: numberText.setFillColor(sf::Color(47, 79, 79)); break;
        case 8: numberText.setFillColor(sf::Color(139, 0, 139)); break;
        default: numberText.setFillColor(sf::Color::Black);
        }

        sf::FloatRect textRect = numberText.getLocalBounds();
        numberText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        numberText.setPosition(offsetX + x * cellSize + cellSize / 2.0f, offsetY + y * cellSize + cellSize / 2.0f);

        window.draw(numberText);
    }
    else if (cell.isFlagged()) {
        sf::Sprite temp = FlagSprite;
        temp.setPosition(offsetX + x * cellSize, offsetY + y * cellSize);
        temp.setScale(
            cellSize / temp.getTexture()->getSize().x,
            cellSize / temp.getTexture()->getSize().y
        );
        window.draw(temp);
    }
}
void drawRestartButton(sf::RenderWindow& window, const sf::Sprite& restartButtonSprite,
    bool isVictory, float scale, float offsetX, float offsetY) {
    sf::Sprite temp = restartButtonSprite;
    temp.setScale(200.0f * scale / temp.getTexture()->getSize().x, 200.0f * scale / temp.getTexture()->getSize().y);
    temp.setPosition(offsetX + (static_cast<float>(Width) * 40.0f * scale) / 2.0f - 100.0f * scale,
        offsetY + (static_cast<float>(Height) * 40.0f * scale) / 2.0f - 80.0f * scale);
    window.draw(temp);
    if (isVictory) {
    }
}
class LoadingScreen {
private:
    sf::RectangleShape overlay;
    sf::CircleShape spinnerDots[8];
    sf::Clock spinnerClock;
    sf::Text loadingText;
    sf::Font& font;
    float fadeAlpha = 255.0f;
    bool isActive = false;

public:
    LoadingScreen(sf::Font& gameFont) : font(gameFont) {
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        loadingText.setFont(font);
        loadingText.setString("Loading...");
        loadingText.setCharacterSize(50);
        loadingText.setFillColor(sf::Color::White);
        loadingText.setOutlineColor(sf::Color::Black);
        loadingText.setOutlineThickness(2.0f);

        for (int i = 0; i < 8; ++i) {
            spinnerDots[i].setRadius(10.0f);
            spinnerDots[i].setFillColor(sf::Color(255, 255, 255, 100 + i * 20));
            spinnerDots[i].setOrigin(10.0f, 10.0f);
        }
    }

    void activate() {
        isActive = true;
        spinnerClock.restart();
    }

    void deactivate() {
        isActive = false;
    }

    void update(float deltaTime) {
        if (!isActive) return;

        float time = spinnerClock.getElapsedTime().asSeconds();
        float angleStep = 360.0f / 8.0f;
        for (int i = 0; i < 8; ++i) {
            float angle = time * 180.0f + i * angleStep;
            float alpha = 100 + (i * 20) + (sin(time * 3.0f + i * 0.5f) * 50.0f);
            spinnerDots[i].setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha)));
        }
    }

    void draw(sf::RenderWindow& window) {
        if (!isActive) return;

        sf::Vector2f windowSize = static_cast<sf::Vector2f>(window.getSize());

        overlay.setSize(windowSize);
        window.draw(overlay);

        loadingText.setOrigin(loadingText.getLocalBounds().width / 2.0f,
            loadingText.getLocalBounds().height / 2.0f);
        loadingText.setPosition(windowSize.x / 2.0f, windowSize.y / 2.0f + 50.0f);
        window.draw(loadingText);

        float radius = 40.0f;
        for (int i = 0; i < 8; ++i) {
            float angle = i * (360.0f / 8.0f) + spinnerClock.getElapsedTime().asSeconds() * 180.0f;
            spinnerDots[i].setPosition(
                windowSize.x / 2.0f + radius * cos(angle * 3.14159f / 180.0f),
                windowSize.y / 2.0f + radius * sin(angle * 3.14159f / 180.0f)
            );
            window.draw(spinnerDots[i]);
        }
    }

    bool getIsActive() const { return isActive; }
};
class ScoreAnimation {
private:
    sf::RectangleShape background;
    sf::Text scoreText;
    sf::Font& font;

    int targetScore = 0;
    int currentDisplayScore = 0;
    int step = 0;

    sf::Clock animationClock;
    float animationDuration = 3.0f;
    const float frameTime = 1.0f / 60.0f;
    float nextUpdateTime = 0.0f;

    bool active = false;

public:
    ScoreAnimation(sf::Font& gameFont) : font(gameFont) {
        background.setFillColor(sf::Color(0, 0, 0, 180));
        scoreText.setFont(font);
        scoreText.setCharacterSize(120);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setOutlineColor(sf::Color::Black);
        scoreText.setOutlineThickness(4.f);
    }

    void start(int score) {
        targetScore = score;
        currentDisplayScore = 0;
        int totalFrames = static_cast<int>(animationDuration / frameTime);
        step = std::max(1, static_cast<int>(std::round(static_cast<float>(targetScore) / totalFrames)));

        active = true;
        animationClock.restart();
        nextUpdateTime = frameTime;
        updateText();
    }

    void update(float deltaTime) {
        if (!active) return;

        float elapsed = animationClock.getElapsedTime().asSeconds();
        if (elapsed >= animationDuration) {
            currentDisplayScore = targetScore;
            active = false;
            updateText();
            return;
        }

        if (elapsed >= nextUpdateTime) {
            int framesPassed = static_cast<int>(elapsed / frameTime);
            int expectedScore = framesPassed * step;
            currentDisplayScore = std::min(expectedScore, targetScore);
            updateText();
            nextUpdateTime = (framesPassed + 1) * frameTime;
        }
    }

    void updateText() {
        std::ostringstream oss;
        oss << std::setw(4) << std::setfill('0') << currentDisplayScore;
        scoreText.setString(oss.str());

        sf::FloatRect textRect = scoreText.getLocalBounds();
        scoreText.setOrigin(textRect.left + textRect.width / 2.0f,
            textRect.top + textRect.height / 2.0f);
    }

    void draw(sf::RenderWindow& window) {
        if (!active && currentDisplayScore == 0) return;

        if (active) {
            background.setSize(sf::Vector2f(window.getSize()));
            window.draw(background);

            scoreText.setPosition(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
            window.draw(scoreText);
        }
    }

    bool isActive() const { return active; }
    bool isComplete() const { return !active; }
};
class GameMenu {
private:
    ScoreAnimation scoreAnimation;
    bool wheelClicked = false;
    bool detectorClicked = false;
    bool bootClicked = false;
    bool bootActive = false;
    bool bootProtectionUsed = false;
    bool wheelUsed = false;
    bool detectorUsed = false;
    bool bootUsed = false;
    int score = 0;

    sf::Sprite mainButton;
    sf::Sprite wheelButton;
    sf::Sprite detectorButton;
    sf::Sprite bootButton;
    bool isExpanded = false;
    float animationProgress = 0.f;
    bool isAnimating = false;
    bool targetExpandedState = false;

    sf::Texture& mainButtonTex;
    sf::Texture& wheelButtonTex;
    sf::Texture& detectorButtonTex;
    sf::Texture& bootButtonTex;

    sf::RectangleShape scoreBar;
    sf::Text scoreText;
    sf::Font& font;

    sf::Vector2f mainButtonPos;
    sf::Vector2f wheelButtonStartPos;
    sf::Vector2f wheelButtonTargetPos;
    sf::Vector2f detectorButtonStartPos;
    sf::Vector2f detectorButtonTargetPos;
    sf::Vector2f bootButtonStartPos;
    sf::Vector2f bootButtonTargetPos;

    sf::Vector2f lerp(const sf::Vector2f& a, const sf::Vector2f& b, float t) {
        float easedT = t < 0.5f ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
        return a + (b - a) * easedT;
    }

    float smoothStep(float t) {
        return t * t * (3.f - 2.f * t);
    }

    void startExpandAnimation() {
        targetExpandedState = true;
        isAnimating = true;
        animationProgress = 0.f;
    }

    void startCollapseAnimation() {
        targetExpandedState = false;
        isAnimating = true;
        animationProgress = 0.f;
    }

public:
    GameMenu(sf::Font& gameFont, sf::Texture& mainTex, sf::Texture& wheelTex,
        sf::Texture& detectorTex, sf::Texture& bootTex)
        : font(gameFont), mainButtonTex(mainTex), wheelButtonTex(wheelTex),
        detectorButtonTex(detectorTex), bootButtonTex(bootTex),
        scoreAnimation(gameFont) {

        mainButton.setTexture(mainButtonTex);
        wheelButton.setTexture(wheelButtonTex);
        detectorButton.setTexture(detectorButtonTex);
        bootButton.setTexture(bootButtonTex);

        mainButton.setColor(sf::Color(255, 255, 255, 200));
        mainButton.setScale(0.25f, 0.25f);
        wheelButton.setScale(0.125f, 0.125f);
        detectorButton.setScale(0.25f, 0.25f);
        bootButton.setScale(0.25f, 0.25f);

        scoreBar.setSize(sf::Vector2f(200.f, 30.f));
        scoreBar.setFillColor(sf::Color(100, 100, 100, 180));
        scoreBar.setOutlineThickness(2.f);
        scoreBar.setOutlineColor(sf::Color(50, 50, 50, 200));

        scoreText.setFont(font);
        scoreText.setCharacterSize(24);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setString("Score: 0");
    }

    void update(const sf::Vector2f& mousePos, const sf::Event& event, float deltaTime) {
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            if (mainButton.getGlobalBounds().contains(mousePos) && !isAnimating) {
                if (isExpanded) {
                    startCollapseAnimation();
                }
                else {
                    startExpandAnimation();
                }
            }
            else if (isExpanded && !isAnimating) {
                if (wheelButton.getGlobalBounds().contains(mousePos) && !wheelUsed) {
                    wheelClicked = true;
                    wheelUsed = true;
                    startCollapseAnimation();
                }
                else if (detectorButton.getGlobalBounds().contains(mousePos) && !detectorUsed) {
                    detectorClicked = true;
                    detectorUsed = true;
                    startCollapseAnimation();
                }
                else if (bootButton.getGlobalBounds().contains(mousePos) && !bootUsed) {
                    bootClicked = true;
                    bootUsed = true;
                    bootActive = true;
                    bootProtectionUsed = false;
                    startCollapseAnimation();
                }
            }
        }

        if (isAnimating) {
            animationProgress += deltaTime * 5.f;
            if (animationProgress >= 1.f) {
                animationProgress = 1.f;
                isAnimating = false;
                isExpanded = targetExpandedState;
            }

            if (targetExpandedState) {
                wheelButton.setPosition(lerp(wheelButtonStartPos, wheelButtonTargetPos, smoothStep(animationProgress)));
                detectorButton.setPosition(lerp(detectorButtonStartPos, detectorButtonTargetPos, smoothStep(animationProgress)));
                bootButton.setPosition(lerp(bootButtonStartPos, bootButtonTargetPos, smoothStep(animationProgress)));
            }
            else {
                wheelButton.setPosition(lerp(wheelButtonTargetPos, wheelButtonStartPos, smoothStep(animationProgress)));
                detectorButton.setPosition(lerp(detectorButtonTargetPos, detectorButtonStartPos, smoothStep(animationProgress)));
                bootButton.setPosition(lerp(bootButtonTargetPos, bootButtonStartPos, smoothStep(animationProgress)));
            }
        }

        scoreAnimation.update(deltaTime);
    }

    void updatePositions(const sf::Vector2f& windowSize) {
        mainButtonPos = sf::Vector2f(
            windowSize.x / 2 - mainButton.getGlobalBounds().width / 2,
            windowSize.y - mainButton.getGlobalBounds().height - 20
        );
        mainButton.setPosition(mainButtonPos);

        wheelButtonStartPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width / 2 - wheelButton.getGlobalBounds().width / 2,
            mainButtonPos.y + mainButton.getGlobalBounds().height / 2 - wheelButton.getGlobalBounds().height / 2
        );
        detectorButtonStartPos = wheelButtonStartPos;
        bootButtonStartPos = wheelButtonStartPos;

        wheelButtonTargetPos = sf::Vector2f(
            mainButtonPos.x - wheelButton.getGlobalBounds().width - 10,
            mainButtonPos.y - 10
        );
        detectorButtonTargetPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width / 2 - detectorButton.getGlobalBounds().width / 2,
            mainButtonPos.y - detectorButton.getGlobalBounds().height - 20
        );
        bootButtonTargetPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width + 10,
            mainButtonPos.y - 10
        );

        if (!isAnimating) {
            if (isExpanded) {
                wheelButton.setPosition(wheelButtonTargetPos);
                detectorButton.setPosition(detectorButtonTargetPos);
                bootButton.setPosition(bootButtonTargetPos);
            }
            else {
                wheelButton.setPosition(wheelButtonStartPos);
                detectorButton.setPosition(detectorButtonStartPos);
                bootButton.setPosition(bootButtonStartPos);
            }
        }
    }

    void updateScore(int width, int height, int mines) {
        int maxMines = (width * height) / 2;
        float minesRatio = static_cast<float>(mines) / maxMines;
        float coefficient = 1.0f + minesRatio;
        score += static_cast<int>((width * height) * coefficient);
        scoreText.setString("Score: " + std::to_string(score));
        scoreAnimation.start(score);
    }

    void draw(sf::RenderWindow& window) {
        sf::Vector2f windowSize = static_cast<sf::Vector2f>(window.getSize());
        updatePositions(windowSize);

        scoreBar.setPosition(20.f, 20.f);
        window.draw(scoreBar);
        scoreText.setPosition(30.f, 20.f);
        window.draw(scoreText);

        window.draw(mainButton);

        if (isExpanded || isAnimating) {
            if (wheelButton.getPosition() != wheelButtonStartPos) {
                sf::RectangleShape wheelBg(sf::Vector2f(
                    wheelButton.getGlobalBounds().width + 20,
                    wheelButton.getGlobalBounds().height + 20
                ));
                wheelBg.setPosition(wheelButton.getPosition().x - 10, wheelButton.getPosition().y - 10);
                wheelBg.setFillColor(sf::Color(80, 80, 80, 220));
                wheelBg.setOutlineThickness(2.f);
                wheelBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(wheelBg);
                window.draw(wheelButton);
            }

            if (detectorButton.getPosition() != detectorButtonStartPos) {
                sf::RectangleShape detectorBg(sf::Vector2f(
                    detectorButton.getGlobalBounds().width + 20,
                    detectorButton.getGlobalBounds().height + 20
                ));
                detectorBg.setPosition(detectorButton.getPosition().x - 10, detectorButton.getPosition().y - 10);
                detectorBg.setFillColor(sf::Color(80, 80, 80, 220));
                detectorBg.setOutlineThickness(2.f);
                detectorBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(detectorBg);
                window.draw(detectorButton);
            }

            if (bootButton.getPosition() != bootButtonStartPos) {
                sf::RectangleShape bootBg(sf::Vector2f(
                    bootButton.getGlobalBounds().width + 20,
                    bootButton.getGlobalBounds().height + 20
                ));
                bootBg.setPosition(bootButton.getPosition().x - 10, bootButton.getPosition().y - 10);
                bootBg.setFillColor(sf::Color(80, 80, 80, 220));
                bootBg.setOutlineThickness(2.f);
                bootBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(bootBg);
                window.draw(bootButton);

                if (bootActive && !bootProtectionUsed) {
                    sf::CircleShape indicator(5.f);
                    indicator.setFillColor(sf::Color::Green);
                    indicator.setPosition(
                        bootButton.getPosition().x + bootButton.getGlobalBounds().width - 5.f,
                        bootButton.getPosition().y + 5.f
                    );
                    window.draw(indicator);
                }
            }

            if (wheelUsed && wheelButton.getPosition() != wheelButtonStartPos) {
                sf::RectangleShape overlay(wheelButton.getGlobalBounds().getSize());
                overlay.setPosition(wheelButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }

            if (detectorUsed && detectorButton.getPosition() != detectorButtonStartPos) {
                sf::RectangleShape overlay(detectorButton.getGlobalBounds().getSize());
                overlay.setPosition(detectorButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }

            if (bootUsed && bootButton.getPosition() != bootButtonStartPos) {
                sf::RectangleShape overlay(bootButton.getGlobalBounds().getSize());
                overlay.setPosition(bootButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }
        }

        scoreAnimation.draw(window);
    }

    bool getWheelClicked() {
        if (wheelClicked) {
            wheelClicked = false;
            return true;
        }
        return false;
    }

    bool getDetectorClicked() {
        if (detectorClicked) {
            detectorClicked = false;
            return true;
        }
        return false;
    }

    bool getBootClicked() {
        if (bootClicked) {
            bootClicked = false;
            return true;
        }
        return false;
    }

    bool isBootActive() const { return bootActive && !bootProtectionUsed; }

    void markBootUsed() {
        bootProtectionUsed = true;
        bootActive = false;
    }

    void resetItems() {
        wheelUsed = false;
        detectorUsed = false;
        bootUsed = false;
        bootActive = false;
        bootProtectionUsed = false;
    }

    bool shouldShowRestartButton() const {
        return scoreAnimation.isComplete();
    }
};

int calculateScore(int width, int height, int mines) {
    int maxMines = (width * height) / 2;
    float minesRatio = static_cast<float>(mines) / maxMines;
    float coefficient = 1.0f + minesRatio;
    return static_cast<int>((width * height) * coefficient);
}
int main() {
    sf::RenderWindow window(sf::VideoMode(1920, 1080), "Minesweeper", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    // Загрузка шрифта
    sf::Font gameFont;
    if (!gameFont.loadFromFile(getResourcePath("fonts\\Gamepixies.ttf"))) {
        return -1;
    }

    // Загрузка текстур для предметов
    sf::Texture equipButtonTex, wheelTex, detectorTex, bootTex;
    if (!equipButtonTex.loadFromFile(getResourcePath("images(textures,sprites)\\equip_button.png")) ||
        !wheelTex.loadFromFile(getResourcePath("images(textures,sprites)\\wheel_spin.png")) ||
        !detectorTex.loadFromFile(getResourcePath("images(textures,sprites)\\metal_detector.png")) ||
        !bootTex.loadFromFile(getResourcePath("images(textures,sprites)\\boot_texture.png"))) {
        return -1;
    }

    // Создание спрайтов для предметов
    sf::Sprite mainButton(equipButtonTex);
    sf::Sprite wheelButton(wheelTex);
    sf::Sprite detectorButton(detectorTex);
    sf::Sprite bootButton(bootTex);

    // Настройка спрайтов
    mainButton.setColor(sf::Color(255, 255, 255, 200));
    mainButton.setScale(0.25f, 0.25f);
    wheelButton.setScale(0.125f, 0.125f);
    detectorButton.setScale(0.25f, 0.25f);
    bootButton.setScale(0.25f, 0.25f);

    // Создание элементов интерфейса
    sf::RectangleShape scoreBar(sf::Vector2f(200.f, 30.f));
    scoreBar.setFillColor(sf::Color(100, 100, 100, 180));
    scoreBar.setOutlineThickness(2.f);
    scoreBar.setOutlineColor(sf::Color(50, 50, 50, 200));

    sf::Text scoreText("Score: 0", gameFont, 24);
    scoreText.setFillColor(sf::Color::White);

    // Переменные для анимации score
    int targetScore = 0;
    int currentDisplayScore = 0;
    int step = 0;
    sf::Clock animationClock;
    float animationDuration = 3.0f;
    const float frameTime = 1.0f / 60.0f;
    float nextUpdateTime = 0.0f;
    bool scoreAnimationActive = false;
    sf::RectangleShape scoreBackground;
    scoreBackground.setFillColor(sf::Color(0, 0, 0, 180));

    // Переменные для предметов
    bool wheelClicked = false;
    bool detectorClicked = false;
    bool bootClicked = false;
    bool bootActive = false;
    bool bootProtectionUsed = false;
    bool wheelUsed = false;
    bool detectorUsed = false;
    bool bootUsed = false;
    int score = 0;

    // Переменные для анимации меню
    bool isExpanded = false;
    float animationProgress = 0.f;
    bool isAnimating = false;
    bool targetExpandedState = false;
    sf::Vector2f mainButtonPos;
    sf::Vector2f wheelButtonStartPos, wheelButtonTargetPos;
    sf::Vector2f detectorButtonStartPos, detectorButtonTargetPos;
    sf::Vector2f bootButtonStartPos, bootButtonTargetPos;

    Minesweeper game;
    ResourceManager resources;
    MainMenu mainMenu;
    SettingsMenu settingsMenu;
    AnimatedBackground background;
    GameBackground gameBackground;
    FadeEffect fadeEffect;
    LoadingScreen loadingScreen(gameFont);
    MusicController musicController;

    if (!resources.loadEssentialResources()) return -1;
    if (!resources.loadMenuMusic()) return -1;
    musicController.play(resources.getMenuMusic());

    bool inMenu = true;
    bool inSettings = false;
    bool gameOver = false;
    bool isVictory = false;
    bool isLoading = false;
    bool loadingComplete = false;
    bool isFullscreen = false;
    bool gameStarted = false;
    bool levelPassedSoundPlayed = false;
    float cellSize = 40.0f;
    int loadStep = 0;

    fadeEffect.startFadeIn();
    game.reset();
    sf::Clock deltaClock;
    sf::Clock restartDelayClock;
    bool restartDelay = false;

    // Функция для обновления позиций элементов меню
    auto updateMenuPositions = [&](const sf::Vector2f& windowSize) {
        mainButtonPos = sf::Vector2f(
            windowSize.x / 2 - mainButton.getGlobalBounds().width / 2,
            windowSize.y - mainButton.getGlobalBounds().height - 20
        );
        mainButton.setPosition(mainButtonPos);

        wheelButtonStartPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width / 2 - wheelButton.getGlobalBounds().width / 2,
            mainButtonPos.y + mainButton.getGlobalBounds().height / 2 - wheelButton.getGlobalBounds().height / 2
        );
        detectorButtonStartPos = wheelButtonStartPos;
        bootButtonStartPos = wheelButtonStartPos;

        wheelButtonTargetPos = sf::Vector2f(
            mainButtonPos.x - wheelButton.getGlobalBounds().width - 10,
            mainButtonPos.y - 10
        );
        detectorButtonTargetPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width / 2 - detectorButton.getGlobalBounds().width / 2,
            mainButtonPos.y - detectorButton.getGlobalBounds().height - 20
        );
        bootButtonTargetPos = sf::Vector2f(
            mainButtonPos.x + mainButton.getGlobalBounds().width + 10,
            mainButtonPos.y - 10
        );

        if (!isAnimating) {
            if (isExpanded) {
                wheelButton.setPosition(wheelButtonTargetPos);
                detectorButton.setPosition(detectorButtonTargetPos);
                bootButton.setPosition(bootButtonTargetPos);
            }
            else {
                wheelButton.setPosition(wheelButtonStartPos);
                detectorButton.setPosition(detectorButtonStartPos);
                bootButton.setPosition(bootButtonStartPos);
            }
        }
        };

    // Функция для обновления счета
    auto updateScore = [&](int width, int height, int mines) {
        int maxMines = (width * height) / 2;
        float minesRatio = static_cast<float>(mines) / maxMines;
        float coefficient = 1.0f + minesRatio;
        score += static_cast<int>((width * height) * coefficient);
        scoreText.setString("Score: " + std::to_string(score));

        // Запуск анимации счета
        targetScore = score;
        currentDisplayScore = 0;
        int totalFrames = static_cast<int>(animationDuration / frameTime);
        step = std::max(1, static_cast<int>(std::round(static_cast<float>(targetScore) / totalFrames)));
        scoreAnimationActive = true;
        animationClock.restart();
        nextUpdateTime = frameTime;
        };

    // Функция для отрисовки меню
    auto drawGameMenu = [&](sf::RenderWindow& window) {
        sf::Vector2f windowSize = static_cast<sf::Vector2f>(window.getSize());
        updateMenuPositions(windowSize);

        scoreBar.setPosition(20.f, 20.f);
        window.draw(scoreBar);
        scoreText.setPosition(30.f, 20.f);
        window.draw(scoreText);

        window.draw(mainButton);

        if (isExpanded || isAnimating) {
            if (wheelButton.getPosition() != wheelButtonStartPos) {
                sf::RectangleShape wheelBg(sf::Vector2f(
                    wheelButton.getGlobalBounds().width + 20,
                    wheelButton.getGlobalBounds().height + 20
                ));
                wheelBg.setPosition(wheelButton.getPosition().x - 10, wheelButton.getPosition().y - 10);
                wheelBg.setFillColor(sf::Color(80, 80, 80, 220));
                wheelBg.setOutlineThickness(2.f);
                wheelBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(wheelBg);
                window.draw(wheelButton);
            }

            if (detectorButton.getPosition() != detectorButtonStartPos) {
                sf::RectangleShape detectorBg(sf::Vector2f(
                    detectorButton.getGlobalBounds().width + 20,
                    detectorButton.getGlobalBounds().height + 20
                ));
                detectorBg.setPosition(detectorButton.getPosition().x - 10, detectorButton.getPosition().y - 10);
                detectorBg.setFillColor(sf::Color(80, 80, 80, 220));
                detectorBg.setOutlineThickness(2.f);
                detectorBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(detectorBg);
                window.draw(detectorButton);
            }

            if (bootButton.getPosition() != bootButtonStartPos) {
                sf::RectangleShape bootBg(sf::Vector2f(
                    bootButton.getGlobalBounds().width + 20,
                    bootButton.getGlobalBounds().height + 20
                ));
                bootBg.setPosition(bootButton.getPosition().x - 10, bootButton.getPosition().y - 10);
                bootBg.setFillColor(sf::Color(80, 80, 80, 220));
                bootBg.setOutlineThickness(2.f);
                bootBg.setOutlineColor(sf::Color(30, 30, 30, 240));
                window.draw(bootBg);
                window.draw(bootButton);

                if (bootActive && !bootProtectionUsed) {
                    sf::CircleShape indicator(5.f);
                    indicator.setFillColor(sf::Color::Green);
                    indicator.setPosition(
                        bootButton.getPosition().x + bootButton.getGlobalBounds().width - 5.f,
                        bootButton.getPosition().y + 5.f
                    );
                    window.draw(indicator);
                }
            }

            if (wheelUsed && wheelButton.getPosition() != wheelButtonStartPos) {
                sf::RectangleShape overlay(wheelButton.getGlobalBounds().getSize());
                overlay.setPosition(wheelButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }

            if (detectorUsed && detectorButton.getPosition() != detectorButtonStartPos) {
                sf::RectangleShape overlay(detectorButton.getGlobalBounds().getSize());
                overlay.setPosition(detectorButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }

            if (bootUsed && bootButton.getPosition() != bootButtonStartPos) {
                sf::RectangleShape overlay(bootButton.getGlobalBounds().getSize());
                overlay.setPosition(bootButton.getPosition());
                overlay.setFillColor(sf::Color(100, 100, 100, 180));
                window.draw(overlay);
            }
        }

        // Отрисовка анимации счета
        if (scoreAnimationActive) {
            float elapsed = animationClock.getElapsedTime().asSeconds();
            if (elapsed >= animationDuration) {
                currentDisplayScore = targetScore;
                scoreAnimationActive = false;
            }
            else if (elapsed >= nextUpdateTime) {
                int framesPassed = static_cast<int>(elapsed / frameTime);
                int expectedScore = framesPassed * step;
                currentDisplayScore = std::min(expectedScore, targetScore);
                nextUpdateTime = (framesPassed + 1) * frameTime;
            }

            std::ostringstream oss;
            oss << std::setw(4) << std::setfill('0') << currentDisplayScore;
            sf::Text animScoreText(oss.str(), gameFont, 120);
            animScoreText.setFillColor(sf::Color::White);
            animScoreText.setOutlineColor(sf::Color::Black);
            animScoreText.setOutlineThickness(4.f);

            sf::FloatRect textRect = animScoreText.getLocalBounds();
            animScoreText.setOrigin(textRect.left + textRect.width / 2.0f,
                textRect.top + textRect.height / 2.0f);
            animScoreText.setPosition(windowSize.x / 2.0f, windowSize.y / 2.0f);

            scoreBackground.setSize(windowSize);
            window.draw(scoreBackground);
            window.draw(animScoreText);
        }
        };

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));

        musicController.update(deltaTime);
        fadeEffect.update(deltaTime);
        loadingScreen.update(deltaTime);

        // Обновление анимации меню
        if (isAnimating) {
            animationProgress += deltaTime * 5.f;
            if (animationProgress >= 1.f) {
                animationProgress = 1.f;
                isAnimating = false;
                isExpanded = targetExpandedState;
            }

            auto lerp = [](const sf::Vector2f& a, const sf::Vector2f& b, float t) {
                float easedT = t < 0.5f ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
                return a + (b - a) * easedT;
                };

            if (targetExpandedState) {
                wheelButton.setPosition(lerp(wheelButtonStartPos, wheelButtonTargetPos, animationProgress));
                detectorButton.setPosition(lerp(detectorButtonStartPos, detectorButtonTargetPos, animationProgress));
                bootButton.setPosition(lerp(bootButtonStartPos, bootButtonTargetPos, animationProgress));
            }
            else {
                wheelButton.setPosition(lerp(wheelButtonTargetPos, wheelButtonStartPos, animationProgress));
                detectorButton.setPosition(lerp(detectorButtonTargetPos, detectorButtonStartPos, animationProgress));
                bootButton.setPosition(lerp(bootButtonTargetPos, bootButtonStartPos, animationProgress));
            }
        }

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    if (inSettings) {
                        inSettings = false;
                    }
                    else if (!inMenu && !isLoading) {
                        inMenu = true;
                        musicController.switchTo(resources.getMenuMusic());
                        if (gameStarted) {
                            resources.unloadGameResources();
                            background.cleanup();
                            gameStarted = false;
                            isLoading = false;
                            loadingComplete = false;
                            loadStep = 0;
                        }
                    }
                }
                else if (event.key.code == sf::Keyboard::F) {
                    isFullscreen = !isFullscreen;
                    window.create(
                        isFullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode(1920, 1080),
                        "Minesweeper",
                        isFullscreen ? sf::Style::Fullscreen : sf::Style::Default
                    );
                }
            }

            if (!inMenu && !inSettings && event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left) {

                // Обработка клика по главной кнопке меню
                if (mainButton.getGlobalBounds().contains(mousePos) && !isAnimating) {
                    if (isExpanded) {
                        targetExpandedState = false;
                        isAnimating = true;
                        animationProgress = 0.f;
                    }
                    else {
                        targetExpandedState = true;
                        isAnimating = true;
                        animationProgress = 0.f;
                    }
                }
                // Обработка кликов по предметам
                else if (isExpanded && !isAnimating) {
                    if (wheelButton.getGlobalBounds().contains(mousePos) && !wheelUsed) {
                        wheelClicked = true;
                        wheelUsed = true;
                        targetExpandedState = false;
                        isAnimating = true;
                        animationProgress = 0.f;
                    }
                    else if (detectorButton.getGlobalBounds().contains(mousePos) && !detectorUsed) {
                        detectorClicked = true;
                        detectorUsed = true;
                        targetExpandedState = false;
                        isAnimating = true;
                        animationProgress = 0.f;
                    }
                    else if (bootButton.getGlobalBounds().contains(mousePos) && !bootUsed) {
                        bootClicked = true;
                        bootUsed = true;
                        bootActive = true;
                        bootProtectionUsed = false;
                        targetExpandedState = false;
                        isAnimating = true;
                        animationProgress = 0.f;
                    }
                }
            }

            if (!isLoading && !inMenu && event.type == sf::Event::MouseButtonPressed) {
                if (wheelClicked) {
                    wheelClicked = false;
                    // Логика колеса фортуны
                    sf::Texture wheelTexture;
                    sf::Texture arrowTexture;
                    if (!wheelTexture.loadFromFile(getResourcePath("images(textures,sprites)\\wheel_spin.png")) ||
                        !arrowTexture.loadFromFile(getResourcePath("images(textures,sprites)\\cursor.png"))) {
                        continue;
                    }

                    const std::vector<std::pair<float, int>> sectors = {
                        {342.0f, 2}, {18.0f, 3}, {54.0f, 0}, {90.0f, 1}, {126.0f, 2},
                        {162.0f, 3}, {198.0f, 1}, {234.0f, 2}, {270.0f, 3}, {306.0f, 1}
                    };

                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> sectorDist(0, 9);
                    int selectedSector = sectorDist(gen);
                    float sectorStartAngle = sectors[selectedSector].first;
                    int minesToReveal = sectors[selectedSector].second;
                    float sectorCenterAngle = sectorStartAngle + 18.0f;
                    float targetAngle = 360.0f - sectorCenterAngle;

                    sf::Sprite wheel(wheelTexture);
                    wheel.setOrigin(static_cast<float>(wheelTexture.getSize().x) / 2.0f,
                        static_cast<float>(wheelTexture.getSize().y) / 2.0f);
                    wheel.setPosition(static_cast<float>(window.getSize().x) / 2.0f,
                        static_cast<float>(window.getSize().y) / 2.0f);

                    sf::Sprite arrow(arrowTexture);
                    arrow.setOrigin(static_cast<float>(arrowTexture.getSize().x) / 2.0f,
                        static_cast<float>(arrowTexture.getSize().y) / 2.0f);
                    arrow.setPosition(static_cast<float>(window.getSize().x) / 2.0f,
                        static_cast<float>(window.getSize().y) / 2.0f - 350.0f);
                    arrow.setScale(0.3f, 0.3f);
                    arrow.setRotation(90.0f);

                    float totalRotation = 720.0f + targetAngle;
                    float currentRotation = 0.0f;
                    float baseSpeed = 360.0f;
                    float currentSpeed = baseSpeed;
                    bool spinning = true;
                    bool minesRevealed = false;

                    sf::Clock spinClock;
                    while (window.isOpen()) {
                        sf::Event wheelEvent;
                        while (window.pollEvent(wheelEvent)) {
                            if (wheelEvent.type == sf::Event::Closed) {
                                window.close();
                                return 0;
                            }
                        }

                        if (spinning) {
                            float deltaTime = spinClock.restart().asSeconds();
                            float remainingRotation = totalRotation - currentRotation;

                            if (remainingRotation < 720.0f) {
                                float slowdownFactor = remainingRotation / 720.0f;
                                currentSpeed = baseSpeed * slowdownFactor;
                                currentSpeed = std::max(currentSpeed, 10.0f);
                            }

                            currentRotation += currentSpeed * deltaTime;

                            if (currentRotation >= totalRotation) {
                                currentRotation = totalRotation;
                                spinning = false;

                                if (!minesRevealed && minesToReveal > 0) {
                                    minesRevealed = true;
                                    std::vector<std::pair<int, int>> hiddenMines;
                                    for (int y = 0; y < Height; y++) {
                                        for (int x = 0; x < Width; x++) {
                                            if (game.getCell(x, y).isMine() &&
                                                !game.getCell(x, y).isRevealed() &&
                                                !game.getCell(x, y).isFlagged()) {
                                                hiddenMines.emplace_back(x, y);
                                            }
                                        }
                                    }

                                    std::shuffle(hiddenMines.begin(), hiddenMines.end(), gen);
                                    int revealed = 0;
                                    for (const auto& pos : hiddenMines) {
                                        if (revealed >= minesToReveal) break;
                                        game.revealCell(pos.first, pos.second);
                                        revealed++;
                                    }

                                    bool allClear = true;
                                    for (int y = 0; y < Height; y++) {
                                        for (int x = 0; x < Width; x++) {
                                            const Cell& cell = game.getCell(x, y);
                                            if (!cell.isMine() && !cell.isRevealed()) {
                                                allClear = false;
                                                break;
                                            }
                                        }
                                        if (!allClear) break;
                                    }

                                    if (allClear) {
                                        isVictory = true;
                                        updateScore(Width, Height, Mines);
                                        if (!levelPassedSoundPlayed && resources.areSoundsLoaded()) {
                                            musicController.pause();
                                            resources.getLevelPassedSound().play();
                                            levelPassedSoundPlayed = true;
                                        }
                                    }
                                }
                            }
                            wheel.setRotation(currentRotation);
                        }

                        window.clear();
                        background.update();
                        background.draw(window);

                        float gameWidth = static_cast<float>(Width) * cellSize;
                        float gameHeight = static_cast<float>(Height) * cellSize;
                        float windowWidth = static_cast<float>(window.getSize().x);
                        float windowHeight = static_cast<float>(window.getSize().y);
                        float offsetX = (windowWidth - gameWidth) / 2.0f;
                        float offsetY = (windowHeight - gameHeight) / 2.0f;

                        gameBackground.draw(window, cellSize, offsetX, offsetY);

                        for (int y = 0; y < Height; y++) {
                            for (int x = 0; x < Width; x++) {
                                drawCell(window, game.getCell(x, y), x, y,
                                    sf::Sprite(resources.getTextures().mine),
                                    sf::Sprite(resources.getTextures().flag),
                                    sf::Sprite(resources.getTextures().safeCell),
                                    cellSize, offsetX, offsetY);
                            }
                        }

                        window.draw(wheel);
                        window.draw(arrow);
                        window.display();

                        if (!spinning) {
                            sf::sleep(sf::milliseconds(1000));
                            break;
                        }
                    }
                }
                else if (detectorClicked) {
                    detectorClicked = false;
                    bool hintGiven = false;
                    for (int y = 0; y < Height && !hintGiven; y++) {
                        for (int x = 0; x < Width && !hintGiven; x++) {
                            if (!game.getCell(x, y).isMine() && !game.getCell(x, y).isRevealed()) {
                                game.revealCell(x, y);
                                hintGiven = true;
                            }
                        }
                    }
                }
                else if (bootClicked) {
                    bootClicked = false;
                    bootActive = true;
                    bootProtectionUsed = false;
                }
            }

            if ((gameOver || (isVictory && !scoreAnimationActive)) &&
                event.type == sf::Event::MouseButtonPressed) {
                float gameWidth = static_cast<float>(Width) * cellSize;
                float gameHeight = static_cast<float>(Height) * cellSize;
                float windowWidth = static_cast<float>(window.getSize().x);
                float windowHeight = static_cast<float>(window.getSize().y);
                float offsetX = (windowWidth - gameWidth) / 2.0f;
                float offsetY = (windowHeight - gameHeight) / 2.0f;

                float restartButtonSize = 200.0f;
                float restartButtonX = offsetX + gameWidth / 2 - restartButtonSize / 2;
                float restartButtonY = offsetY + gameHeight / 2 - restartButtonSize / 2;

                sf::FloatRect restartButtonRect(
                    restartButtonX,
                    restartButtonY,
                    restartButtonSize,
                    restartButtonSize
                );
                if (restartButtonRect.contains(mousePos)) {
                    game.reset();
                    gameOver = false;
                    isVictory = false;
                    wheelUsed = false;
                    detectorUsed = false;
                    bootUsed = false;
                    bootActive = false;
                    bootProtectionUsed = false;
                    levelPassedSoundPlayed = false;
                    musicController.resume();
                    restartDelay = true;
                    restartDelayClock.restart();
                }
            }

            if (inMenu && !inSettings && !isLoading) {
                int buttonClicked = mainMenu.handleEvents(event, mousePos);
                if (buttonClicked == 1) {
                    isLoading = true;
                    loadingComplete = false;
                    loadStep = 0;
                    loadingScreen.activate();
                    musicController.fadeOut();
                }
                else if (buttonClicked == 2) {
                    if (resources.loadMenuResources()) {
                        settingsMenu.initialize(resources.getTextures().settingsBackground);
                        inSettings = true;
                    }
                }
                else if (buttonClicked == 3) {
                    window.close();
                }
            }
            else if (inSettings) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (settingsMenu.isBackButtonClicked(mousePos)) {
                        inSettings = false;
                    }
                    else if (settingsMenu.isSafeCellCheckboxClicked(mousePos)) {
                        showFirstSafeCell = !showFirstSafeCell;
                    }
                    else {
                        settingsMenu.handleMousePress(mousePos);
                    }
                }
                else if (event.type == sf::Event::MouseMoved) {
                    settingsMenu.handleMouseMove(mousePos, resources.getMenuMusic(), resources.getExplosionSound());
                }
            }

            if (!isLoading && !inMenu && !gameOver && !isVictory &&
                (!restartDelay || restartDelayClock.getElapsedTime().asSeconds() > 0.3f)) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    float gameWidth = static_cast<float>(Width) * cellSize;
                    float gameHeight = static_cast<float>(Height) * cellSize;
                    float windowWidth = static_cast<float>(window.getSize().x);
                    float windowHeight = static_cast<float>(window.getSize().y);
                    float offsetX = (windowWidth - gameWidth) / 2.0f;
                    float offsetY = (windowHeight - gameHeight) / 2.0f;

                    float adjustedX = event.mouseButton.x - offsetX;
                    float adjustedY = event.mouseButton.y - offsetY;

                    if (adjustedX >= 0 && adjustedX < gameWidth && adjustedY >= 0 && adjustedY < gameHeight) {
                        int x = static_cast<int>(adjustedX / cellSize);
                        int y = static_cast<int>(adjustedY / cellSize);

                        if (x >= 0 && x < Width && y >= 0 && y < Height) {
                            if (event.mouseButton.button == sf::Mouse::Left && !game.getCell(x, y).isFlagged()) {
                                bool wasProtected = false;

                                if (bootActive && !bootProtectionUsed && game.getCell(x, y).isMine()) {
                                    wasProtected = true;
                                    bootProtectionUsed = true;
                                    bootActive = false;
                                }

                                game.revealCell(x, y);

                                if (game.isMine(x, y)) {
                                    if (!wasProtected) {
                                        gameOver = true;
                                        musicController.pause();
                                        if (resources.areSoundsLoaded()) {
                                            resources.getExplosionSound().play();
                                        }
                                    }
                                }
                                else {
                                    game.checkLevelCompletion();
                                    if (game.isLevelCompleted()) {
                                        isVictory = true;
                                        updateScore(Width, Height, Mines);
                                        if (!levelPassedSoundPlayed && resources.areSoundsLoaded()) {
                                            musicController.pause();
                                            resources.getLevelPassedSound().play();
                                            levelPassedSoundPlayed = true;
                                        }
                                    }
                                }
                            }
                            else if (event.mouseButton.button == sf::Mouse::Right) {
                                game.toggleFlag(x, y);
                            }
                        }
                    }
                }
            }
        }

        window.clear();

        if (isLoading) {
            if (inMenu) mainMenu.draw(window);
            loadingScreen.draw(window);

            if (!loadingComplete) {
                bool stepComplete = false;
                switch (loadStep) {
                case 0: stepComplete = resources.loadGameResources(); break;
                case 1:
                    stepComplete = resources.loadSounds();
                    if (stepComplete) {
                        resources.getExplosionSound().setVolume(globalVolume);
                        resources.getLevelPassedSound().setVolume(globalVolume);
                    }
                    break;
                case 2:
                    stepComplete = resources.loadGameMusic();
                    if (stepComplete) resources.getGameMusic().setVolume(globalVolume);
                    break;
                case 3: stepComplete = background.loadFrames(); break;
                case 4: stepComplete = gameBackground.loadTexture(); break;
                case 5:
                    gameBackground.initialize(Width, Height);
                    gameStarted = true;
                    stepComplete = true;
                    break;
                }

                if (stepComplete) {
                    loadStep++;
                    if (loadStep >= 6) {
                        loadingComplete = true;
                        loadingScreen.deactivate();
                        musicController.switchTo(resources.getGameMusic());
                    }
                }
            }
            else {
                isLoading = false;
                inMenu = false;
                game.reset();
                gameOver = isVictory = false;
                wheelUsed = false;
                detectorUsed = false;
                bootUsed = false;
                bootActive = false;
                bootProtectionUsed = false;
            }
        }
        else if (inMenu) {
            if (inSettings) {
                settingsMenu.draw(window);
            }
            else {
                mainMenu.update(deltaTime, mousePos);
                mainMenu.draw(window);
            }
        }
        else {
            background.update();
            background.draw(window);

            float gameWidth = static_cast<float>(Width) * cellSize;
            float gameHeight = static_cast<float>(Height) * cellSize;
            float windowWidth = static_cast<float>(window.getSize().x);
            float windowHeight = static_cast<float>(window.getSize().y);
            float offsetX = (windowWidth - gameWidth) / 2.0f;
            float offsetY = (windowHeight - gameHeight) / 2.0f;

            gameBackground.draw(window, cellSize, offsetX, offsetY);

            for (int y = 0; y < Height; y++) {
                for (int x = 0; x < Width; x++) {
                    drawCell(window, game.getCell(x, y), x, y,
                        sf::Sprite(resources.getTextures().mine),
                        sf::Sprite(resources.getTextures().flag),
                        sf::Sprite(resources.getTextures().safeCell),
                        cellSize, offsetX, offsetY);
                }
            }

            // Отрисовка меню игры
            drawGameMenu(window);

            if (gameOver) {
                drawRestartButton(window,
                    sf::Sprite(resources.getTextures().restartButton),
                    false, 1.0f, offsetX, offsetY);
            }
            else if (isVictory && !scoreAnimationActive) {
                drawRestartButton(window,
                    sf::Sprite(resources.getTextures().restartButton),
                    true, 1.0f, offsetX, offsetY);
            }
        }

        if (!fadeEffect.isComplete()) {

        }

        window.display();
    }

    return 0;
}