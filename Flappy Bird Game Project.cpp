#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <SDL_mixer.h>

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int BIRD_WIDTH = 55;
const int BIRD_HEIGHT = 55;
const int PIPE_WIDTH = 80;
const int PIPE_GAP = 200;
const int INITIAL_PIPE_VELOCITY = 5;
const int MAX_PIPES = 10;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* birdTexture = nullptr;
SDL_Texture* pipeTexture = nullptr;
SDL_Texture* forestTexture = nullptr;
TTF_Font* font = nullptr;
Mix_Music* backgroundMusic = nullptr;
Mix_Chunk* collisionSound = nullptr;


// Bird position and velocity
int birdX = 100, birdY = SCREEN_HEIGHT / 2, birdVelY = 0;
const int GRAVITY = 1;
const int JUMP_STRENGTH = -15;

// Pipe positions and lifecycle
int pipeX[MAX_PIPES];
int pipeHeight[MAX_PIPES];
bool pipeActive[MAX_PIPES];
int pipeSpawnTimer = 0;

// Game variables
int score = 0;
int highScore = 0;
bool isGameOver = false;
bool gameStarted = false;
bool isPaused = false;
int pipeVelocity = INITIAL_PIPE_VELOCITY;

// File paths for score storage
const string SCORE_FILE = "Scores.txt";
const string HIGHSCORE_FILE = "Highscore.txt";

bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    window = SDL_CreateWindow("Flappy Bird", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        cerr << "SDL_image could not initialize! IMG_Error: " << IMG_GetError() << endl;
        return false;
    }

    if (TTF_Init() == -1) {
        cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << endl;
        return false;
    }

    return true;
}

SDL_Texture* loadTexture(const string& path) {
    SDL_Texture* newTexture = IMG_LoadTexture(renderer, path.c_str());
    if (!newTexture) {
        cerr << "Unable to load texture from " << path << "! SDL_image Error: " << IMG_GetError() << endl;
    }
    return newTexture;
}

SDL_Texture* loadTextTexture(const string& text, TTF_Font* font, SDL_Color color) {
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!textSurface) {
        cerr << "Unable to create text surface! TTF_Error: " << TTF_GetError() << endl;
        return nullptr;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    return textTexture;
}

void loadHighScore() {
    ifstream file(HIGHSCORE_FILE);
    if (file.is_open()) {
        file >> highScore;
        file.close();
    }
    else {
        highScore = 0; // Default high score
    }
}

void saveHighScore() {
    ofstream file(HIGHSCORE_FILE);
    if (file.is_open()) {
        file << highScore;
        file.close();
    }
}

void saveScore(int score) {
    ofstream file(SCORE_FILE, ios::app);
    if (file.is_open()) {
        file << score << endl;
        file.close();
    }
}

void printScoresToConsole() {
    cout << "Score: " << score << " | High Score: " << highScore << endl;
}

void handleEvents(bool& quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            quit = true;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_SPACE:
                if (isPaused) break;
                if (!gameStarted) {
                    gameStarted = true;
                    Mix_ResumeMusic(); // Resume background music

                    isGameOver = false;
                    birdY = SCREEN_HEIGHT / 2;
                    birdVelY = 0;
                    score = 0;

                    for (int i = 0; i < MAX_PIPES; ++i) {
                        pipeActive[i] = false;
                    }
                    cout << "Game Started! Good luck!" << endl;
                }
                else if (!isGameOver) {
                    birdVelY = JUMP_STRENGTH;
                }
                else if (isGameOver) {
                    saveScore(score);

                    if (score > highScore) {
                        highScore = score;
                        saveHighScore();
                        cout << "New High Score: " << highScore << "!" << endl;
                    }

                    gameStarted = true;
                    isGameOver = false;
                    birdY = SCREEN_HEIGHT / 2;
                    birdVelY = 0;
                    score = 0;

                    for (int i = 0; i < MAX_PIPES; ++i) {
                        pipeActive[i] = false;
                    }
                    pipeVelocity = INITIAL_PIPE_VELOCITY;
                    cout << "Game Restarted!" << endl;

                    // Add this line to start the music again
                    Mix_PlayMusic(backgroundMusic, -1);  // Restart background music


                }
                break;
            case SDLK_p:
                if (gameStarted && !isGameOver) {
                    isPaused = !isPaused;
                    if (isPaused) {
                        Mix_PauseMusic(); // Pause background music
                    }
                    else {
                        Mix_ResumeMusic(); // Resume background music
                    }
                    cout << (isPaused ? "Game Paused" : "Game Resumed") << endl;
                }
                break;

            }
        }
    }
}

void spawnPipe() {
    for (int i = 0; i < MAX_PIPES; ++i) {
        if (!pipeActive[i]) {
            pipeX[i] = SCREEN_WIDTH;
            pipeHeight[i] = rand() % (SCREEN_HEIGHT - PIPE_GAP);
            pipeActive[i] = true;
            break;
        }
    }
}

bool checkCollision() {
    if (birdY + BIRD_HEIGHT >= SCREEN_HEIGHT || birdY <= 0) {
        return true;
    }

    SDL_Rect birdRect = { birdX, birdY, BIRD_WIDTH, BIRD_HEIGHT };

    for (int i = 0; i < MAX_PIPES; ++i) {
        if (pipeActive[i]) {
            SDL_Rect topPipeRect = { pipeX[i], 0, PIPE_WIDTH, pipeHeight[i] };
            SDL_Rect bottomPipeRect = { pipeX[i], pipeHeight[i] + PIPE_GAP, PIPE_WIDTH, SCREEN_HEIGHT - (pipeHeight[i] + PIPE_GAP) };

            if (SDL_HasIntersection(&birdRect, &topPipeRect) || SDL_HasIntersection(&birdRect, &bottomPipeRect)) {
                return true;
            }
        }
    }

    return false;
}

void update() {
    if (!gameStarted || isGameOver || isPaused) return;

    birdVelY += GRAVITY;
    birdY += birdVelY;

    for (int i = 0; i < MAX_PIPES; ++i) {
        if (pipeActive[i]) {
            pipeX[i] -= pipeVelocity;

            if (pipeX[i] + PIPE_WIDTH < 0) {
                pipeActive[i] = false;
                score++;
                printScoresToConsole();

                // Increase difficulty as score increases
                if (score % 5 == 0) {
                    pipeVelocity++;
                }
            }
        }
    }

    if (++pipeSpawnTimer > 100) {
        spawnPipe();
        pipeSpawnTimer = 0;
    }

    if (checkCollision()) {
        Mix_PlayChannel(-1, collisionSound, 0);
        Mix_PauseMusic(); // Pause background music

        isGameOver = true;
        cout << "Game Over! Final Score: " << score << endl;
        if (score > highScore) {
            cout << "Congratulations! New High Score: " << score << "!" << endl;

        }
    }
}

void render() {
    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, forestTexture, nullptr, nullptr);

    SDL_Rect birdRect = { birdX, birdY, BIRD_WIDTH, BIRD_HEIGHT };
    SDL_RenderCopy(renderer, birdTexture, nullptr, &birdRect);

    for (int i = 0; i < MAX_PIPES; ++i) {
        if (pipeActive[i]) {
            SDL_Rect topPipe = { pipeX[i], 0, PIPE_WIDTH, pipeHeight[i] };
            SDL_RenderCopy(renderer, pipeTexture, nullptr, &topPipe);

            SDL_Rect bottomPipe = { pipeX[i], pipeHeight[i] + PIPE_GAP, PIPE_WIDTH, SCREEN_HEIGHT - (pipeHeight[i] + PIPE_GAP) };
            SDL_RenderCopy(renderer, pipeTexture, nullptr, &bottomPipe);
        }
    }

    // Render current score
    SDL_Color textColor = { 255, 255, 255 };
    string scoreText = "Score: " + to_string(score);
    SDL_Texture* scoreTexture = loadTextTexture(scoreText, font, textColor);
    SDL_Rect scoreRect = { 10, 10, 0, 0 };
    SDL_QueryTexture(scoreTexture, nullptr, nullptr, &scoreRect.w, &scoreRect.h);
    SDL_RenderCopy(renderer, scoreTexture, nullptr, &scoreRect);
    SDL_DestroyTexture(scoreTexture);

    // Render high score
    string highScoreText = "High Score: " + to_string(highScore);
    SDL_Texture* highScoreTexture = loadTextTexture(highScoreText, font, textColor);
    SDL_Rect highScoreRect = { SCREEN_WIDTH - 200, 10, 0, 0 };
    SDL_QueryTexture(highScoreTexture, nullptr, nullptr, &highScoreRect.w, &highScoreRect.h);
    SDL_RenderCopy(renderer, highScoreTexture, nullptr, &highScoreRect);
    SDL_DestroyTexture(highScoreTexture);

    if (isGameOver) {
        string gameOverText = "Game Over! Press Space to Restart";
        SDL_Texture* gameOverTexture = loadTextTexture(gameOverText, font, textColor);
        int textWidth = 0, textHeight = 0;
        SDL_QueryTexture(gameOverTexture, nullptr, nullptr, &textWidth, &textHeight);
        SDL_Rect gameOverRect = { (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2, textWidth, textHeight };
        SDL_RenderCopy(renderer, gameOverTexture, nullptr, &gameOverRect);
        SDL_DestroyTexture(gameOverTexture);
    }

    if (isPaused) {
        string pausedText = "Game Paused. Press P to Resume";
        SDL_Texture* pausedTexture = loadTextTexture(pausedText, font, textColor);
        int textWidth = 0, textHeight = 0;
        SDL_QueryTexture(pausedTexture, nullptr, nullptr, &textWidth, &textHeight);
        SDL_Rect pausedRect = { (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2, textWidth, textHeight };
        SDL_RenderCopy(renderer, pausedTexture, nullptr, &pausedRect);
        SDL_DestroyTexture(pausedTexture);
    }

    SDL_RenderPresent(renderer);
}

void cleanUp() {
    SDL_DestroyTexture(birdTexture);
    SDL_DestroyTexture(pipeTexture);
    SDL_DestroyTexture(forestTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    IMG_Quit();
    SDL_Quit();
    TTF_Quit();
    Mix_FreeMusic(backgroundMusic);
    Mix_FreeChunk(collisionSound);
    Mix_CloseAudio();

}

int main(int argc, char* args[]) {
    srand(time(0));

    if (!initSDL()) {
        cerr << "Failed to initialize SDL!" << endl;
        return -1;
    }

    loadHighScore();

    birdTexture = loadTexture("assets/bird.png");
    pipeTexture = loadTexture("assets/pipe.png");
    forestTexture = loadTexture("assets/forest.png");
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << endl;
        return -1;
    }

    // Load background music and collision sound
    backgroundMusic = Mix_LoadMUS("assets/background_music.mp3");
    if (!backgroundMusic) {
        cerr << "Failed to load background music! Mix_Error: " << Mix_GetError() << endl;
    }

    collisionSound = Mix_LoadWAV("assets/collision_sound.wav");
    if (!collisionSound) {
        cerr << "Failed to load collision sound! Mix_Error: " << Mix_GetError() << endl;
    }

    // Play background music in a loop
    if (Mix_PlayMusic(backgroundMusic, -1) == -1) {
        cerr << "Failed to play background music! Mix_Error: " << Mix_GetError() << endl;
    }


    font = TTF_OpenFont("assets/arial.ttf", 28);
    if (!font) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        return -1;
    }

    bool quit = false;
    while (!quit) {
        handleEvents(quit);
        update();
        render();
        SDL_Delay(1000 / 60); // 60 FPS
    }

    cleanUp();
    return 0;
}