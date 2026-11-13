#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>
namespace anvil {
struct GridPoint { int x=0, y=0; };
class NavigationGrid {
public:
    NavigationGrid(int w,int h) : w_(w),h_(h) {
        if (w_ <= 0 || h_ <= 0) { w_=h_=0; return; }
        const auto ww=static_cast<std::size_t>(w_), hh=static_cast<std::size_t>(h_);
        if (ww > static_cast<std::size_t>(std::numeric_limits<int>::max()) / hh)
            throw std::length_error("Navigation grid exceeds supported index range");
        blocked_.assign(ww*hh,false);
    }
    bool valid() const { return w_>0 && h_>0 && !blocked_.empty(); }
    bool block(int x,int y,bool b=true) { if(!inside(x,y))return false;blocked_[idx(x,y)]=b;return true; }
    bool walkable(int x,int y) const { return inside(x,y)&&!blocked_[idx(x,y)]; }
    bool findPath(GridPoint a,GridPoint b,std::vector<GridPoint>&out) const {
        out.clear(); if(!walkable(a.x,a.y)||!walkable(b.x,b.y))return false;
        const int n=static_cast<int>(blocked_.size()),start=static_cast<int>(idx(a.x,a.y)),goal=static_cast<int>(idx(b.x,b.y));
        std::vector<int> prev(static_cast<std::size_t>(n),-1),dist(static_cast<std::size_t>(n),std::numeric_limits<int>::max());
        using Q=std::pair<int,int>;std::priority_queue<Q,std::vector<Q>,std::greater<Q>>q;dist[start]=0;q.push({0,start});
        const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
        while(!q.empty()){auto[cost,u]=q.top();q.pop();if(cost!=dist[u])continue;if(u==goal)break;int x=u%w_,y=u/w_;
            for(int k=0;k<4;++k){int nx=x+dx[k],ny=y+dy[k];if(!walkable(nx,ny))continue;int v=static_cast<int>(idx(nx,ny));if(cost+1<dist[v]){dist[v]=cost+1;prev[v]=u;q.push({dist[v],v});}}}
        if(dist[goal]==std::numeric_limits<int>::max())return false;
        for(int u=goal;u!=-1;u=prev[u])out.push_back({u%w_,u/w_});std::reverse(out.begin(),out.end());return true;
    }
private:
    bool inside(int x,int y)const{return x>=0&&y>=0&&x<w_&&y<h_;}
    std::size_t idx(int x,int y)const{return static_cast<std::size_t>(y)*static_cast<std::size_t>(w_)+static_cast<std::size_t>(x);}
    int w_=0,h_=0;std::vector<bool>blocked_;
};
} // namespace anvil
