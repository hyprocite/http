// chat_server.hpp
#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "connection.hpp"
#include "chat_handlers.hpp"


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

    explicit ChatServer(MessageStore& store) : handlers_(store) {
        handlers_.register_routes(router_);
    }

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;

    void on_new_connection(std::shared_ptr<Connection> c) {
        c->setMessageCallback([this](const std::shared_ptr<Connection>& conn) {
            on_bytes(conn);
        });
        c->setCloseCallback([this](const std::shared_ptr<Connection>& conn) {
            on_close(conn);
            if (close_hook_) close_hook_(conn);
        });
        if (!c->start()) c->close();
    }

private:
    void on_bytes(const std::shared_ptr<Connection>& c) {
        auto& parser = parser_of(c);
        while (!c->input_buffer().empty()) {
            const size_t n = parser.push_chunk(c->input_buffer().const_span());
            c->input_buffer().consume(n);
            if (parser.has_error()) {
                c->close();
                return;
            }
            if (parser.request_finished()) {
                HttpResponse resp;
                router_.dispath(parser, resp);
                resp.keep_alive = parser.keep_alive();
                parser.reset_state();
                c->send(serialize_response(resp), !resp.keep_alive);
                if (c->closed() || !resp.keep_alive) return;
            } else if (n == 0 || c->input_buffer().empty()) {
                return;
            }
        }
    }

    void on_close(const std::shared_ptr<Connection>& c) {
        // 短轮询无状态；后续长轮询在此移除挂起请求
        parsers_.erase(c.get());
    }

    http_request_parser& parser_of(const std::shared_ptr<Connection>& c) {
        auto it = parsers_.find(c.get());
        if (it == parsers_.end())
            it = parsers_.emplace(c.get(), http_request_parser{}).first;
        return it->second;
    }

    // Connection/parser state belongs to the event-loop thread.
    ChatHandlers handlers_;
    Router router_;
    CloseHook close_hook_;
    std::unordered_map<const Connection*, http_request_parser> parsers_;
};
