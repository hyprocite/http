#pragma once
#include<string>
#include<string_view>
#include "bytes_buffer.hpp"
#include<map>
#include<algorithm>
#include<cctype>

struct http11_header_parser
{
    std::string m_header;
    std::string m_headline;
    std::string m_body;
    std::map<std::string,std::string> m_header_keys;
    bool m_header_finished = false;;

    void reset_state() {
        m_header.clear();
        m_headline.clear();
        m_header_keys.clear();
        m_body.clear();
        m_header_finished = false;
    }

    bool header_finished() const {return m_header_finished;}    

    void _extract_headers()
    {
        std::string_view header = m_header;
    size_t pos = header.find("\r\n");
    if (pos == std::string_view::npos) return;
    
    // 1. 提取请求行
    m_headline = std::string(header.substr(0, pos));
    pos += 2; // 跳过请求行的 \r\n，来到第一个 Header 键值对的开头

    // 2. 安全地循环解析每一行 Header
    while (pos < header.size()) {
        size_t next_pos = header.find("\r\n", pos);
        
        // 计算当前行的长度，如果找不到 \r\n，就说明这是最后一行，直接截取到末尾
        size_t line_len = (next_pos == std::string_view::npos) ? std::string_view::npos : (next_pos - pos);
        
        std::string_view line = header.substr(pos, line_len);
        size_t colon = line.find(": ");
        if (colon != std::string_view::npos) {
            std::string key(line.substr(0, colon));
            std::string_view value = line.substr(colon + 2);
            
            // 转小写
            std::transform(key.begin(), key.end(), key.begin(),
                           [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
            m_header_keys.insert_or_assign(std::move(key), value);
        }
        
        // 【关键防御】：如果后面没有 \r\n 了，说明全部解析完毕，立刻打破循环
        if (next_pos == std::string_view::npos) {
            break;
        }
        
        // 正常跳到下一行的开头（跳过当前行的 \r\n）
        pos = next_pos + 2; 


    }
}
    void _push_chunk(bytes_const_view chunk)
    {
        if(!m_header_finished)
        {
         m_header.append(chunk.data(),chunk.size());
         size_t pos =   m_header.find("\r\n\r\n");
         if(pos != std::string::npos)
         {
            m_header_finished = true;
            m_body = m_header.substr(pos + 4);
            m_header.resize(pos);
            _extract_headers();
         }   
        }
    }
};




struct http_request_parser
{
    http11_header_parser m_head_parser;
    size_t m_content_length = 0;
    bool m_body_finished = false;

    void reset_state() {    
        m_head_parser.reset_state();
        m_content_length = 0;
        m_body_finished = false;
    }

    //获取标志位
    bool request_finished() { return m_body_finished;}
     //获取header键值对
    std::map<std::string,std::string> &headers() noexcept {return m_head_parser.m_header_keys;}
    //获取body 
    std::string &body() {return m_head_parser.m_body;}

    std::string query() const
    {
        auto& hl = m_head_parser.m_headline;
        size_t p1 = hl.find(' ');
        if (p1 == std::string::npos) return "";
        size_t p2 = hl.find(' ', p1 + 1);
        std::string target = hl.substr(p1 + 1, p2 - (p1 + 1));
        size_t q = target.find('?');
        return q == std::string::npos ? "" : target.substr(q + 1);
    }
    

    std::string url () const
    {
        auto &hl = m_head_parser.m_headline;
        size_t p1 = hl.find(' ');
        if(p1 == std::string::npos) return "/";
        size_t p2 = hl.find(' ',p1+1);
        return hl.substr(p1+1 , p2 - (p1+1));
    }

    std::string method() const
    {
        auto &hm = m_head_parser.m_headline;
        size_t p = hm.find(' ');
        return p == std::string::npos ? "GET" : hm.substr(0, p);
    }

    size_t _extract_content_length()
    {
        auto &hdrs = m_head_parser.m_header_keys;
        auto it = hdrs.find("content-length");
        if(it == hdrs.end())return 0;
        try{
            return std::stoul(it->second);
        }
        catch(...)
        {
            return 0;
        }
    }

    void push_chunk(bytes_const_view chunk)
    {
        if(!m_head_parser.header_finished())
        {
            // 若还没读取到header的尾部，交由headerparser继续push
            m_head_parser._push_chunk(chunk);
            if(m_head_parser.header_finished())
            {
                m_content_length = _extract_content_length();
                if(body().size()>=m_content_length) 
                {
                    m_body_finished = true;
                }
            }
        }
        else
        {
        body().append(chunk.data(), chunk.size()); 
            // 追加完新数据后，立刻检查当前总长度是否已经达到了预期的 Content-Length
        if(body().size() >= m_content_length)
            {
                m_body_finished = true;
            }

        }
    }

};