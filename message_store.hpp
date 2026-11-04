#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Message {
    size_t id;
    std::string text;
};

// Owns message history; has no dependency on HTTP or connections.
class MessageStore {
public:
    explicit MessageStore(size_t capacity = 1000) : capacity_(capacity),max_bytes(524288), current_bytes(0){
        if (capacity_ == 0) {
            throw std::invalid_argument("message capacity must be positive");
        }
    }

    size_t append(std::string text) {
        std::lock_guard lock(mutex_);
        const size_t id = next_id_;
        if(text.size() > max_bytes || text.size() > 16384) return 0;
        size_t temp = text.size();
        messages_.push_back({id, std::move(text)});
        current_bytes += temp;
        ++next_id_;
        while(messages_.size() > capacity_ || current_bytes > max_bytes) 
        {   
            current_bytes -= messages_.begin()->text.size();
            messages_.pop_front();
            
        }
        return id;
    }

    // Returns an owned snapshot. Callers serialize it after releasing the lock.
    std::vector<Message> since(size_t id) const {
        std::lock_guard lock(mutex_);
        std::vector<Message> result;
        for (const auto& message : messages_) {
            if (message.id > id) result.push_back(message);
        }
        return result;
    }

private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<Message> messages_;
    size_t max_bytes ;
    size_t current_bytes ;
    size_t next_id_ = 1;
};
