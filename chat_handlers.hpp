#pragma once

#include <string>
#include "Router.hpp"
#include "message_store.hpp"

// HTTP adapter for the chat business. Must outlive its registered route callbacks.
class ChatHandlers {
public:
    explicit ChatHandlers(MessageStore& store) : store_(store) {}

    void register_routes(Router& router) {
        router.get("/", [this](auto& q, auto& r) { handle_index(q, r); });
        router.get("/messages", [this](auto& q, auto& r) { handle_messages(q, r); });
        router.post("/send", [this](auto& q, auto& r) { handle_send(q, r); });
    }

private:
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
            "    const p=document.createElement('p');"
            "    p.textContent=m.text;"
            "    document.getElementById('l').appendChild(p);"
            "  }"
            "},1000);"
            "</script>";
    }

    void handle_send(http_request_parser& q, HttpResponse& r) {
        if (!q.body().empty()) store_.append(q.body());
        r.status = 302;
        r.reason = "Found";
        r.headers["Location"] = "/";
        r.body.clear();
    }

    void handle_messages(http_request_parser& q, HttpResponse& r) {
        size_t since = 0;
        const std::string query = q.query();
        if (auto p = query.find("since="); p != std::string::npos) {
            try { since = std::stoul(query.substr(p + 6)); } catch (...) {}
        }
        const auto messages = store_.since(since);
        std::string json = "[";
        bool first = true;
        for (const auto& message : messages) {
            if (!first) json += ',';
            first = false;
            json += "{\"id\":" + std::to_string(message.id) +
                    ",\"text\":\"" + escape_json(message.text) + "\"}";
        }
        json += "]";
        r.headers["Content-Type"] = "application/json";
        r.body = std::move(json);
    }

    static std::string escape_json(const std::string& s) {
        std::string o;
        for (unsigned char c : s) {
            switch (c) {
                case '"':  o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n";  break;
                case '\r': o += "\\r";  break;
                case '\t': o += "\\t";  break;
                default:
                    if (c < 0x20) {
                        constexpr char hex[] = "0123456789abcdef";
                        o += "\\u00";
                        o += hex[c >> 4];
                        o += hex[c & 0x0f];
                    } else {
                        o += static_cast<char>(c);
                    }
                    break;
            }
        }
        return o;
    }

    MessageStore& store_;
};
