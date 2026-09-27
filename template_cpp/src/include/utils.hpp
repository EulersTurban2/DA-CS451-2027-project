#pragma once

#include <functional>
#include <utility>

namespace Utils {

    struct hashPair{
        std::size_t operator()(const std::pair<unsigned long, int>& p) const noexcept {
            auto hash1 = std::hash<unsigned long>{}(p.first);
            auto hash2 = std::hash<int>{}(p.second);
            return hash1 ^ (hash2 << 1);
        }
    };

};