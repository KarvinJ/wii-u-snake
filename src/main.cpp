#include "sdl_starter.h"
#include "sdl_assets_loader.h"
#include <unistd.h> // chdir header
#include <romfs-wiiu.h>
#include <whb/proc.h>
#include <time.h>
#include <deque>
#include <math.h>
#include <fstream>

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_GameController *controller = nullptr;

Mix_Chunk *actionSound = nullptr;

bool isGamePaused;

int score;
int highScore;

SDL_Texture *pauseTexture = nullptr;
SDL_Rect pauseBounds;

SDL_Texture *scoreTexture = nullptr;
SDL_Rect scoreBounds;

SDL_Texture *highScoreTexture = nullptr;
SDL_Rect highScoreBounds;

TTF_Font *font = nullptr;

typedef struct
{
    int x;
    int y;
} Vector2;

typedef struct
{
    int cellCount;
    int cellSize;
    std::deque<Vector2> body;
    Vector2 direction;
    bool shouldAddSegment;
} Snake;

Snake snake;

typedef struct
{
    int cellCount;
    int cellSize;
    Vector2 position;
    bool isDestroyed;
} Food;

Food food;

SDL_Rect foodBounds;

int rand_range(int min, int max)
{
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

Vector2 generateRandomPosition()
{
    int positionX = rand_range(0, CELL_COUNT - 1);
    int positionY = rand_range(0, CELL_COUNT - 1);

    return Vector2{positionX, positionY};
}

Vector2 vector2Add(Vector2 vector1, Vector2 vector2)
{
    Vector2 result = {vector1.x + vector2.x, vector1.y + vector2.y};

    return result;
}

int vector2Equals(Vector2 vector1, Vector2 vector2)
{
    const float EPSILON = 0.000001f;
    int result = ((fabsf(vector1.x - vector2.x)) <= (EPSILON * fmaxf(1.0f, fmaxf(fabsf(vector1.x), fabsf(vector2.x))))) &&
                 ((fabsf(vector1.y - vector2.y)) <= (EPSILON * fmaxf(1.0f, fmaxf(fabsf(vector1.y), fabsf(vector2.y)))));

    return result;
}

double lastUpdateTime = 0;

bool eventTriggered(float deltaTime, float intervalUpdate)
{
    lastUpdateTime += deltaTime;

    if (lastUpdateTime >= intervalUpdate)
    {
        lastUpdateTime = 0;

        return true;
    }

    return false;
}

void saveScore()
{
    std::string path = "high-score.txt";

    std::ofstream highScores(path);

    std::string scoreString = std::to_string(score);
    highScores << scoreString;

    highScores.close();
}

int loadHighScore()
{
    std::string highScoreText;

    std::string path = "high-score.txt";

    std::ifstream highScores(path);

    if (!highScores.is_open())
    {
        saveScore();

        std::ifstream auxHighScores(path);

        getline(auxHighScores, highScoreText);

        highScores.close();

        int highScore = stoi(highScoreText);

        return highScore;
    }

    getline(highScores, highScoreText);

    highScores.close();

    int highScore = stoi(highScoreText);

    return highScore;
}

void resetSnakePosition()
{
    // highScore = loadHighScore();

    if (score > highScore)
    {
        saveScore();

        std::string highScoreString = "High Score: " + std::to_string(score);

        updateTextureText(highScoreTexture, highScoreString.c_str(), font, renderer);
    }

    snake.body = {{6, 9}, {5, 9}, {4, 9}};
    snake.direction = {1, 0};

    score = 0;
    updateTextureText(scoreTexture, "Score: 0", font, renderer);
}

bool checkCollisionWithFood(Vector2 foodPosition)
{
    if (vector2Equals(snake.body[0], foodPosition))
    {
        snake.shouldAddSegment = true;
        return true;
    }

    return false;
}

void checkCollisionWithEdges()
{
    if (snake.body[0].x == CELL_COUNT || snake.body[0].x == -1 || snake.body[0].y == CELL_COUNT || snake.body[0].y == -1)
    {
        resetSnakePosition();
    }
}

void checkCollisionBetweenHeadAndBody()
{
    for (size_t i = 1; i < snake.body.size(); i++)
    {
        if (vector2Equals(snake.body[0], snake.body[i]))
        {
            resetSnakePosition();
        }
    }
}

void quitGame()
{
    Mix_FreeChunk(actionSound);
    SDL_DestroyTexture(pauseTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    WHBProcShutdown();
}

void handleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            quitGame();
            break;
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
        {
            isGamePaused = !isGamePaused;
            Mix_PlayChannel(-1, actionSound, 0);
        }
    }
}

void update(float deltaTime)
{
    if (eventTriggered(deltaTime, 0.2))
    {
        if (!snake.shouldAddSegment)
        {
            snake.body.pop_back();
            snake.body.push_front(vector2Add(snake.body[0], snake.direction));
        }
        else
        {
            snake.body.push_front(vector2Add(snake.body[0], snake.direction));
            snake.shouldAddSegment = false;
        }
    }

    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) && snake.direction.y != 1)
    {
        snake.direction = {0, -1};
    }

    else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) && snake.direction.y != -1)
    {
        snake.direction = {0, 1};
    }

    else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) && snake.direction.x != -1)
    {
        snake.direction = {1, 0};
    }

    else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) && snake.direction.x != 1)
    {
        snake.direction = {-1, 0};
    }

    checkCollisionWithEdges();
    checkCollisionBetweenHeadAndBody();

    food.isDestroyed = checkCollisionWithFood(food.position);

    if (food.isDestroyed)
    {
        food.position = generateRandomPosition();
        score++;

        std::string scoreString = "Score: " + std::to_string(score);

        updateTextureText(scoreTexture, scoreString.c_str(), font, renderer);

        Mix_PlayChannel(-1, actionSound, 0);
    }
}

void renderSprite(Sprite &sprite)
{
    SDL_RenderCopy(renderer, sprite.texture, NULL, &sprite.textureBounds);
}

void render()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (isGamePaused)
    {
        SDL_RenderCopy(renderer, pauseTexture, NULL, &pauseBounds);
    }

    SDL_QueryTexture(scoreTexture, NULL, NULL, &scoreBounds.w, &scoreBounds.h);
    scoreBounds.x = SCREEN_WIDTH / 2 + 100;
    scoreBounds.y = scoreBounds.h / 2;
    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreBounds);

    SDL_QueryTexture(highScoreTexture, NULL, NULL, &highScoreBounds.w, &highScoreBounds.h);
    highScoreBounds.x = 100;
    highScoreBounds.y = highScoreBounds.h / 2;
    SDL_RenderCopy(renderer, highScoreTexture, NULL, &highScoreBounds);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (size_t i = 0; i < snake.body.size(); i++)
    {
        int positionX = snake.body[i].x;
        int positionY = snake.body[i].y;

        SDL_Rect bodyBounds = {positionX * CELL_SIZE, positionY * CELL_SIZE, CELL_SIZE, CELL_SIZE};

        SDL_RenderFillRect(renderer, &bodyBounds);
    }

    foodBounds = {food.position.x * CELL_SIZE, food.position.y * CELL_SIZE, CELL_SIZE, CELL_SIZE};

    SDL_RenderFillRect(renderer, &foodBounds);

    SDL_RenderDrawLine(renderer, 0, 1, SCREEN_WIDTH, 1);
    SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SCREEN_HEIGHT - 1);
    SDL_RenderDrawLine(renderer, 0, 0, 0, SCREEN_HEIGHT);
    SDL_RenderDrawLine(renderer, SCREEN_WIDTH - 1, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT);

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    WHBProcInit();
    romfsInit();
    chdir("romfs:/");

    window = SDL_CreateWindow("Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (startSDL(window, renderer) > 0)
    {
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    controller = SDL_GameControllerOpen(0);

    font = TTF_OpenFont("fonts/LeroyLetteringLightBeta01.ttf", 36);

    updateTextureText(pauseTexture, "Game Paused", font, renderer);

    SDL_QueryTexture(pauseTexture, NULL, NULL, &pauseBounds.w, &pauseBounds.h);
    pauseBounds.x = SCREEN_WIDTH / 2 - pauseBounds.w / 2;
    pauseBounds.y = 100;

    // highScore = loadHighScore();

    std::string highScoreString = "High Score: " + std::to_string(highScore);

    updateTextureText(highScoreTexture, highScoreString.c_str(), font, renderer);

    updateTextureText(scoreTexture, "Score: 0", font, renderer);

    actionSound = loadSound("sounds/pop1.wav");

    Mix_VolumeChunk(actionSound, MIX_MAX_VOLUME / 2);

    Uint32 previousFrameTime = SDL_GetTicks();
    Uint32 currentFrameTime = previousFrameTime;
    float deltaTime = 0.0f;

    srand(time(NULL));

    Vector2 initialFoodPosition = generateRandomPosition();

    food = {CELL_COUNT, CELL_SIZE, initialFoodPosition, false};

    std::deque<Vector2> initialBody = {{6, 9}, {5, 9}, {4, 9}};
    Vector2 direction = {1, 0};

    snake = {CELL_COUNT, CELL_SIZE, initialBody, direction, false};

    while (true)
    {
        currentFrameTime = SDL_GetTicks();
        deltaTime = (currentFrameTime - previousFrameTime) / 1000.0f;
        previousFrameTime = currentFrameTime;

        SDL_GameControllerUpdate();

        handleEvents();

        if (!isGamePaused)
        {
            update(deltaTime);
        }

        render();
    }

    quitGame();
}
