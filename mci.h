#pragma once
#define PLAY 2
#define PAUSE 1
#define STOP 0
#define CIRCLE 10
#define LIST 20
#define RANDOM 30
#include <string>
#include <codecvt>


typedef unsigned long DWORD;

class Player
{
public:
    Player() {}
    void play(std::string& file_path);
    void stop();
    void pause();
    void resume();
    void close();
    bool is_play() const { return this->_state == 2; }
    static DWORD get_state();
    std::wstring get_message() const
    {
        return this->_str_message;
    }
private:
    const wchar_t* _path = L"";
    wchar_t* _device_type = L"mpegvideo";
    wchar_t* _str_message = L"no songs";
    int _state = STOP;
    int _mode = LIST;
};

