#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kBucketCount = 131071;
constexpr const char *kHashFile = "hash.bin";
constexpr const char *kDataFile = "data.bin";

struct NodeHeader {
    std::uint64_t next;
    std::int32_t value;
    std::uint16_t key_len;
    std::uint8_t active;
    std::uint8_t padding;
};

class FileStore {
public:
    FileStore() { initialize(); }

    void insert(const std::string &key, std::int32_t value) {
        const std::uint64_t bucket = hash_key(key);
        std::uint64_t offset = read_bucket_head(bucket);
        while (offset != 0) {
            NodeHeader header = read_header(offset);
            if (header.active && header.value == value && read_key(offset, header.key_len) == key) {
                return;
            }
            offset = header.next;
        }
        NodeHeader header{};
        header.next = read_bucket_head(bucket);
        header.value = value;
        header.key_len = static_cast<std::uint16_t>(key.size());
        header.active = 1;
        data_.seekp(0, std::ios::end);
        const std::uint64_t new_offset = static_cast<std::uint64_t>(data_.tellp());
        data_.write(reinterpret_cast<const char *>(&header), sizeof(header));
        data_.write(key.data(), static_cast<std::streamsize>(key.size()));
        data_.flush();
        write_bucket_head(bucket, new_offset);
    }

    void erase(const std::string &key, std::int32_t value) {
        const std::uint64_t bucket = hash_key(key);
        std::uint64_t offset = read_bucket_head(bucket);
        while (offset != 0) {
            NodeHeader header = read_header(offset);
            if (header.active && header.value == value && read_key(offset, header.key_len) == key) {
                header.active = 0;
                write_header(offset, header);
                return;
            }
            offset = header.next;
        }
    }

    std::vector<std::int32_t> find(const std::string &key) {
        const std::uint64_t bucket = hash_key(key);
        std::vector<std::int32_t> values;
        std::uint64_t offset = read_bucket_head(bucket);
        while (offset != 0) {
            NodeHeader header = read_header(offset);
            if (header.active && read_key(offset, header.key_len) == key) {
                values.push_back(header.value);
            }
            offset = header.next;
        }
        std::sort(values.begin(), values.end());
        return values;
    }

private:
    std::fstream hash_;
    std::fstream data_;

    static std::uint64_t hash_key(const std::string &key) {
        std::uint64_t hash = 1469598103934665603ull;
        for (unsigned char ch : key) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        return hash % kBucketCount;
    }

    static std::streamoff active_offset(std::uint64_t node_offset) {
        return static_cast<std::streamoff>(node_offset + offsetof(NodeHeader, active));
    }

    static std::streamoff key_offset(std::uint64_t node_offset) {
        return static_cast<std::streamoff>(node_offset + sizeof(NodeHeader));
    }

    void initialize() {
        ensure_hash_file();
        ensure_data_file();
        hash_.open(kHashFile, std::ios::in | std::ios::out | std::ios::binary);
        data_.open(kDataFile, std::ios::in | std::ios::out | std::ios::binary);
    }

    void ensure_hash_file() {
        if (std::filesystem::exists(kHashFile)) {
            return;
        }
        std::ofstream out(kHashFile, std::ios::binary);
        std::uint64_t zero = 0;
        for (std::uint64_t i = 0; i < kBucketCount; ++i) {
            out.write(reinterpret_cast<const char *>(&zero), sizeof(zero));
        }
    }

    void ensure_data_file() {
        if (std::filesystem::exists(kDataFile)) {
            return;
        }
        std::ofstream out(kDataFile, std::ios::binary);
    }

    std::uint64_t read_bucket_head(std::uint64_t bucket) {
        std::uint64_t head = 0;
        hash_.seekg(static_cast<std::streamoff>(bucket * sizeof(head)));
        hash_.read(reinterpret_cast<char *>(&head), sizeof(head));
        return head;
    }

    void write_bucket_head(std::uint64_t bucket, std::uint64_t head) {
        hash_.seekp(static_cast<std::streamoff>(bucket * sizeof(head)));
        hash_.write(reinterpret_cast<const char *>(&head), sizeof(head));
        hash_.flush();
    }

    NodeHeader read_header(std::uint64_t offset) {
        NodeHeader header{};
        data_.seekg(static_cast<std::streamoff>(offset));
        data_.read(reinterpret_cast<char *>(&header), sizeof(header));
        return header;
    }

    void write_header(std::uint64_t offset, const NodeHeader &header) {
        data_.seekp(static_cast<std::streamoff>(offset));
        data_.write(reinterpret_cast<const char *>(&header), sizeof(header));
        data_.flush();
    }

    std::string read_key(std::uint64_t offset, std::uint16_t length) {
        std::string key(length, '\0');
        data_.seekg(key_offset(offset));
        data_.read(key.data(), static_cast<std::streamsize>(length));
        return key;
    }
};

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    FileStore store;
    int n = 0;
    std::cin >> n;
    for (int i = 0; i < n; ++i) {
        std::string command;
        std::string key;
        std::cin >> command >> key;
        if (command == "insert") {
            int value = 0;
            std::cin >> value;
            store.insert(key, value);
        } else if (command == "delete") {
            int value = 0;
            std::cin >> value;
            store.erase(key, value);
        } else if (command == "find") {
            const auto values = store.find(key);
            if (values.empty()) {
                std::cout << "null\n";
            } else {
                for (std::size_t j = 0; j < values.size(); ++j) {
                    if (j != 0) {
                        std::cout << ' ';
                    }
                    std::cout << values[j];
                }
                std::cout << '\n';
            }
        }
    }
    return 0;
}
