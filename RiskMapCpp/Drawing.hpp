#pragma once
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <cctype>

namespace ink {
inline void color(SDL_Renderer* r, SDL_Color c) { SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }
inline void box(SDL_Renderer* r,int x,int y,int w,int h,SDL_Color c) {
    color(r,c); SDL_Rect rect{x,y,w,h}; SDL_RenderFillRect(r,&rect);
}
inline void circle(SDL_Renderer* r,int x,int y,int radius,SDL_Color c) {
    color(r,c);
    for(int dy=-radius;dy<=radius;++dy) {
        int dx=int(std::sqrt(double(radius*radius-dy*dy)));
        SDL_RenderDrawLine(r,x-dx,y+dy,x+dx,y+dy);
    }
}
inline SDL_Color shade(SDL_Color c,double factor) {
    return {Uint8(std::clamp(int(c.r*factor),0,255)),Uint8(std::clamp(int(c.g*factor),0,255)),Uint8(std::clamp(int(c.b*factor),0,255)),255};
}
inline void medal(SDL_Renderer* r,int x,int y,int radius,SDL_Color c) {
    circle(r,x+2,y+5,radius+2,{10,13,14,170});
    circle(r,x,y,radius+1,{45,31,18,255});
    circle(r,x,y-1,radius,{220,188,118,255});
    circle(r,x,y+1,radius-2,{112,78,39,255});
    for(int dy=-radius+4;dy<=radius-4;++dy) {
        const int rr=radius-4;
        int dx=int(std::sqrt(double(rr*rr-dy*dy)));
        color(r,shade(c,1.22-0.50*double(dy+rr)/std::max(1,2*rr)));
        SDL_RenderDrawLine(r,x-dx,y+dy,x+dx,y+dy);
    }
}
// Compact built-in 5x7 font: no SDL_ttf or external font dependency.
inline void text(SDL_Renderer* r,int x,int y,const std::string& value,int size,SDL_Color c) {
    static const char* glyphs[]={
        "01110100011000111111100011000110001", "11110100011000111110100011000111110",
        "01111100001000010000100001000001111", "11110100011000110001100011000111110",
        "11111100001000011110100001000011111", "11111100001000011110100001000010000",
        "01111100001000010111100011000101111", "10001100011000111111100011000110001",
        "11111001000010000100001000010011111", "00111000100001000010000101001001100",
        "10001100101010011000101001001010001", "10000100001000010000100001000011111",
        "10001110111010110101100011000110001", "10001110011010110011100011000110001",
        "01110100011000110001100011000101110", "11110100011000111110100001000010000",
        "01110100011000110001101011001001101", "11110100011000111110101001001010001",
        "01111100001000001110000010000111110", "11111001000010000100001000010000100",
        "10001100011000110001100011000101110", "10001100011000110001100010101000100",
        "10001100011000110101101011010101010", "10001100010101000100010101000110001",
        "10001100010101000100001000010000100", "11111000010001000100010001000011111",
        "01110100011001110101110011000101110", "00100011000010000100001000010001110",
        "01110100010000100010001000100011111", "11110000010000101110000010000111110",
        "00010001100101010010111110001000010", "11111100001000011110000010000111110",
        "01110100001000011110100011000101110", "11111000010001000100010000100001000",
        "01110100011000101110100011000101110", "01110100011000101111000010000101110",
        "00000001000010011111001000010000000", "00000000000000011111000000000000000",
        "00000000000000000000000000011000110", "00000001100011000000001100011000000"
    };
    const std::string chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.:";
    for(unsigned char raw:value) {
        auto n=chars.find(char(std::toupper(raw)));
        if(n!=std::string::npos) for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(glyphs[n][row*5+col]=='1') box(r,x+col*size,y+row*size,size,size,c);
        x+=6*size;
    }
}
inline void centered(SDL_Renderer* r,int x,int y,const std::string& s,int size,SDL_Color c) {
    text(r,x-int(s.size()*6*size)/2,y,s,size,c);
}
}
