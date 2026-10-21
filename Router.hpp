#pragma once
#include <functional>
#include <string>
#include <vector>
#include <map>
#include "http_codec.hpp"


struct HttpResponse
{
    int status = 200;
    std::string reason = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    bool keep_alive = true;
};

struct Route
{
    using Handler = std::function<void(http_request_parser&,HttpResponse&)>;
    std::string method,path;
    Handler handler;
};

struct  Router
{
    using Handler = std::function<void(http_request_parser&,HttpResponse&)>;
    std::vector<Route> routes_;

    void get(std::string path , Handler h){add("GET",std::move(path),std::move(h));}
    void post(std::string path , Handler h){add("POST",std::move(path),std::move(h));}



    void add(std::string method , std::string path, Handler h )
    {
        routes_.push_back({std::move(method),std::move(path),std::move(h)});   
    }

    bool dispath(http_request_parser& req , HttpResponse& resp)const{
        std::string m = req.method();
        std::string u = req.url();
        for(auto&  r : routes_)
        {
            if(r.method != m) continue;
            if(r.path == u) {r.handler(req,resp); return true;}

        }
        //找不到转入分支
        resp.status = 404;
        resp.reason = "Not Found";
        resp.headers["Content-Type"] = "text/plain";
        resp.body = "404 Not Found";
        return false;
    }





};


