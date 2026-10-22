
#pragma once

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <map>
#include <limits>

#include "bytes_buffer.hpp"

struct http11_header_parser {
    std::string m_header;
    std::string m_headline;
    std::string m_body;
    std::map<std::string, std::string> m_header_keys;
    bool m_header_finished = false;
    bool m_valid = true;

    void reset_state() {
        m_header.clear();
        m_headline.clear();
        m_body.clear();
        m_header_keys.clear();
        m_header_finished = false;
        m_valid = true;
    }

    bool header_finished() const { return m_header_finished; }

    static std::string trim(std::string_view value) {
        while (!value.empty() &&
               (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }

        while (!value.empty() &&
               (value.back() == ' ' || value.back() == '\t')) {
            value.remove_suffix(1);
        }

        return std::string(value);
    }

    static std::string lowercase(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))
            );
        }
        return s;
    }

    void _extract_headers() {
        const size_t first_end = m_header.find("\r\n");

        if (first_end == std::string::npos) {
            m_valid = false;
            return;
        }

        m_headline = m_header.substr(0, first_end);

        if (m_headline.empty()) {
            m_valid = false;
            return;
        }

        size_t pos = first_end + 2;

        while (pos < m_header.size()) {
            size_t end = m_header.find("\r\n", pos);

            if (end == std::string::npos) {
                end = m_header.size();
            }

            std::string_view line(m_header.data() + pos, end - pos);

            if (line.empty()) {
                pos = end + 2;
                continue;
            }

            // 不接受 obs-fold / continuation header。
            if (line.front() == ' ' || line.front() == '\t') {
                m_valid = false;
                return;
            }

            const size_t colon = line.find(':');

            if (colon == std::string_view::npos || colon == 0) {
                m_valid = false;
                return;
            }

            std::string key(line.substr(0, colon));

            for (unsigned char c : key) {
                if (!(std::isalnum(c) || c == '!' || c == '#' ||
                      c == '$' || c == '%' || c == '&' ||
                      c == '\'' || c == '*' || c == '+' ||
                      c == '-' || c == '.' || c == '^' ||
                      c == '_' || c == '`' || c == '|' ||
                      c == '~')) {
                    m_valid = false;
                    return;
                }
            }

            key = lowercase(std::move(key));

            const std::string value =
                trim(line.substr(colon + 1));

            // Content-Length 重复时不能静默覆盖。
            if (key == "content-length" &&
                m_header_keys.count(key)) {
                m_valid = false;
                return;
            }

            m_header_keys.insert_or_assign(key, value);

            if (end == m_header.size()) break;
            pos = end + 2;
        }
    }
};

struct http_request_parser {
    static constexpr size_t kMaxHeaderSize = 32 * 1024;
    static constexpr size_t kMaxBodySize = 1024 * 1024;

    http11_header_parser m_head_parser;
    size_t m_content_length = 0;
    bool m_body_finished = false;

    bool m_error = false;
    int m_error_status = 400;
    std::string m_error_message;

    void reset_state() {
        m_head_parser.reset_state();
        m_content_length = 0;
        m_body_finished = false;
        m_error = false;
        m_error_status = 400;
        m_error_message.clear();
    }

    bool request_finished() const { return m_body_finished; }
    bool has_error() const { return m_error; }
    int error_status() const { return m_error_status; }
    const std::string& error_message() const {
        return m_error_message;
    }

    std::map<std::string, std::string>& headers() noexcept {
        return m_head_parser.m_header_keys;
    }

    std::string& body() { return m_head_parser.m_body; }
    const std::string& body() const { return m_head_parser.m_body; }

    void fail(int status, std::string message) {
        m_error = true;
        m_error_status = status;
        m_error_message = std::move(message);
    }

    size_t _extract_content_length() {
        auto& h = m_head_parser.m_header_keys;

        // 该最小实现不支持 chunked 编码。
        if (h.count("transfer-encoding")) {
            fail(501, "Transfer-Encoding is not supported");
            return 0;
        }

        auto it = h.find("content-length");

        if (it == h.end()) return 0;

        const std::string& value = it->second;

        if (value.empty()) {
            fail(400, "Empty Content-Length");
            return 0;
        }

        size_t length = 0;

        const char* first = value.data();
        const char* last = first + value.size();

        auto result = std::from_chars(first, last, length);

        if (result.ec != std::errc{} || result.ptr != last) {
            fail(400, "Invalid Content-Length");
            return 0;
        }

        if (length > kMaxBodySize) {
            fail(413, "Request body too large");
            return 0;
        }

        return length;
    }

    bool keep_alive() const {
        const auto& h = m_head_parser.m_header_keys;
        auto it = h.find("connection");

        std::string version;
        const auto& line = m_head_parser.m_headline;
        const size_t p = line.rfind(' ');

        if (p != std::string::npos) {
            version = line.substr(p + 1);
        }

        bool alive = (version == "HTTP/1.1");

        if (it != h.end()) {
            std::string value = it->second;

            for (char& c : value) {
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c))
                );
            }

            if (value.find("close") != std::string::npos) {
                return false;
            }

            if (value.find("keep-alive") != std::string::npos) {
                alive = true;
            }
        }

        return alive;
    }

    std::string method() const {
        const auto& line = m_head_parser.m_headline;
        const size_t p = line.find(' ');

        return p == std::string::npos
            ? std::string{}
            : line.substr(0, p);
    }

    std::string url() const {
        const auto& line = m_head_parser.m_headline;
        const size_t p1 = line.find(' ');

        if (p1 == std::string::npos) return "/";

        const size_t p2 = line.find(' ', p1 + 1);

        if (p2 == std::string::npos) return "/";

        std::string target = line.substr(p1 + 1, p2 - p1 - 1);
        const size_t query_pos = target.find('?');

        if (query_pos != std::string::npos) {
            target.resize(query_pos);
        }

        return target;
    }

    std::string query() const {
        const auto& line = m_head_parser.m_headline;
        const size_t p1 = line.find(' ');

        if (p1 == std::string::npos) return "";

        const size_t p2 = line.find(' ', p1 + 1);

        if (p2 == std::string::npos) return "";

        const std::string target =
            line.substr(p1 + 1, p2 - p1 - 1);

        const size_t q = target.find('?');

        return q == std::string::npos
            ? ""
            : target.substr(q + 1);
    }

    // 返回本次调用从 chunk 消费的字节数。
    // 调用方必须保留未消费的剩余字节。
    size_t push_chunk(bytes_const_view chunk) {
        if (m_error || m_body_finished || chunk.size() ==0) {
            return 0;
        }

        size_t consumed = 0;

        if (!m_head_parser.header_finished()) {
            const size_t old_size = m_head_parser.m_header.size();

            std::string combined = m_head_parser.m_header;
            combined.append(chunk.data(), chunk.size());

            const size_t pos = combined.find("\r\n\r\n");

            if (pos == std::string::npos) {
                if (combined.size() > kMaxHeaderSize) {
                    fail(431, "Request headers too large");
                    return chunk.size();
                }

                m_head_parser.m_header = std::move(combined);
                return chunk.size();
            }

            if (pos + 4 > kMaxHeaderSize) {
                fail(431, "Request headers too large");
                return chunk.size();
            }

            consumed = pos + 4 - old_size;

            m_head_parser.m_header = combined.substr(0, pos);
            m_head_parser.m_header_finished = true;
            m_head_parser._extract_headers();

            if (!m_head_parser.m_valid) {
                fail(400, "Malformed request headers");
                return consumed;
            }

            // 检查请求行的基本格式。
            const auto& line = m_head_parser.m_headline;
            const size_t p1 = line.find(' ');
            const size_t p2 = p1 == std::string::npos
                ? std::string::npos
                : line.find(' ', p1 + 1);

            if (p1 == std::string::npos ||
                p1 == 0 ||
                p2 == std::string::npos ||
                p2 == p1 + 1) {
                fail(400, "Malformed request line");
                return consumed;
            }

            const std::string version = line.substr(p2 + 1);

            if (version != "HTTP/1.0" && version != "HTTP/1.1") {
                fail(400, "Unsupported HTTP version");
                return consumed;
            }

            m_content_length = _extract_content_length();

            if (m_error) return consumed;
        }

        const size_t available = chunk.size() - consumed;
        const size_t needed = m_content_length - m_head_parser.m_body.size();
        const size_t take = std::min(available, needed);

        if (take > 0) {
            m_head_parser.m_body.append(
                chunk.data() + consumed, take
            );
            consumed += take;
        }

        if (m_head_parser.m_body.size() == m_content_length) {
            m_body_finished = true;
        }

        return consumed;
    }
};
