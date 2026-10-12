#include <iostream>
#include <raylib.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

int main(int argc, char** argv){
        
    int sW = 800;
    int sH = 800;
    std::srand(std::time({}));
    
    SetConfigFlags(FLAG_MSAA_4X_HINT);

    InitWindow(sW,sH,"window");

    struct Picircle{
        private:
        public:
        Vector2 center_circle;
        int radius;
        Color colour = GREEN;



        Picircle(Vector2 center_position,int radius):center_circle (center_position),radius(radius){};

    } ;

    Picircle mycircle ({400,400},200); 
    
    //
    int g = 0;
    float red = 0;
    float blue = 0;
    
    SetTargetFPS(60);

    while (!WindowShouldClose()) {

    



        // DrawText("helloworld\n",0,0,11, LIGHTGRAY);
        
        if(g <1000000){
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        DrawCircleV(mycircle.center_circle,mycircle.radius,mycircle.colour);
        
        DrawCircleV(mycircle.center_circle,mycircle.radius-1,GRAY);

        // std::vector<int> points_x;

        for (int i = 0; i < 1000000;i++){
                int x = rand() % 801; 
                int y = rand() % 801; 
                Vector2 point = {x,y};

                if(CheckCollisionPointCircle(point,mycircle.center_circle,mycircle.radius)){
                    DrawCircleV(point,2,RED);
                    red++;

                } else {
                    DrawCircleV(point,2,BLUE);
                    blue++;
                }
                g++;
            }

                
        DrawText(TextFormat("red: %d", (int)red), 20,700,32,GREEN);
        DrawText(TextFormat("blue: %d", (int)blue), 20,728,32,GREEN);
        
    }



    float pi = ((red)/(blue+red))/(mycircle.radius*mycircle.radius) * sW*sW;

    


    DrawText(TextFormat("pi:%f ",pi), 20,750,40,BLACK);

    EndDrawing();
    }
    

    
    
    CloseWindow();
    return 0;

}
