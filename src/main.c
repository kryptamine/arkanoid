#include "raylib.h"
#include "resources/destroy.h"
#include "resources/hit.h"
#include "resources/loose.h"
#include "resources/reset.h"
#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1024
#define HEIGHT 768

#define FONT_SIZE 20

#define BAR_HEIGHT 20
#define BAR_WIDTH 150
#define BAR_SPEED 10.0f
#define INITIAL_BAR_Y HEIGHT - (HEIGHT * 0.1f)

#define BALL_RADIUS 10
#define BALL_SPEED 4.0f

#define INITIAL_BALL_Y INITIAL_BAR_Y - BALL_RADIUS
#define INITIAL_BALL_X WIDTH / 2.0f

#define ROWS_COUNT 5
#define COLS_COUNT 5
#define BRICK_WIDTH 100
#define BRICK_HEIGHT 20
#define BRICK_GAP 10
#define BRICK_PADDING 25
#define BRICKS_TOP_PADDING 80
#define BRICKS_LEFT_PADDING                                                    \
  (WIDTH - (BRICK_WIDTH + BRICK_PADDING) * ROWS_COUNT) / 2.0f

#define SCORE_PADDING 35.0f
#define MAX_PARTICLES 200

#define PARTICLE_LIFESPAN 1.0f
#define PARTICLE_RADIUS 3.0f
#define PARTICLES_COUNT 200

#define SPEED_INCREMENT 1.0f

#define GRAVITY 0.02f

// Messages
#define INIT_MESSAGE "Press Space to Start"
#define GAME_OVER_MESSAGE "Game Over! Press Space to Restart."

typedef struct {
  Vector2 position;
  Vector2 velocity;
  Color color;
  float radius;
  float lifespan;
  float fade;
  bool active;
} Particle;

enum GameState {
  GameOver,
  Running,
  Init,
};

typedef struct {
  struct Vector2 position;
  struct Vector2 velocity;
  float radius;
  bool active;
} Ball;

typedef struct {
  Rectangle rect;
  Color color;
  float fade;
  bool active;
} Brick;

enum GameState gameState = Init;

static Particle particles[MAX_PARTICLES];
static Brick bricks[ROWS_COUNT][COLS_COUNT];
static Rectangle bar = {0};
static Ball ball = {0};

static int score = 0;
static int level = 1;
static int bricksLeft = ROWS_COUNT * COLS_COUNT;
static int speedIncrement = 0;
static int lives = 3;

// Sounds
static Sound hitBarSound;
static Sound destroyBrickSound;
static Sound looseSound;
static Sound resetSound;

static void DrawGame(void);
static void DrawScore(void);
static void DrawBricks(void);
static void DrawParticles(void);
static void UpdateGameFrame(void);
static void ResetGame(void);
static void ResetBall(void);
static void ResetBricks(void);
static void ResetParticles(void);
static void HandleBricksFade(void);
static bool HandleBricksCollision(void);
static bool HandleBordersCollision(void);
static void HandleSpaceKeyPressed(void);
static void HandleArrowKeysPressed(void);
static void UnloadSounds(void);
static void InitGame(void);
static void HandleBarCollision(void);
static void SpawnParticles(Vector2, int, Color);
static void UpdateParticles(void);
static void UnloadGame(void);
static void FadeInBricks(void);
static void CheckForNextLevel(void);

int main(void) {
  InitGame();

  while (!WindowShouldClose()) {
    UpdateGameFrame();
    DrawGame();
  }

  UnloadGame();

  return 0;
}

void UnloadGame(void) {
  UnloadSounds();
  CloseAudioDevice();
  CloseWindow();
}

float GetCenter(float textWidth) { return WIDTH / 2.0f - textWidth / 2.0f; }

void DrawInitMessage() {
  float textWidth = MeasureText(INIT_MESSAGE, FONT_SIZE);
  float center = GetCenter(textWidth);

  DrawText(INIT_MESSAGE, center, HEIGHT / 2.0f, FONT_SIZE, WHITE);
}

void DrawGameOverMessage() {
  float textWidth = MeasureText(GAME_OVER_MESSAGE, FONT_SIZE);
  float center = GetCenter(textWidth);

  DrawText(GAME_OVER_MESSAGE, center, HEIGHT / 2.0f, FONT_SIZE, WHITE);
}

void DrawLevel() {
  char levelText[50];
  sprintf(levelText, "Level: %d", level);
  int textWidth = MeasureText(levelText, FONT_SIZE);
  int posX = WIDTH - 160;

  DrawText(levelText, posX - textWidth, SCORE_PADDING, FONT_SIZE, WHITE);
}

void DrawScore() {
  char scoreText[50];
  sprintf(scoreText, "Score: %d", score);

  int textWidth = MeasureText(scoreText, FONT_SIZE);
  int posX = WIDTH - SCORE_PADDING;

  DrawText(scoreText, posX - textWidth, SCORE_PADDING, FONT_SIZE, WHITE);
}

void DrawLives() {
  char livesText[50];
  sprintf(livesText, "Lives: %d", lives);

  int textWidth = MeasureText(livesText, FONT_SIZE);
  int posX = WIDTH - 265;

  DrawText(livesText, posX - textWidth, SCORE_PADDING, FONT_SIZE, WHITE);
}

void CheckForNextLevel() {
  if (bricksLeft == 0) {
    ResetGame();
    level++;
    speedIncrement += SPEED_INCREMENT;
    return;
  }
}

void DrawBall(void) {
  if (!ball.active) {
    return;
  }

  DrawCircleV(ball.position, BALL_RADIUS, RED);
}

void DrawGame(void) {
  BeginDrawing();
  ClearBackground(BLACK);
  DrawScore();
  DrawLives();
  DrawLevel();
  DrawBricks();
  DrawBall();
  DrawRectangleRec(bar, BLUE);
  DrawParticles();

  if (gameState == Init) {
    DrawInitMessage();
  }

  if (gameState == GameOver) {
    DrawGameOverMessage();
  }

  EndDrawing();
}

void UpdateGameFrame(void) {
  HandleArrowKeysPressed();
  HandleSpaceKeyPressed();

  if (gameState == Init) {
    HandleBricksFade();
  }

  if (gameState == Running) {
    UpdateParticles();

    if (HandleBordersCollision()) {
      return;
    }

    HandleBarCollision();

    if (HandleBricksCollision()) {
      CheckForNextLevel();
      return;
    }

    ball.position.x += ball.velocity.x;
    ball.position.y -= ball.velocity.y;
    ball.velocity.y += GRAVITY;
  }
}

void DrawParticles(void) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle particle = particles[i];

    if (particle.active) {
      Color fadedColor = Fade(particle.color, particle.fade);
      DrawCircleV(particle.position, particle.radius, fadedColor);
    }
  }
}

void SpawnParticles(Vector2 position, int count, Color color) {
  for (int i = 0; i < count; i++) {
    for (int j = 0; j < MAX_PARTICLES; j++) {
      Particle *particle = &particles[j];

      if (particle->active) {
        continue;
      }

      particle->position = position;
      particle->velocity = (Vector2){(float)(rand() % 100 - 50) / 10.0f,
                                     (float)(rand() % 100 - 50) / 10.0f};
      particle->color = color;
      particle->radius = PARTICLE_RADIUS;
      particle->lifespan = PARTICLE_LIFESPAN;
      particle->active = true;
      particle->fade = 1.0f;
      break;
    }
  }
}

void ResetParticles(void) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }
}

void ResetBricks() {
  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      bricks[i][j].active = true;
    }
  }
}

void HandleBricksFade(void) {
  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      Brick *brick = &bricks[i][j];

      if (!brick->active || brick->fade >= 1.0f) {
        continue;
      }

      brick->fade += 0.01f;
    }
  }
}

void DestroyBrick(Brick *brick) {
  // Set the brick to inactive
  brick->active = false;
  brick->fade = 0.0f;

  // Increase score and decrease remaining brick count
  score += 10;
  bricksLeft--;

  Vector2 collisionPoint = {brick->rect.x + brick->rect.width / 2,
                            brick->rect.y + brick->rect.height / 2};
  SpawnParticles(collisionPoint, 20, brick->color);
  PlaySound(destroyBrickSound);
}

void HandleBarCollision() {
  if (!CheckCollisionCircleRec(ball.position, ball.radius, bar)) {
    return;
  }

  // Move ball back to prevent sticking
  ball.position.y = INITIAL_BALL_Y;
  ball.velocity.y *= -1;

  // Calculate offset from bar center
  float barCenter = bar.x + (bar.width / 2.0f);
  float distanceFromCenter = ball.position.x - barCenter;

  // Change X velocity proportionally based on where the ball hits the bar
  ball.velocity.x = distanceFromCenter * 0.05f;

  PlaySound(hitBarSound);
}

bool HandleBricksCollision(void) {
  // Get ball boundaries
  float ballLeft = ball.position.x - ball.radius;
  float ballRight = ball.position.x + ball.radius;
  float ballTop = ball.position.y - ball.radius;
  float ballBottom = ball.position.y + ball.radius;

  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      Brick *brick = &bricks[i][j];

      if (!brick->active) {
        continue;
      }

      if (!CheckCollisionCircleRec(ball.position, ball.radius, brick->rect)) {
        continue;
      }

      // Get brick boundaries
      float brickLeft = brick->rect.x;
      float brickRight = brick->rect.x + brick->rect.width;
      float brickTop = brick->rect.y;
      float brickBottom = brick->rect.y + brick->rect.height;

      // Calculate the overlap on each side of the brick
      float overlapLeft = ballRight - brickLeft;
      float overlapRight = brickRight - ballLeft;
      float overlapTop = ballBottom - brickTop;
      float overlapBottom = brickBottom - ballTop;

      // Determine the minimum overlap to find the collision side
      float minOverlap = fminf(fminf(overlapLeft, overlapRight),
                               fminf(overlapTop, overlapBottom));

      // Adjust ball velocity based on collision side
      if (minOverlap == overlapLeft || minOverlap == overlapRight) {
        ball.velocity.x *= -1;
      } else if (minOverlap == overlapTop || minOverlap == overlapBottom) {
        ball.velocity.y *= -1;
      }

      DestroyBrick(brick);
      return true;
    }
  }

  return false;
}

void HandleBottomCollision(void) {
  PlaySound(looseSound);

  if (lives > 1) {
    lives--;
    ResetGame();
    return;
  }

  ball.active = false;
  gameState = GameOver;
}

bool HandleBordersCollision(void) {
  if (ball.position.x > WIDTH - BALL_RADIUS) {
    ball.position.x = WIDTH - BALL_RADIUS;
    ball.velocity.x *= -1;
  }

  if (ball.position.x < BALL_RADIUS) {
    ball.position.x = BALL_RADIUS;
    ball.velocity.x *= -1;
  }

  if (ball.position.y < BALL_RADIUS) {
    ball.position.y = BALL_RADIUS;
    ball.velocity.y *= -1;
  }

  if (ball.position.y > HEIGHT - BALL_RADIUS) {
    HandleBottomCollision();
    return true;
  }

  return false;
}

void UpdateParticles() {
  float deltaTime = GetFrameTime();

  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle *particle = &particles[i];

    if (particle->active) {
      particle->position.x += particle->velocity.x * deltaTime * 60;
      particle->position.y += particle->velocity.y * deltaTime * 60;
      particle->velocity.y += GRAVITY * deltaTime * 60;

      // Decrease lifespan
      particle->lifespan -= deltaTime;
      particle->fade -= 0.01f;

      if (particle->lifespan <= 0) {
        particle->active = false;
      }
    }
  }
}

void ResetBall() {
  ball.position.x = INITIAL_BALL_X;
  ball.position.y = INITIAL_BALL_Y;
  ball.velocity.x = 0.0f;
  ball.velocity.y = BALL_SPEED + speedIncrement;
}

Color LerpColor(Color startColor, Color endColor, float t) {
  return (Color){(int)(startColor.r + t * (endColor.r - startColor.r)),
                 (int)(startColor.g + t * (endColor.g - startColor.g)),
                 (int)(startColor.b + t * (endColor.b - startColor.b)),
                 (int)(startColor.a + t * (endColor.a - startColor.a))};
}

void DrawBricks() {
  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      Brick brick = bricks[i][j];

      if (!brick.active) {
        continue;
      }

      DrawRectangleRec(brick.rect, Fade(brick.color, brick.fade));
    }
  }
}

void ResetGame() {
  gameState = Init;
  ResetParticles();
  ResetBall();
  ResetBricks();
  PlaySound(resetSound);
  bricksLeft = ROWS_COUNT * COLS_COUNT;
  ball.active = true;
}

void UnloadSounds() {
  UnloadSound(hitBarSound);
  UnloadSound(destroyBrickSound);
  UnloadSound(looseSound);
  UnloadSound(resetSound);
}

void HandleArrowKeysPressed() {
  if (IsKeyDown(KEY_LEFT) && bar.x > 0) {
    bar.x -= BAR_SPEED;
  }

  if (IsKeyDown(KEY_RIGHT) && bar.x < WIDTH - bar.width) {
    bar.x += BAR_SPEED;
  }
}

void FadeInBricks(void) {
  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      Brick *brick = &bricks[i][j];

      if (!brick->active) {
        continue;
      }

      brick->fade = 1.0f;
    }
  }
}

void HandleSpaceKeyPressed() {
  if (IsKeyPressed(KEY_SPACE)) {
    switch (gameState) {
    case Init:
      FadeInBricks();
      gameState = Running;
      break;
    case Running:
      break;
    case GameOver:
      ResetGame();
      score = 0;
      speedIncrement = 0;
      level = 1;
      lives = 3;
      break;
    }
  }
}

void InitBricks() {
  Color startColor = RED;
  Color endColor = GREEN;

  for (int i = 0; i < ROWS_COUNT; i++) {
    for (int j = 0; j < COLS_COUNT; j++) {
      Brick *brick = &bricks[i][j];

      // Calculate interpolation factors based on brick's row and column
      float rowFactor = (float)i / (ROWS_COUNT - 1);
      float colFactor = (float)j / (COLS_COUNT - 1);

      Color start = LerpColor(startColor, endColor, rowFactor);
      Color end = LerpColor(startColor, endColor, colFactor);
      Color brickColor = LerpColor(start, end, 0.2f);

      Rectangle rect = {
          .x = j * (BRICK_WIDTH + BRICK_PADDING) + BRICKS_LEFT_PADDING,
          .y = i * (BRICK_HEIGHT + BRICK_PADDING) + BRICKS_TOP_PADDING,
          .width = BRICK_WIDTH,
          .height = BRICK_HEIGHT,
      };

      brick->active = true;
      brick->fade = 0.0f;
      brick->color = brickColor;
      brick->rect = rect;
    }
  }
}

void InitSounds() {
  hitBarSound = LoadSoundFromWave((Wave){
      .frameCount = HIT_FRAME_COUNT,
      .sampleSize = HIT_SAMPLE_SIZE,
      .sampleRate = HIT_SAMPLE_RATE,
      .channels = HIT_CHANNELS,
      .data = HIT_DATA,
  });

  destroyBrickSound = LoadSoundFromWave((Wave){
      .frameCount = DESTROY_FRAME_COUNT,
      .sampleSize = DESTROY_SAMPLE_SIZE,
      .sampleRate = DESTROY_SAMPLE_RATE,
      .channels = DESTROY_CHANNELS,
      .data = DESTROY_DATA,
  });

  looseSound = LoadSoundFromWave((Wave){
      .frameCount = LOOSE_FRAME_COUNT,
      .sampleSize = LOOSE_SAMPLE_SIZE,
      .sampleRate = LOOSE_SAMPLE_RATE,
      .channels = LOOSE_CHANNELS,
      .data = LOOSE_DATA,
  });

  resetSound = LoadSoundFromWave((Wave){
      .frameCount = RESET_FRAME_COUNT,
      .sampleSize = RESET_SAMPLE_SIZE,
      .sampleRate = RESET_SAMPLE_RATE,
      .channels = RESET_CHANNELS,
      .data = RESET_DATA,
  });
}

void InitGame() {
  InitWindow(WIDTH, HEIGHT, "Arkanoid");
  SetTargetFPS(100);
  InitAudioDevice();
  InitSounds();
  InitBricks();

  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }

  bar.x = (WIDTH / 2.0f) - (BAR_WIDTH / 2.0f);
  bar.y = INITIAL_BAR_Y;
  bar.width = BAR_WIDTH;
  bar.height = BAR_HEIGHT;

  Vector2 ballVelocity = {
      .x = 0.0f,
      .y = BALL_SPEED,
  };

  Vector2 ballPosition = {
      .x = INITIAL_BALL_X,
      .y = INITIAL_BALL_Y,
  };

  ball.position = ballPosition;
  ball.velocity = ballVelocity;
  ball.radius = BALL_RADIUS;
  ball.active = true;
}
