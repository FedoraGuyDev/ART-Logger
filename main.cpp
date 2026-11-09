#include <iostream>

#define NOGDI
#define NOUSER
#include "udp.hpp"

#include <vector>
#include <string>
#include "raylib.h"
#include "rlgl.h"
#include "functional"
#include "json.hpp"
#include <algorithm>
#include <chrono>
#include <format>

struct Log{
    int logLevel;
    std::string logText;
    std::string logTimestamp;
};

struct Button{
    Texture2D& icon;
    float x;
    float y;
    int w;
    int h;
    bool toggled = true;
};

std::string runtimeUUID = "nodefined";
std::vector<Log> logs;

void CheckButton(const Button& btn, std::function<void()> fnc){
    if(CheckCollisionPointRec(GetMousePosition(),{btn.x,btn.y,btn.w,btn.h})){
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && fnc){
            fnc();
        }
    }
}
void DrawButton(const Button& btn){
    Color noActiveColor = {255,255,255,128};

    if(btn.toggled){
        DrawTexture(btn.icon,btn.x,btn.y,WHITE);
    }else{
        DrawTexture(btn.icon,btn.x,btn.y,noActiveColor);
    }
}

std::string getActualTime(){
    auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    std::chrono::zoned_time local_time{std::chrono::current_zone(), now};

    return std::format("{:%H:%M:%S}", local_time);
}

float windowAlpha = 0.0f;

float scroll  = 0;

int main(){/*
    logs.push_back({0,"aaaaaaaa","00:00:00"});
    logs.push_back({2,"bbbbbbbbbbbb","00:00:00"});
    logs.push_back({0,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({1,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({0,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({0,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({1,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({0,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({2,"ccccccccccccccccccc","00:00:00"});
    logs.push_back({0,"ccccccccccccccccccc","00:00:00"});*/

    bool showInfo = true;
    bool showAdvice = true;
    bool showError = true;

    bool shouldclose = false;

    SetConfigFlags(
        FLAG_WINDOW_UNDECORATED |
        FLAG_WINDOW_TRANSPARENT
    );
    ///Init Window
    InitWindow(800,500,"ART Logger");
    SetTargetFPS(60);

    RenderTexture2D canvas = LoadRenderTexture(800, 500);
    RenderTexture2D canvaslogs = LoadRenderTexture(759,423);

    ///Load Font
    Font consolasFont = LoadFont("assets/consolas.ttf");

    ///Load Images
    Texture2D background = LoadTexture("assets/background.png");

    Texture2D icon = LoadTexture("assets/icon.png");
    Texture2D iconGlow = LoadTexture("assets/iconglow.png");

    Texture2D minimizeButton = LoadTexture("assets/minimize.png");
    Texture2D exitButton = LoadTexture("assets/exit.png");

    Texture2D informationButton = LoadTexture("assets/informationbutton.png");
    Texture2D adviceButton = LoadTexture("assets/advicebutton.png");
    Texture2D errorButton = LoadTexture("assets/errorbutton.png");
    Texture2D saveButton = LoadTexture("assets/savebutton.png");

    Texture2D informationIcon = LoadTexture("assets/informationicon.png");
    Texture2D adviceIcon = LoadTexture("assets/adviceicon.png");
    Texture2D errorIcon = LoadTexture("assets/erroricon.png");

    Button ButtonExit = {exitButton,740,22,exitButton.width,exitButton.height};
    Button ButtonMinimize = {minimizeButton,700,22,minimizeButton.width,minimizeButton.height};

    Button ButtonSave = {saveButton,98,14,saveButton.width,saveButton.height};
    Button ButtonInformation = {informationButton,147,14,informationButton.width,informationButton.height};
    Button ButtonAdvice = {adviceButton,197,14,adviceButton.width,adviceButton.height};
    Button ButtonError = {errorButton,250,14,errorButton.width,errorButton.height};

    ///Start Socket and listener
    udp::Socket sock;
    sock.bind(7777,"127.0.0.1");

    while(!shouldclose){
        if (windowAlpha + 0.1 <= 1.1) windowAlpha += 0.1;
        shouldclose = WindowShouldClose();

        ///Maximize window if its minimized
        if(IsWindowMinimized() && IsWindowFocused()){
            RestoreWindow();
        }

        ///DRAG N MOVE WINDOW
        Rectangle titleBar = {0,0,800,67};

        static bool dragging = false;

        static Vector2 dragOffset = {0,0};

        Vector2 dragMouse = GetMousePosition();

        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(dragMouse,titleBar)){
            dragging = true;
            dragOffset = dragMouse;
        }

        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonUp(MOUSE_BUTTON_LEFT)){
            dragging = false;
        }

        if(dragging){
            Vector2 windowPos = GetWindowPosition();

            SetWindowPosition(windowPos.x + dragMouse.x - dragOffset.x ,windowPos.y + dragMouse.y - dragOffset.y);
        }

        ///CHECK BUTTONS
        CheckButton(ButtonMinimize,[](){
                        MinimizeWindow();
        });
        CheckButton(ButtonExit,[&](){
                        shouldclose = true;
        });
        CheckButton(ButtonInformation,[&](){
                        showInfo = !showInfo;
                        ButtonInformation.toggled = showInfo;
        });
        CheckButton(ButtonAdvice,[&](){
                        showAdvice = !showAdvice;
                        ButtonAdvice.toggled = showAdvice;
        });
        CheckButton(ButtonError,[&](){
                        showError = !showError;
                        ButtonError.toggled = showError;
        });

        ///GET LOG;
        udp::Endpoint from;
        while (auto msg = sock.recv_string(from,0)){
            nlohmann::json j = nlohmann::json::parse(*msg,nullptr,false);
            if(!j.is_object()) continue;

            int level = j.value("level",0);
            std::string text = j.value("text", "");
            std::string timestamp = j.value("timestamp", "00:00:00");
            std::string uuid = j.value("uuid","-1");

            if(runtimeUUID == "nodefined" && uuid != "-1"){
                runtimeUUID = uuid;

                logs.push_back({0,"[ART LOGGER] Targeting to uuid: " + uuid,getActualTime()});
            }

            if(uuid == runtimeUUID){
                logs.push_back({level,text,timestamp});
                std::cout << "Log Level: " << level << " Text: " << text << " Timestamp: " << timestamp << std::endl;
            }
        }

        ///SCROLL
        if(GetMouseWheelMove() != 0.0f){
            scroll += 32 * GetMouseWheelMove();

            scroll = std::clamp((int)scroll,-2000,0);
        }

        ///DRAW
        BeginTextureMode(canvaslogs);
        int logindex = 0;
        for(Log& log : logs){
            Texture2D* iconlog = nullptr;

            switch(log.logLevel){
                case 0:
                    if(!showInfo) continue;

                    iconlog = &informationIcon;
                    break;
                case 1:
                    if(!showAdvice) continue;

                    iconlog = &adviceIcon;
                    break;
                case 2:
                    if(!showError) continue;

                    iconlog = &errorIcon;
                    break;
            }

            DrawTexture(*iconlog,0,iconlog->height * logindex + scroll,WHITE);

            std::string txt = "[ " + log.logTimestamp + " ] - " + log.logText;
            DrawTextPro(consolasFont,txt.c_str(),{iconlog->width + 8,iconlog->height * logindex + 18+ scroll},{0,0},0,16,1,WHITE);

            logindex++;
        }

        ClearBackground(BLANK);

        EndTextureMode();

        BeginTextureMode(canvas);

        rlSetBlendFactorsSeparate(
            RL_SRC_ALPHA,
            RL_ONE_MINUS_SRC_ALPHA,
            RL_ONE,
            RL_ONE_MINUS_SRC_ALPHA,
            RL_FUNC_ADD,
            RL_FUNC_ADD
        );

        BeginBlendMode(BLEND_CUSTOM_SEPARATE);

        ClearBackground(BLANK);

        DrawTexture(background,0,0,WHITE);

        if(!IsWindowFocused()){
            DrawTexture(icon,16,14,WHITE);
        }else{
            DrawTexture(iconGlow,16,14,WHITE);
        }

        DrawButton(ButtonExit);
        DrawButton(ButtonMinimize);

        DrawButton(ButtonSave);
        DrawButton(ButtonInformation);
        DrawButton(ButtonAdvice);
        DrawButton(ButtonError);

        DrawTextureRec(canvaslogs.texture,{0,0,759,-423},{0,77},WHITE);

        EndTextureMode();

        BeginDrawing();

        ClearBackground(BLANK);

        DrawTextureRec(canvas.texture,{0,0,800,-500},{0,0},{255,255,255,255*windowAlpha});

        EndDrawing();
    }

    UnloadRenderTexture(canvas);
    UnloadRenderTexture(canvaslogs);

    UnloadFont(consolasFont);

    UnloadTexture(background);
    UnloadTexture(icon);
    UnloadTexture(iconGlow);

    UnloadTexture(minimizeButton);
    UnloadTexture(exitButton);

    UnloadTexture(informationButton);
    UnloadTexture(adviceButton);
    UnloadTexture(errorButton);
    UnloadTexture(saveButton);

    UnloadTexture(informationIcon);
    UnloadTexture(adviceIcon);
    UnloadTexture(errorIcon);
    CloseWindow();
    return 0;
}
