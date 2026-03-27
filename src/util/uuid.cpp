#include "cortex/util/uuid.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace cortex {

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist;

    auto r = [&]() { return dist(gen); };

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << r() << "-";
    ss << std::setw(4) << (r() & 0xFFFF) << "-";
    ss << std::setw(4) << ((r() & 0x0FFF) | 0x4000) << "-";
    ss << std::setw(4) << ((r() & 0x3FFF) | 0x8000) << "-";
    ss << std::setw(8) << r() << std::setw(4) << (r() & 0xFFFF);

    return ss.str();
}

} // namespace cortex
