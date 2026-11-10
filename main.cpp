#define NOGDI
#define NOUSER

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>

#include "json.hpp"
#include "udp.hpp"
#include "raylib.h"
#include "rlgl.h"
#include "save.h"

struct Log{
    int logLevel;
    std::string logText;
    std::string logTimestamp;
    std::vector<std::string> lines;
    int height = 0;
    bool wrapped = false;
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

const int logViewW = 759;
const int logViewH = 423;
const float fontSize = 16.0f;
const float fontSpacing = 1.0f;
const int lineHeight = 18;
const int textTopOffset = 18;
const int entryBottomPad = 8;

void CheckButton(const Button& btn, std::function<void()> fnc){
    Rectangle rect = {btn.x, btn.y, (float)btn.w, (float)btn.h};
    if(CheckCollisionPointRec(GetMousePosition(), rect)){
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

float TextWidth(const Font& font, const std::string& s){
    return MeasureTextEx(font, s.c_str(), fontSize, fontSpacing).x;
}

std::vector<std::string> WrapText(const Font& font, const std::string& text, float maxWidth){
    std::vector<std::string> lines;

    size_t start = 0;
    while(true){
        size_t end = text.find('\n', start);
        std::string paragraph = text.substr(start, end == std::string::npos ? std::string::npos : end - start);

        std::string current;
        size_t pos = 0;
        while(pos <= paragraph.size()){
            size_t sp = paragraph.find(' ', pos);
            std::string word = paragraph.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
            pos = (sp == std::string::npos) ? paragraph.size() + 1 : sp + 1;

            std::string candidate = current.empty() ? word : current + " " + word;
            if(TextWidth(font, candidate) <= maxWidth){
                current = candidate;
                continue;
            }

            if(!current.empty()){
                lines.push_back(current);
                current.clear();
            }

            while(word.size() > 1 && TextWidth(font, word) > maxWidth){
                size_t n = 1;
                while(n < word.size() && TextWidth(font, word.substr(0, n + 1)) <= maxWidth) n++;
                while(n > 1 && n < word.size() && (word[n] & 0xC0) == 0x80) n--;
                lines.push_back(word.substr(0, n));
                word = word.substr(n);
            }
            current = word;
        }
        lines.push_back(current);

        if(end == std::string::npos) break;
        start = end + 1;
    }

    if(lines.empty()) lines.push_back("");
    return lines;
}

bool IsLevelVisible(int level, bool showInfo, bool showAdvice, bool showError){
    switch(level){
        case 0: return showInfo;
        case 1: return showAdvice;
        case 2: return showError;
    }
    return false;
}

Texture2D* IconForLevel(int level, Texture2D& info, Texture2D& advice, Texture2D& error){
    switch(level){
        case 0: return &info;
        case 1: return &advice;
        case 2: return &error;
    }
    return nullptr;
}

float windowAlpha = 0.0f;

float scroll  = 0;
float scrollmax = 0;
int main(){
    bool showInfo = true;
    bool showAdvice = true;
    bool showError = true;

    bool shouldclose = false;

    SetConfigFlags(
        FLAG_WINDOW_UNDECORATED |
        FLAG_WINDOW_TRANSPARENT
    );
    InitWindow(800,500,"ART Logger");
    SetTargetFPS(60);

    RenderTexture2D canvas = LoadRenderTexture(800, 500);
    RenderTexture2D canvaslogs = LoadRenderTexture(logViewW,logViewH);

    Font consolasFont = LoadFont("assets/consolas.ttf");

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

    udp::Socket sock;
    sock.bind(7777,"127.0.0.1");

    const int textX = informationIcon.width + 8;
    const float textMaxWidth = (float)(logViewW - textX - 8);

    while(!shouldclose){
        if (windowAlpha + 0.1 <= 1.1) windowAlpha += 0.1;
        shouldclose = WindowShouldClose();

        if(IsWindowMinimized() && IsWindowFocused()){
            RestoreWindow();
        }

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
        CheckButton(ButtonSave,[&](){
                        std::string logToSave = "";

                        for(Log& log : logs){
                            logToSave = logToSave + "["+ log.logTimestamp + "] - " +  log.logText + "\n";
                        }

                        SaveTXTDialog(logToSave);
        });
        bool moveScroll = false;

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

                bool atBottom = scroll <= -scrollmax + 64;
                if(atBottom && IsLevelVisible(level,showInfo,showAdvice,showError)){
                    moveScroll = true;
                }
            }
        }

        // Layout pass: only visible logs count toward the total height
        float totalHeight = 0;
        for(Log& log : logs){
            Texture2D* iconlog = IconForLevel(log.logLevel,informationIcon,adviceIcon,errorIcon);
            if(!iconlog || !IsLevelVisible(log.logLevel,showInfo,showAdvice,showError)) continue;

            if(!log.wrapped){
                std::string txt = "[" + log.logTimestamp + "] - " + log.logText;
                log.lines = WrapText(consolasFont,txt,textMaxWidth);
                log.height = std::max(iconlog->height,
                                      textTopOffset + (int)log.lines.size() * lineHeight + entryBottomPad);
                log.wrapped = true;
            }

            totalHeight += log.height;
        }

        scrollmax = std::max(0.0f,totalHeight - logViewH);

        if(GetMouseWheelMove() != 0.0f){
            scroll += 32 * GetMouseWheelMove();
        }

        if(moveScroll){
            scroll = -scrollmax;
        }

        scroll = std::clamp(scroll,-scrollmax,0.0f);

        BeginTextureMode(canvaslogs);

        ClearBackground(BLANK);

        float y = scroll;
        for(Log& log : logs){
            Texture2D* iconlog = IconForLevel(log.logLevel,informationIcon,adviceIcon,errorIcon);
            if(!iconlog || !IsLevelVisible(log.logLevel,showInfo,showAdvice,showError)) continue;

            if(y + log.height >= 0 && y <= logViewH){
                DrawTexture(*iconlog,0,(int)y,WHITE);

                for(size_t i = 0; i < log.lines.size(); i++){
                    DrawTextPro(consolasFont,log.lines[i].c_str(),
                                {(float)textX, y + textTopOffset + i * lineHeight},
                                {0,0},0,fontSize,fontSpacing,WHITE);
                }
            }

            y += log.height;
        }

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

        DrawTextureRec(canvaslogs.texture,{0,0,(float)logViewW,-(float)logViewH},{0,77},WHITE);

        EndTextureMode();

        BeginDrawing();

        ClearBackground(BLANK);

        DrawTextureRec(canvas.texture,{0,0,800,-500},{0,0},{255,255,255,(unsigned char)(255*windowAlpha)});

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
