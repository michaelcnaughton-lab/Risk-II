#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "MapInteraction.hpp"
#include "Troops.hpp"
#include "Drawing.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
static void require(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what + ": " + SDL_GetError());
}
static void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(std::string("Test failed: ") + what);
}
static Surface loadBmp(const fs::path& path) {
    Surface raw(SDL_LoadBMP(path.string().c_str()), SDL_FreeSurface);
    require(bool(raw), "Cannot load " + path.string());
    Surface converted(SDL_ConvertSurfaceFormat(raw.get(), SDL_PIXELFORMAT_RGBA32, 0), SDL_FreeSurface);
    require(bool(converted), "Convert image");
    return converted;
}
static double seconds() {
    return double(SDL_GetPerformanceCounter()) / double(SDL_GetPerformanceFrequency());
}
struct Region {
    SDL_Rect bounds{};
    SDL_Point anchor{};
    Texture light{nullptr, SDL_DestroyTexture};
};
struct App {
    Window window{nullptr, SDL_DestroyWindow};
    Renderer renderer{nullptr, SDL_DestroyRenderer};
    Texture map{nullptr, SDL_DestroyTexture};
    Texture panel{nullptr, SDL_DestroyTexture};
    std::vector<Region> regions;
    std::vector<std::string> names{"Ocean"};
    std::vector<std::uint8_t> ids;
    mapview::Interaction interaction;
    mapview::Troops troops;
    const std::array<SDL_Color,6> colors{{{166,48,46,255},{51,103,162,255},{ 60,120,79,255},{210,165,55,255},{132,83,149,255},{ 70, 70, 70,255}}};
    const std::array<std::string,6> colorNames{{"RED","BLUE","GREEN","GOLD","PURPLE","BLACK"}};
    std::string savePath;
    bool persistent=false;
    int width=0, height=0;
    bool running=true, mouseInside=false;

    App(const fs::path& assets, bool hidden) {
        std::ifstream list(assets / "territories.txt");
        if (!list) throw std::runtime_error("Missing territories.txt in " + assets.string());
        for (std::string line; std::getline(list,line);) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (!line.empty()) names.push_back(line);
        }
        if (names.size()!=43) throw std::runtime_error("Expected 42 territory names");
        auto artwork=loadBmp(assets/"map.bmp");
        auto mask=loadBmp(assets/"territories.bmp");
        width=artwork->w; height=artwork->h;
        if (mask->w!=width || mask->h!=height) throw std::runtime_error("Map/mask sizes differ");
        ids.resize(std::size_t(width)*height);
        std::array<int,43> counts{};
        for (int y=0;y<height;++y) {
            auto* row=static_cast<Uint8*>(mask->pixels)+y*mask->pitch;
            for (int x=0;x<width;++x) {
                const int id=row[x*4];
                if (id>42 || row[x*4+1]!=id || row[x*4+2]!=id)
                    throw std::runtime_error("Invalid territory mask pixel");
                ids[std::size_t(y)*width+x]=static_cast<Uint8>(id); ++counts[id];
            }
        }
        for (int id=1;id<=42;++id) if (!counts[id]) throw std::runtime_error("Empty territory: "+names[id]);
        window.reset(SDL_CreateWindow("Risk map", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      1400,800,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|
                                      (hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN)));
        require(bool(window), "Create window");
        SDL_SetWindowMinimumSize(window.get(),1000,640);
        persistent=!hidden;
        if(persistent) {
            char* pref=SDL_GetPrefPath("RiskMapCpp","RiskMap");
            if(pref) {savePath=std::string(pref)+"troops.txt"; SDL_free(pref);
                if(fs::exists(savePath) && !troops.load(savePath))
                    throw std::runtime_error("Invalid saved army file: "+savePath);
            }
        }
        renderer.reset(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC));
        if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE));
        require(bool(renderer), "Create renderer");
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
        panel.reset(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,300,800));
        require(bool(panel),"Create panel texture");
        map.reset(SDL_CreateTextureFromSurface(renderer.get(),artwork.get()));
        require(bool(map), "Create map texture");
        regions.resize(43);
        std::vector<int> clearance(ids.size(),0);
        for(int y=1;y<height-1;++y) for(int x=1;x<width-1;++x) {
            auto i=std::size_t(y)*width+x;
            if(ids[i]) clearance[i]=(ids[i]==ids[i-1] && ids[i]==ids[i-width]) ? 1+std::min(clearance[i-1],clearance[i-width]) : 1;
        }
        for(int y=height-2;y>0;--y) for(int x=width-2;x>0;--x) {
            auto i=std::size_t(y)*width+x;
            if(ids[i]) clearance[i]=std::min(clearance[i],(ids[i]==ids[i+1] && ids[i]==ids[i+width]) ? 1+std::min(clearance[i+1],clearance[i+width]) : 1);
        }
        for (int id=1;id<=42;++id) {
            int left=width,top=height,right=-1,bottom=-1;
            for (int y=0;y<height;++y) for(int x=0;x<width;++x) {
                if (ids[std::size_t(y)*width+x]!=id) continue;
                left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
            }
            auto& region=regions[id];region.bounds={left,top,right-left+1,bottom-top+1};
            int best=-1;
            for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) {
                auto i=std::size_t(y)*width+x;
                if(ids[i]==id && clearance[i]>best) {best=clearance[i];region.anchor={x,y};}
            }
            Surface lit(SDL_CreateRGBSurfaceWithFormat(0,region.bounds.w,region.bounds.h,32,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);
            require(bool(lit), "Create highlight surface");
            require(SDL_FillRect(lit.get(),nullptr,SDL_MapRGBA(lit->format,0,0,0,0))==0,"Clear highlight");
            for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) {
                if(ids[std::size_t(y)*width+x]!=id)continue;
                auto* src=static_cast<Uint8*>(artwork->pixels)+y*artwork->pitch+x*4;
                auto* dst=static_cast<Uint8*>(lit->pixels)+(y-top)*lit->pitch+(x-left)*4;
                for(int c=0;c<3;++c) dst[c]=static_cast<Uint8>(std::min(255.0,std::round(src[c]*(1.0+mapview::BrightnessGain))));
                dst[3]=255;
            }
            region.light.reset(SDL_CreateTextureFromSurface(renderer.get(),lit.get()));
            require(bool(region.light),"Create territory texture");
            require(SDL_SetTextureBlendMode(region.light.get(),SDL_BLENDMODE_BLEND)==0,"Enable highlight blending");
        }
    }
    mapview::Rect viewport() const {
        int w=0,h=0;require(SDL_GetRendererOutputSize(renderer.get(),&w,&h)==0,"Get output size");
        return mapview::fit(w-int(300*uiScale()),h,width,height);
    }
    float uiScale() const {
        int w,h; SDL_GetRendererOutputSize(renderer.get(),&w,&h);
        return std::min(w/1400.0f,h/800.0f);
    }
    SDL_Point physical(int x,int y) const {
        int ww,wh,rw,rh;SDL_GetWindowSize(window.get(),&ww,&wh);
        SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);
        return {ww>0?int(double(x)*rw/ww):0,wh>0?int(double(y)*rh/wh):0};
    }
    int hitWindow(int x,int y) const {
        auto p=physical(x,y); const auto v=viewport();
        // Medallions remain clickable even where their rim overlaps the coastline.
        for(int id=42;id>=1;--id) if(troops.board[id].count) {
            auto a=regions[id].anchor;
            int cx=v.x+a.x*v.w/width,cy=v.y+a.y*v.h/height;
            int radius=std::max(10,int(22*uiScale()));
            if((p.x-cx)*(p.x-cx)+(p.y-cy)*(p.y-cy)<=radius*radius) return id;
        }
        return mapview::hit(p.x,p.y,v,width,height,ids);
    }
    void save() {
        if(!savePath.empty()) {
            const std::string temporary=savePath+".tmp";
            std::error_code error;
            if(troops.save(temporary)) fs::rename(temporary,savePath,error);
            else error=std::make_error_code(std::errc::io_error);
            if(error) {troops.message="Could not save armies.";std::cerr<<troops.message<<" "<<error.message()<<'\n';}
            else troops.message="Armies saved.";
        }
    }
    bool panelClick(int x,int y) {
        auto p=physical(x,y);int rw,rh;SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);
        float scale=uiScale();float px=(p.x-(rw-300*scale))/scale,py=p.y/scale;
        if(px<0)return false;
        for(int i=0;i<6;++i) {
            int cx=50+(i%3)*100,cy=125+(i/3)*86;
            if(std::abs(px-cx)<42 && std::abs(py-cy)<38) {troops.color=i;return true;}
        }
        if(py>=423&&py<465) {
            if(px>=20&&px<65)troops.batch=std::max(1,troops.batch-1);
            if(px>=235&&px<280)troops.batch=std::min(999,troops.batch+1);
        }
        if(px>=20&&px<280) {
            if(py>=495&&py<539)troops.change(interaction.selected,1);
            if(py>=549&&py<593)troops.change(interaction.selected,-1);
            if(py>=603&&py<647)troops.change(interaction.selected,2);
            if(py>=657&&py<701)troops.change(interaction.selected,0);
        }
        return true;
    }
    void event(const SDL_Event& e,double now) {
        if(e.type==SDL_QUIT)running=false;
        if(e.type==SDL_MOUSEMOTION) {mouseInside=true;interaction.hover(hitWindow(e.motion.x,e.motion.y),now);}
        if(e.type==SDL_MOUSEBUTTONDOWN) {
            if(e.button.button==SDL_BUTTON_LEFT && panelClick(e.button.x,e.button.y))return;
            int id=hitWindow(e.button.x,e.button.y);
            if(e.button.button==SDL_BUTTON_LEFT) {interaction.select(id);troops.change(id,1);}
            if(e.button.button==SDL_BUTTON_RIGHT && id) {interaction.select(id);troops.change(id,-1);}
        }
        if(e.type==SDL_KEYDOWN) {
            auto key=e.key.keysym.sym;
            if(key==SDLK_ESCAPE) interaction.clear();
            if(key==SDLK_q)running=false;
            if(key>=SDLK_1&&key<=SDLK_6)troops.color=int(key-SDLK_1);
            if(key==SDLK_EQUALS||key==SDLK_PLUS||key==SDLK_KP_PLUS)troops.batch=std::min(999,troops.batch+1);
            if(key==SDLK_MINUS||key==SDLK_KP_MINUS)troops.batch=std::max(1,troops.batch-1);
            if(!e.key.repeat) {
                if(key==SDLK_z)troops.undo();
                if(key==SDLK_DELETE||key==SDLK_BACKSPACE)troops.change(interaction.selected,0);
                if(key==SDLK_s)save();
            }
        }
        if(e.type==SDL_WINDOWEVENT) {
            if(e.window.event==SDL_WINDOWEVENT_LEAVE || e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
                mouseInside=false;interaction.hover(0,now);
            }
            if(e.window.event==SDL_WINDOWEVENT_ENTER)mouseInside=true;
        }
    }
    void drawPanel() {
        int rw,rh;SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);
        float scale=uiScale();
        require(SDL_SetRenderTarget(renderer.get(),panel.get())==0,"Draw panel texture");
        const int x=0;
        auto* r=renderer.get();const SDL_Color gold{209,180,121,255},muted{154,165,161,255},white{235,228,209,255};
        ink::box(r,x,0,300,800,{24,33,36,255});
        ink::box(r,x,0,2,800,{133,110,69,255});
        ink::text(r,x+22,28,"ARMY PLACEMENT",2,gold);
        ink::text(r,x+22,68,"CHOOSE YOUR COLOR",1,muted);
        for(int i=0;i<6;++i) {
            int cx=x+50+(i%3)*100,cy=125+(i/3)*86;
            if(i==troops.color)ink::circle(r,cx,cy,31,{240,222,167,255});
            ink::medal(r,cx,cy,25,colors[i]);
            ink::centered(r,cx,cy-7,std::to_string(i+1),2,white);
            ink::centered(r,cx,cy+35,colorNames[i],1,i==troops.color?gold:muted);
        }
        ink::box(r,x+20,270,260,1,{83,86,74,255});
        ink::text(r,x+20,286,"SELECTED TERRITORY",1,muted);
        std::string name=interaction.selected?names[interaction.selected]:"NONE";
        // Wrap long territory names at spaces for a legible fixed-width panel.
        if(name.size()>21) {auto pos=name.rfind(' ',21);if(pos!=std::string::npos){ink::text(r,x+20,309,name.substr(0,pos),2,white);name=name.substr(pos+1);}}
        ink::text(r,x+20,332,name,2,white);
        auto a=troops.board[interaction.selected];
        ink::text(r,x+20,366,a.count?std::to_string(a.count)+" TROOPS - "+colorNames[a.color]:"NO TROOPS",1,gold);
        ink::text(r,x+20,402,"TROOPS PER CLICK",1,muted);
        ink::box(r,x+20,423,260,42,{41,52,54,255});
        ink::text(r,x+36,436,"-",2,white);ink::text(r,x+250,436,"+",2,white);
        ink::centered(r,x+150,436,std::to_string(troops.batch),2,gold);
        const std::array<std::string,4> labels{{"ADD TROOPS","REMOVE TROOPS","SET COLOR","CLEAR TERRITORY"}};
        for(int i=0;i<4;++i) {
            ink::box(r,x+20,495+i*54,260,44,i==0?SDL_Color{ 80, 80,55,255}:SDL_Color{43,54,55,255});
            ink::centered(r,x+150,510+i*54,labels[i],2,interaction.selected?white:muted);
        }
        ink::text(r,x+20,722,"LEFT CLICK: ADD   RIGHT: REMOVE",1,muted);
        ink::text(r,x+20,739,"1-6 COLOR   Z UNDO   S SAVE",1,muted);
        ink::text(r,x+20,768,troops.message,1,gold);
        require(SDL_SetRenderTarget(r,nullptr)==0,"Restore window target");
        const SDL_Rect target{rw-int(300*scale),0,int(300*scale),int(800*scale)};
        ink::box(r,target.x,0,target.w,rh,{24,33,36,255});
        require(SDL_RenderCopy(r,panel.get(),nullptr,&target)==0,"Render panel");
    }
    void draw(double now) {
        SDL_SetRenderDrawColor(renderer.get(),15,21,25,255);SDL_RenderClear(renderer.get());
        const auto v=viewport();const SDL_Rect dst{v.x,v.y,v.w,v.h};
        require(SDL_RenderCopy(renderer.get(),map.get(),nullptr,&dst)==0,"Draw map");
        for(int id: {interaction.selected,interaction.hovered}) {
            if(!id)continue;
            auto& region=regions[id]; const auto b=region.bounds;
            const int left=v.x+int(double(b.x)*v.w/width),top=v.y+int(double(b.y)*v.h/height);
            const int right=v.x+int(double(b.x+b.w)*v.w/width),bottom=v.y+int(double(b.y+b.h)*v.h/height);
            const SDL_Rect target{left,top,right-left,bottom-top};
            SDL_SetTextureAlphaMod(region.light.get(),static_cast<Uint8>(std::lround(255*interaction.amount(id,now))));
            require(SDL_RenderCopy(renderer.get(),region.light.get(),nullptr,&target)==0,"Draw highlight");
            if(interaction.selected==interaction.hovered)break;
        }
        for(int id=1;id<=42;++id) if(troops.board[id].count) {
            const auto a=regions[id].anchor;const auto army=troops.board[id];
            int cx=v.x+a.x*v.w/width,cy=v.y+a.y*v.h/height;
            int radius=std::max(10,int(22*uiScale()));
            SDL_SetRenderDrawBlendMode(renderer.get(),SDL_BLENDMODE_BLEND);
            ink::medal(renderer.get(),cx,cy,radius,colors[army.color]);
            int font=std::max(1,int(2*uiScale()));
            if(army.count>=1000)font=std::max(1,int(uiScale()));
            ink::centered(renderer.get(),cx+1,cy-3*font+1,std::to_string(army.count),font,{10,12,13,255});
            ink::centered(renderer.get(),cx,cy-3*font,std::to_string(army.count),font,{255,248,226,255});
        }
        drawPanel();
    }
    void saveFrame(const fs::path& path) {
        int w=0,h=0;require(SDL_GetRendererOutputSize(renderer.get(),&w,&h)==0,"Get output size");
        Surface frame(SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);
        require(bool(frame),"Create screenshot");
        require(SDL_RenderReadPixels(renderer.get(),nullptr,SDL_PIXELFORMAT_RGBA32,frame->pixels,frame->pitch)==0,"Read frame");
        require(SDL_SaveBMP(frame.get(),path.string().c_str())==0,"Save frame");
    }
    void test() {
        mapview::Interaction t;t.hover(12,10);
        check(t.amount(12,10)==0,"pulse begins unlit");
        check(std::abs(t.amount(12,11.4)-1)<1e-9,"pulse reaches full brightness at 1.4 seconds");
        check(std::abs(t.amount(12,12.8))<1e-9,"pulse returns at 2.8 seconds");
        t.select(12);t.hover(2,20);check(t.amount(12,100)==1,"selection persists after hover leaves");
        t.select(2);check(t.amount(12,100)==0,"new selection releases old one");t.clear();check(t.selected==0,"clear selection");
        const auto v=viewport();int ww=0,wh=0,rw=0,rh=0;SDL_GetWindowSize(window.get(),&ww,&wh);SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);
        // Exercise the same event handler used by the real SDL event loop.
        const int bx=int((v.x+475.0*v.w/width)*ww/rw),by=int((v.y+630.0*v.h/height)*wh/rh);
        check(hitWindow(bx,by)==12,"Brazil hit test including display scaling");
        SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.x=bx;e.motion.y=by;event(e,10);check(interaction.hovered==12,"mouse hover");
        e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=bx;e.button.y=by;event(e,11);check(interaction.selected==12,"left click selects");
        e={};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_LEAVE;event(e,12);check(interaction.hovered==0&&interaction.selected==12,"leaving preserves selection");
        draw(13);
        e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_RIGHT;event(e,14);check(interaction.selected==12,"right click on ocean preserves selection");
        check(hitWindow(0,0)==0,"map frame is not a territory");
        check(mapview::hit(-1,10,{0,0,width,height},width,height,ids)==0,"outside map");
        SDL_SetWindowSize(window.get(),900,900);check(viewport().y>0,"resize letterboxes map");
        draw(15);
        check(troops.board[12].count==1,"click placed one troop");
        e={};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_2;event(e,16);
        check(troops.color==1,"color keyboard shortcut");
        auto anchor=regions[12].anchor;
        auto vv=viewport();SDL_GetWindowSize(window.get(),&ww,&wh);SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);
        int tx=int((vv.x+anchor.x*vv.w/width)*double(ww)/rw),ty=int((vv.y+anchor.y*vv.h/height)*double(wh)/rh);
        e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=tx;e.button.y=ty;event(e,17);
        check(troops.board[12].color==0&&troops.board[12].count==1,"other color cannot silently overwrite army");
        const auto clickPanel=[&](int x,int y) {
            float scale=uiScale();SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;
            click.button.x=int((rw-300*scale+x*scale)*ww/rw);click.button.y=int(y*scale*wh/rh);event(click,18);
        };
        clickPanel(150,620);check(troops.board[12].color==1,"set color button");
        clickPanel(250,441);check(troops.batch==2,"increase batch button");
        event(e,19);check(troops.board[12].count==3,"map and medallion add batch");
        e.button.button=SDL_BUTTON_RIGHT;event(e,20);check(troops.board[12].count==1,"right click removes batch");
        clickPanel(150,675);check(troops.board[12].count==0&&troops.board[12].color==-1,"clear territory button");
        e={};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_z;event(e,21);check(troops.board[12].count==1,"undo clear");
        clickPanel(250,125);check(troops.color==2,"palette click selects green without placing troops");
        const auto temporary=fs::temp_directory_path()/("risk-map-test-"+std::to_string(SDL_GetPerformanceCounter())+".txt");
        check(troops.save(temporary.string()),"save board");mapview::Troops loaded;
        check(loaded.load(temporary.string())&&loaded.board[12].count==1&&loaded.board[12].color==1,"save and load preserve army");
        {std::ofstream bad(temporary);bad<<"RISK_TROOPS_V1\n1 99 -2\n";}
        check(!loaded.load(temporary.string())&&loaded.board[12].count==1,"malformed save rejected without partial load");
        fs::remove(temporary);
        loaded.board[12]={1,9999};loaded.color=1;check(!loaded.change(12,1),"upper troop limit");
        loaded.batch=999;loaded.board[12]={1,1};loaded.change(12,-1);
        check(loaded.board[12].count==0&&loaded.board[12].color==-1,"removal never goes negative");
        for(int id=1;id<=42;++id) {
            auto a=regions[id].anchor;check(ids[std::size_t(a.y)*width+a.x]==id,"medallion anchor belongs to its territory");
        }
        std::cout<<"PASS: placement, palette, recolor, batch controls, removal, undo, save/load, invalid saves and limits.\n";
        troops=mapview::Troops{};SDL_SetWindowSize(window.get(),1400,800);
        std::cout<<"PASS: 42 masks loaded; pulse timing, selection persistence, mouse events, bounds, resize and rendering.\n";
    }
};
int main(int argc,char**argv) {
    bool selfTest=false;fs::path assets,snapshot;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--self-test")selfTest=true;
        else if((arg=="--assets"||arg=="--snapshot")&&i+1<argc) {
            if(arg=="--assets")assets=argv[++i];else snapshot=argv[++i];
        } else {std::cerr<<"Usage: risk-map [--assets directory] [--self-test] [--snapshot output.bmp]\n";return 2;}
    }
    SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){std::cerr<<SDL_GetError()<<'\n';return 1;}
    int result=0;
    try {
        if(assets.empty()) {
            char* base=SDL_GetBasePath();if(base){assets=fs::path(base)/"assets";SDL_free(base);}
            if(!fs::exists(assets/"map.bmp"))assets=fs::current_path()/"assets";
        }
        App app(assets,selfTest||!snapshot.empty());
        if(selfTest)app.test();
        if(!snapshot.empty()) {
            for(int id: {1,7,12,16,20,21,25,28,35,38,41,42}) {app.troops.color=(id+2)%6;app.troops.batch=id+3;app.troops.change(id,1);}
            app.troops.color=2;app.troops.batch=3;app.interaction.select(12);app.draw(seconds());app.saveFrame(snapshot);
        }
        if(!selfTest&&snapshot.empty()) {
            while(app.running) {
                const double now=seconds();SDL_Event e{};while(SDL_PollEvent(&e))app.event(e,now);
                if(app.mouseInside) {int x=0,y=0;SDL_GetMouseState(&x,&y);app.interaction.hover(app.hitWindow(x,y),now);}
                const std::string title="Risk map | "+(app.interaction.hovered?app.names[app.interaction.hovered]:std::string("Hover a territory"))+
                    (app.interaction.selected?" | Selected: "+app.names[app.interaction.selected]:"");
                SDL_SetWindowTitle(app.window.get(),title.c_str());app.draw(now);SDL_RenderPresent(app.renderer.get());SDL_Delay(8);
            }
        }
        if(app.persistent)app.save();
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}
    SDL_Quit();return result;
}
