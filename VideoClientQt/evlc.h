#ifndef EVLA_H
#define EVLA_H
#include"string"
#include <qwindowdefs_win.h>
class Esize
{
private:
    int Width;
    int Height;
public:
    Esize();
    Esize(int wid,int hei):Width(wid),Height(hei){

    }
    Esize& operator=(const Esize& other){
        if(this == &other)
            return *this;
        Width = other.Width;
        Height = other.Height;
        return *this;
    }

};

#endif // EVLA_H
class Evla{
public:
    int SetMedia( std::string strUrl);
    int Play();
    int SetHwnd(HWND hwnd);
    int Pause();
    int Stop();
    int SetVolume(int volume);
    int GetVolume(int volume);
    int SetPosition(float position);
    int GetPosition(float position);
};
