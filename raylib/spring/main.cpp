#include <iostream>
#include <raylib.h>

typedef struct Spring{
    //wall side
    Vector2 pos_w;
    int w_w = 450;
    int w_h = 150;
    int w_offset = 10;
    //end effector side
    int size_e = 50;
    float l_0 = 150;
    
    //physics
    float f = 0;
    float a = 0;
    float v = 0;
    float dx = 0;
    float rest_x = 0;
    float k = 200.0f;
    float damping = 1.0f;
    Vector2 pos_e = {0, 0};
    float m = 1;

} Spring;

void drawWall(Spring& s){
    DrawRectangle(s.pos_w.x, s.pos_w.y, s.w_w, s.w_h, GREEN);
    DrawRectangle(s.pos_w.x+s.w_offset, s.pos_w.y, s.w_w-s.w_offset, s.w_h-s.w_offset, RAYWHITE);
};

void drawSquare(Spring& s){
    DrawRectangle(s.pos_e.x, s.pos_e.y, s.size_e, s.size_e, RED);
}

void drawSpring(Spring& s){
    DrawLine(s.pos_w.x+s.w_offset, s.pos_w.y+s.w_offset*11.5,
             s.pos_e.x,            s.pos_w.y+s.w_offset*11.5, BLUE);
}

int main(int argc, char** argv){

    int sW = 1200;
    int sH = 800;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(sW, sH, "Spring");

    static Spring s;

    s.pos_w   = {GetScreenWidth()/2.0f - s.w_w, GetScreenHeight()/2.0f};
    s.rest_x  = s.pos_w.x + s.w_offset + s.l_0;
    s.pos_e.x = s.rest_x;
    s.pos_e.y = s.pos_w.y + s.w_offset * 9;

    SetTargetFPS(60);
    while (!WindowShouldClose()) {

        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_D)) s.pos_e.x += 250;
        if (IsKeyPressed(KEY_A)) s.pos_e.x -= 250;

        // physics chain
        s.dx = s.pos_e.x - s.rest_x;
        s.f  = (-s.k * s.dx) + (-s.damping * s.v);   // spring + damping
        s.a  = s.f / s.m;
        s.v  += s.a * dt;
        s.pos_e.x += s.v * dt;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        drawWall(s);
        drawSquare(s);
        drawSpring(s);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}