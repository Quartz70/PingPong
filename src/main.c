#include <raylib.h>
#include <raymath.h>

#define WINDOW_WIDTH 1366
#define WINDOW_HEIGHT 768

#define PADDLE_WIDTH 20.0f
#define PADDLE_HEIGHT 100.0f
#define PADDLE_PADDING 10.0f
#define PADDLE_SPEED 500.0f // per sec
#define BALL_RADIUS 10.0f
#define BALL_MAX_SPEED 1000.0f
#define BALL_MAX_ANGLE 60

typedef struct Ball {
    Vector2 center;
    float radius;
    Vector2 velocity;
} Ball;

Vector2 Vector2RotateAround(Vector2 v1, Vector2 v2, float angle) {
    Vector2 v2v1 = Vector2Subtract(v1, v2);
    Vector2 v2v1R = Vector2Rotate(v2v1, angle);
    return Vector2Add(v2, v2v1R);
}

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

float BallBounceAngle(float ballCenterY, Rectangle paddle) {
    // relY: 0 — верх ракетки, 1 — низ
    float relY = (ballCenterY - paddle.y) / paddle.height;
    relY = Clamp(relY, 0.0f, 1.0f);
    return (relY * 2.0f - 1.0f) * BALL_MAX_ANGLE * DEG2RAD;
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
        float angleRad = BallBounceAngle(ball->center.y, myPaddle);

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
        float angleRad = BallBounceAngle(ball->center.y, opponentPaddle);

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

        Vector2 line1Begin;
        line1Begin.x = ball->center.x - ball->radius - 5.0f;
        line1Begin.y = ball->center.y - ball->radius / 2.0f;
        Vector2 line1End;
        line1End.x = line1Begin.x - 20.0f;
        line1End.y = line1Begin.y;

        Vector2 line2Begin;
        line2Begin.x = ball->center.x - ball->radius - 7.0f;
        line2Begin.y = ball->center.y;
        Vector2 line2End;
        line2End.x = line2Begin.x - 20.0f;
        line2End.y = line2Begin.y;

        Vector2 line3Begin;
        line3Begin.x = ball->center.x - ball->radius - 5.0f;
        line3Begin.y = ball->center.y + ball->radius / 2.0f;
        Vector2 line3End;
        line3End.x = line3Begin.x - 20.0f;
        line3End.y = line3Begin.y;

        float ballAngle = Vector2Angle((Vector2){1.0f, 0.0f}, ball->velocity);

        line1Begin = Vector2RotateAround(line1Begin, ball->center, ballAngle);
        line1End = Vector2RotateAround(line1End, ball->center, ballAngle);

        line2Begin = Vector2RotateAround(line2Begin, ball->center, ballAngle);
        line2End = Vector2RotateAround(line2End, ball->center, ballAngle);

        line3Begin = Vector2RotateAround(line3Begin, ball->center, ballAngle);
        line3End = Vector2RotateAround(line3End, ball->center, ballAngle);

        DrawLineEx(line1Begin, line1End, 2.0f, (Color){255, 255, 255, 120});
        DrawLineEx(line2Begin, line2End, 2.0f, (Color){255, 255, 255, 120});
        DrawLineEx(line3Begin, line3End, 2.0f, (Color){255, 255, 255, 120});
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
