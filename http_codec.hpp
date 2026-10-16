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
        std::string_view head = m_header;
        size_t pos = head.find("\r\n");
        m_headline = std::string(head.substr(0,pos));
        while(pos !=std::string::npos)
        {
            pos+=2;
           size_t next_pos = head.find("\r\n",pos);
           size_t content_length = std::string::npos;
           if(next_pos != std::string::npos)
           {
              content_length = next_pos - pos;
           } 
           std::string_view c_line = head.substr(pos,content_length);
           size_t dot = head.find(": ",pos);
           if(dot != std::string::npos)
           {
            std::string key(c_line.substr(0,dot));  //这里由于需要拷贝至map中所以必须使用string 而非string_view
            std::string_view value = c_line.substr(dot+2);
           //小写处理
            std::transform(key.begin(), key.end(), key.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
           //
            m_header_keys.insert_or_assign(std::move(key),value);
           }
           pos = next_pos;

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