#pragma once

#include <string>
#include <vector>

namespace Porkchop {

struct Descriptor {
    virtual ~Descriptor() = default;
    [[nodiscard]] virtual std::string_view descriptor() const noexcept = 0;
    [[nodiscard]] virtual std::vector<const Descriptor*> children() const { return {}; }
    [[nodiscard]] std::string walkDescriptor() const {
        int id = 0;
        std::string buf;
        walkDescriptor(buf, id);
        return buf;
    }

private:
    void walkDescriptor(std::string& buf, int& id) const {
        std::string pid = std::to_string(id);
        buf += pid;
        buf += "[\"";
        buf += descriptor();
        buf += "\"]\n";
        for (auto&& child : children()) {
            ++id;
            buf += pid + "-->" + std::to_string(id) + "\n";
            child->walkDescriptor(buf, id);
        }
    }
};

}