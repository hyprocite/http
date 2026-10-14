#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <string_view>

struct bytes_view
{
    char* m_data;
    size_t m_size;

    char* data() const noexcept{return m_data;}
    char* begin() const noexcept{return m_data;}
    char* end() const noexcept{return m_data + m_size;}
    size_t size() const noexcept{ return m_size;}

    bytes_view subspan(size_t start, size_t len = static_cast<size_t>(-1)) const
    {
        if(start > m_size) throw "error,over len";
        if(start + len > m_size) len = m_size - start;
        return {m_data + start ,len};
    

    }

        operator std::string_view() const noexcept {
            return std::string_view{m_data, m_size};
    }
};


struct bytes_const_view
{
    char const *m_data;
    size_t m_size;

    char const *data() const noexcept{return m_data;}
    char const *begin() const noexcept{return m_data;}
    char const *end() const noexcept{return m_data + m_size;}
    size_t size() const noexcept{ return m_size;}

    bytes_const_view subspan(size_t start, size_t len = static_cast<size_t>(-1)) const
    {
        if(start > m_size) throw "error,over len";
        if(start + len > m_size) len = m_size - start;
        return {m_data + start ,len};
    

    }

    operator std::string_view() const noexcept {
            return std::string_view{m_data, m_size};
    }
};


struct bytes_buffer
{
    std::vector<char> m_data;
    bytes_buffer() = default;
    //显示构造指定大小
    explicit bytes_buffer(size_t n) : m_data(n) {} 

    char* data() noexcept{return m_data.data();}
    char const *data()const noexcept {return m_data.data();}
    bool empty()const noexcept{return m_data.empty();}
    void resize(size_t n) noexcept{return m_data.resize(n);}
    void clear()noexcept{return m_data.clear();}

    void append(bytes_const_view chunk) {
        m_data.insert(m_data.end(), chunk.begin(), chunk.end());
    }

    void append(std::string_view sv) {
        m_data.insert(m_data.end(), sv.begin(), sv.end());
    }

    void consume(size_t n )
    {
        if(n > m_data.size()) m_data.clear();
        else
        {
            m_data.erase(m_data.begin(),m_data.begin()+ static_cast<std::ptrdiff_t>(n));
        }
    }


    bytes_view span() noexcept { return {m_data.data(), m_data.size()}; }
    bytes_const_view const_span() const noexcept { return {m_data.data(), m_data.size()}; }
    operator bytes_view() noexcept { return span(); }
    operator bytes_const_view() const noexcept { return const_span(); }
    operator std::string_view() const noexcept { return {m_data.data(), m_data.size()}; }
};