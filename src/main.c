#include <raylib.h>
#include <raymath.h>

#define WINDOW_WIDTH 1100
#define WINDOW_HEIGHT 650

#define PADDLE_WIDTH 20.0f
#define PADDLE_HEIGHT 100.0f
#define PADDLE_PADDING 10.0f
#define PADDLE_SPEED 300.0f // per sec
#define BALL_RADIUS 10.0f
#define BALL_MAX_SPEED 1200.0f

typedef struct Ball {
    Vector2 center;
    float radius;
    Vector2 velocity;
} Ball;

void MyPaddleMoveHandle(
    const float frameTime, 
    Rectangle *myPaddle
) {
    if (IsKeyDown(KEY_W)) myPaddle->y -= PADDLE_SPEED * frameTime;
    if (IsKeyDown(KEY_S)) myPaddle->y += PADDLE_SPEED * frameTime;

    if (myPaddle->y < 0.0f) myPaddle->y = 0.0f;
    if (myPaddle->y + myPaddle->height > (float)WINDOW_HEIGHT)
        myPaddle->y = (float)WINDOW_HEIGHT - myPaddle->height;
}

void OpponentPaddleMoveHandle(
    const float frameTime,
    Rectangle *opponentPaddle,
    Ball *ball
) {
    // Насколько быстро может двигаться бот (пикселей в секунду).
    // Чуть медленнее игрока — так честнее.
    const float botSpeed = PADDLE_SPEED * 0.85f;

    // Центр ракетки по вертикали.
    float paddleCenterY = opponentPaddle->y + opponentPaddle->height / 2.0f;

    // ---- Предсказание позиции мяча ----
    // Если мяч летит в сторону бота (вправо), оцениваем, где он будет
    // по Y, когда долетит до ракетки. Иначе — просто следим за текущим Y.
    float targetY;
    if (ball->velocity.x > 0.0f) {
        float distanceX = opponentPaddle->x - ball->center.x;
        float timeToReach = distanceX / ball->velocity.x;

        // Сколько раз мяч успеет отскочить от верх/низ границы.
        // Простая модель: отражаем Y в диапазоне [radius, WINDOW_HEIGHT - radius].
        float minY = ball->radius;
        float maxY = (float)WINDOW_HEIGHT - ball->radius;
        float span  = maxY - minY;

        float predictedY = ball->center.y + ball->velocity.y * timeToReach;

        // Приводим к «треугольной волне» внутри [minY, maxY].
        if (span > 0.0f) {
            float period = 2.0f * span;
            float t = fmodf(predictedY - minY, period);
            if (t < 0.0f) t += period;
            if (t > span) t = period - t;
            predictedY = minY + t;
        }
        targetY = predictedY;
    } else {
        targetY = ball->center.y;
    }

    // ---- Движение ----
    // Мёртвая зона: не двигаемся, пока мяч почти на одной линии.
    const float deadZone = 8.0f;
    float diff = targetY - paddleCenterY;

    if (fabsf(diff) > deadZone) {
        float step = botSpeed * frameTime;
        // Не перескакиваем цель.
        if (step > fabsf(diff)) step = fabsf(diff);

        if (diff > 0.0f) opponentPaddle->y += step;
        else             opponentPaddle->y -= step;
    }

    // ---- Ограничение по экрану ----
    if (opponentPaddle->y < 0.0f) opponentPaddle->y = 0.0f;
    if (opponentPaddle->y + opponentPaddle->height > (float)WINDOW_HEIGHT)
        opponentPaddle->y = (float)WINDOW_HEIGHT - opponentPaddle->height;
}

void BallCollisionHandle(
    Rectangle myPaddle,
    Rectangle opponentPaddle,
    Ball *ball
) {
    // ---------- Левая ракетка ----------
    if (CheckCollisionCircleRec(ball->center, ball->radius, myPaddle)
        && ball->velocity.x < 0.0f)   // летит в сторону ракетки
    {
        // relY: 0 — верх ракетки, 1 — низ
        float relY = (ball->center.y - myPaddle.y) / myPaddle.height;
        relY = Clamp(relY, 0.0f, 1.0f);

        // 7 зон: 0..6
        int zone = (int)(relY * 7.0f);
        if (zone > 6) zone = 6;

        // угол от горизонтали: -60°..0°..+60°
        float angleRad = (zone - 3) * 20.0f * DEG2RAD;

        float speed = Vector2Length(ball->velocity);
        ball->velocity.x =  speed * cosf(angleRad);  // всегда вправо
        ball->velocity.y =  speed * sinf(angleRad);

        // выталкиваем мяч из ракетки, чтобы не залипал
        ball->center.x = myPaddle.x + myPaddle.width + ball->radius;
    }

    // ---------- Правая ракетка ----------
    if (CheckCollisionCircleRec(ball->center, ball->radius, opponentPaddle)
        && ball->velocity.x > 0.0f)
    {
        float relY = (ball->center.y - opponentPaddle.y) / opponentPaddle.height;
        relY = Clamp(relY, 0.0f, 1.0f);

        int zone = (int)(relY * 7.0f);
        if (zone > 6) zone = 6;

        float angleRad = (zone - 3) * 20.0f * DEG2RAD;

        float speed = Vector2Length(ball->velocity);
        ball->velocity.x = -speed * cosf(angleRad);  // всегда влево
        ball->velocity.y =  speed * sinf(angleRad);

        ball->center.x = opponentPaddle.x - ball->radius;
    }

    // ---------- Верхняя и нижняя границы ----------
    if (ball->center.y - ball->radius < 0.0f) {
        ball->center.y = ball->radius;
        ball->velocity.y = fabsf(ball->velocity.y);   // вниз
    }
    if (ball->center.y + ball->radius > (float)WINDOW_HEIGHT) {
        ball->center.y = (float)WINDOW_HEIGHT - ball->radius;
        ball->velocity.y = -fabsf(ball->velocity.y);  // вверх
    }
}

void BallMoveHandle(
    const float frameTime, 
    Ball *ball
) {
    Vector2 distance = Vector2Scale(ball->velocity, frameTime);
    ball->center = Vector2Add(ball->center, distance);

    // если кто то проиграл, начинаем игру заново
    if (ball->center.x < 0.0f || ball->center.x > WINDOW_WIDTH) {
        ball->center = (Vector2){WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f};
        ball->velocity = (Vector2){200.0f, 0.0f};
    }
}

void GameHandle(
    Rectangle *myPaddle,
    Rectangle *opponentPaddle,
    Ball *ball
) {
    const float frameTime = GetFrameTime();
    MyPaddleMoveHandle(frameTime, myPaddle);
    OpponentPaddleMoveHandle(frameTime, opponentPaddle, ball);
    BallMoveHandle(frameTime, ball);
    BallCollisionHandle(*myPaddle, *opponentPaddle, ball);

    // постепенное ускорение мяча
    float speed = Vector2Length(ball->velocity);
    if (speed < BALL_MAX_SPEED) {
        ball->velocity = Vector2Scale(ball->velocity, 1.0f + frameTime / 10.0f);
    }
}

void GameRender(Rectangle *myPaddle, Rectangle *opponentPaddle, Ball *ball) {
    BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangleRec(*myPaddle, BLUE);
        DrawRectangleRec(*opponentPaddle, RED);
        DrawCircle(ball->center.x, ball->center.y, ball->radius, WHITE);
    EndDrawing();
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "PingPong");

    Rectangle myPaddle;
    myPaddle.width = PADDLE_WIDTH;
    myPaddle.height = PADDLE_HEIGHT;
    myPaddle.x = PADDLE_PADDING;
    myPaddle.y = WINDOW_HEIGHT / 2.0f - myPaddle.height / 2.0f;

    Rectangle opponentPaddle;
    opponentPaddle.width = PADDLE_WIDTH;
    opponentPaddle.height = PADDLE_HEIGHT;
    opponentPaddle.x = WINDOW_WIDTH - PADDLE_PADDING - PADDLE_WIDTH;
    opponentPaddle.y = WINDOW_HEIGHT / 2.0f - myPaddle.height / 2.0f;

    Ball ball;
    ball.center = (Vector2){WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f};
    ball.radius = BALL_RADIUS;
    ball.velocity = (Vector2){200.0f, 0.0f};

    SetTargetFPS(60);
    DisableCursor();

    while (!WindowShouldClose()) {
        GameHandle(&myPaddle, &opponentPaddle, &ball);
        GameRender(&myPaddle, &opponentPaddle, &ball);
    }

    CloseWindow();
    return 0;
}
