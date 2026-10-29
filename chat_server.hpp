// chat_server.hpp
#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include "connection.hpp"
#include "router.hpp"



inline std::string serialize_response(const HttpResponse& r) {
    std::string s;
    s.reserve(256 + r.body.size());
    s += "HTTP/1.1 " + std::to_string(r.status) + " " + r.reason + "\r\n";
    auto headers = r.headers;
    headers["Content-Length"] = std::to_string(r.body.size());
    if (!r.keep_alive) headers["Connection"] = "close";
    else               headers["Connection"] = "keep-alive";
    for (auto& [k, v] : headers) {
        s += k; s += ": "; s += v; s += "\r\n";
    }
    s += "\r\n";
    s += r.body;
    return s;
}



class ChatServer {
public:
    // 供外部（如 main 的连接表）挂接关闭通知，避免 close 回调被内部覆盖
    using CloseHook = std::function<void(const std::shared_ptr<Connection>&)>;
    void set_close_hook(CloseHook h) { close_hook_ = std::move(h); }

    ChatServer(Eventloop* loop):loop_(loop) {
        router_.get ("/",          [this](auto& q, auto& r){ handle_index(q, r); });
        router_.get ("/messages",  [this](auto& q, auto& r){ handle_messages(q, r); });
        router_.post("/send",      [this](auto& q, auto& r){ handle_send(q, r); });
    }


    
    
    void on_new_connection(std::shared_ptr<Connection> c){
    c->setMessageCallback([this](const std::shared_ptr<Connection>& conn){
        on_bytes(conn);
    });
    c->setCloseCallback([this](const std::shared_ptr<Connection>& conn){
        on_close(conn);
        if (close_hook_) close_hook_(conn);
    });
    if(!c->start())
    {   
        c->close();
    }
}  





private:
    void on_bytes(const std::shared_ptr<Connection>& c) {
        auto& parser = parser_of(c);   
        while(!c->input_buffer().empty()){
            size_t n = parser.push_chunk(c->input_buffer().const_span());
            c->input_buffer().consume(n);
          if (parser.has_error()) 
          {
            c->close();
            return;    
          }
          if(parser.request_finished()){
                HttpResponse resp;
                router_.dispath(parser, resp);
                resp.keep_alive = parser.keep_alive(); //关闭
                parser.reset_state();
                c->send(serialize_response(resp),!resp.keep_alive);
                if (c->closed() || !resp.keep_alive) return;
            }
          else{
                if(n != 0 && !c->input_buffer().empty()) continue;
                else    //这里即缓冲区空了的情况 
                    {
                            return;
                    }
            }   
           
        }
    }

    void handle_index(http_request_parser&, HttpResponse& r) {
        r.headers["Content-Type"] = "text/html; charset=utf-8";
        r.body =
            "<!doctype html><meta charset=utf-8><h1>Chat</h1>"
            "<input id=i><button onclick=\"send()\">send</button>"
            "<div id=l></div>"
            "<script>"
            "let since=0;"
            "async function send(){"
            "  await fetch('/send',{method:'POST',body:document.getElementById('i').value});"
            "}"
            "setInterval(async()=>{"
            "  const r=await fetch('/messages?since='+since);"
            "  for(const m of await r.json()){since=m.id;"
            "    document.getElementById('l').insertAdjacentHTML('beforeend','<p>'+m.text+'</p>');"
            "  }"
            "},1000);"
            "</script>";
    }

    void handle_send(http_request_parser& q, HttpResponse& r) {
        std::string text = q.body();
        if (!text.empty()) {
            std::lock_guard lk(mu_);
            msgs_.push_back({next_id_++, std::move(text)});
            if (msgs_.size() > 1000) msgs_.erase(msgs_.begin());
        }
        r.status = 302;
        r.reason = "Found";
        r.headers["Location"] = "/";
        r.body = "";
    }

    void handle_messages(http_request_parser& q, HttpResponse& r) {
        size_t since = 0;
        std::string query = q.query();
        if (auto p = query.find("since="); p != std::string::npos) {
            try { since = std::stoul(query.substr(p + 6)); } catch (...) {}
        }
        std::string json = "[";
        {
            std::lock_guard lk(mu_);
            bool first = true;
            for (auto& m : msgs_) {
                if (m.id <= since) continue;
                if (!first) json += ',';
                first = false;
                json += "{\"id\":" + std::to_string(m.id) +
                        ",\"text\":\"" + escape_json(m.text) + "\"}";
            }
        }
        json += "]";
        r.headers["Content-Type"] = "application/json";
        r.body = std::move(json);
    }

    void on_close(const std::shared_ptr<Connection>& c) {
        // 短轮询无状态；后续长轮询在此移除挂起请求
        std::lock_guard lk(mu_);
        parsers_.erase(c.get());
    }

    http_request_parser& parser_of(const std::shared_ptr<Connection>& c) {
        std::lock_guard lk(mu_);
        auto it = parsers_.find(c.get());
        if (it == parsers_.end())
            it = parsers_.emplace(c.get(), http_request_parser{}).first;
        return it->second;
    }

    static std::string escape_json(const std::string& s) {
        std::string o;
        for (char c : s) {
            switch (c) {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n";  break;
                case '\r': o += "\\r";  break;
                case '\t': o += "\\t";  break;
                default:   o += c;
            }
        }
        return o;
    }

    struct Msg { size_t id; std::string text; };

    Router router_;
    Eventloop* loop_;
    CloseHook close_hook_;
    std::mutex mu_;
    std::vector<Msg> msgs_;
    size_t next_id_ = 1;
    std::unordered_map<const Connection*, http_request_parser> parsers_;
};
