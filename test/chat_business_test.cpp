#include "chat_handlers.hpp"

#include <cassert>
#include <iostream>
#include <thread>

HttpResponse request(Router& router, const std::string& method,
                     const std::string& target, const std::string& body = "") {
    const std::string wire = method + " " + target + " HTTP/1.1\r\nHost: test\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    http_request_parser parser;
    assert(parser.push_chunk({wire.data(), wire.size()}) == wire.size());
    assert(!parser.has_error() && parser.request_finished());
    HttpResponse response;
    router.dispath(parser, response);
    return response;
}

int main() {
    MessageStore store(2);
    assert(store.since(0).empty());
    assert(store.append("one") == 1);
    const auto snapshot = store.since(0);
    assert(store.append("two") == 2);
    assert(store.append("three") == 3);
    const auto retained = store.since(0);
    assert(retained.size() == 2 && retained[0].id == 2 && retained[1].id == 3);
    assert(snapshot.size() == 1 && snapshot[0].text == "one");
    assert(store.since(2).size() == 1);
    assert(store.since(3).empty());
    bool rejected = false;
    try { MessageStore invalid(0); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    MessageStore shared(1000);
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&] {
            for (int n = 0; n < 100; ++n) {
                shared.append("message");
                (void)shared.since(0);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    const auto concurrent = shared.since(0);
    assert(concurrent.size() == 400);
    for (size_t i = 0; i < concurrent.size(); ++i) assert(concurrent[i].id == i + 1);

    MessageStore messages;
    ChatHandlers handlers(messages);
    Router router;
    handlers.register_routes(router);
    assert(request(router, "GET", "/messages").body == "[]");
    assert(request(router, "POST", "/send").status == 302);
    assert(messages.since(0).empty());
    const auto sent = request(router, "POST", "/send", "hello");
    assert(sent.status == 302 && sent.headers.at("Location") == "/");
    const auto history = request(router, "GET", "/messages?since=0");
    assert(history.headers.at("Content-Type") == "application/json");
    assert(history.body == "[{\"id\":1,\"text\":\"hello\"}]");
    assert(request(router, "GET", "/messages?since=1").body == "[]");
    request(router, "POST", "/send", std::string("\"\\\n\r\t\b\f\0", 8));
    assert(request(router, "GET", "/messages?since=1").body ==
           "[{\"id\":2,\"text\":\"\\\"\\\\\\n\\r\\t\\u0008\\u000c\\u0000\"}]");
    assert(request(router, "GET", "/").body.find("p.textContent=m.text") != std::string::npos);
    assert(request(router, "GET", "/missing").status == 404);
    MessageStore isolated;
    ChatHandlers other_handlers(isolated);
    Router other_router;
    other_handlers.register_routes(other_router);
    assert(request(other_router, "GET", "/messages").body == "[]");
    std::cout << "Chat business tests passed\n";
}
