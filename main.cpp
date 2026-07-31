#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kBucketCount = 262147;
constexpr std::size_t kMaxKeyLength = 64;
constexpr const char *kHashFile = "hash.bin";
constexpr const char *kDataFile = "data.bin";

struct Record {
    std::uint32_t next;
    std::int32_t value;
    std::uint16_t key_len;
    std::uint8_t active;
    std::uint8_t padding;
    char key[kMaxKeyLength];
};

class FileStore {
public:
    FileStore() : heads_(kBucketCount, 0) {
        ensure_hash_file();
        ensure_data_file();
        load_heads();
        data_ = std::fopen(kDataFile, "r+b");
    }

    ~FileStore() {
        flush_heads();
        if (data_ != nullptr) {
            std::fclose(data_);
        }
    }

    void insert(const std::string &key, std::int32_t value) {
        const std::uint32_t bucket = hash_key(key);
        std::uint32_t offset = heads_[bucket];
        while (offset != 0) {
            const Record record = read_record(offset);
            if (record.active && record.value == value && key_equals(record, key)) {
                return;
            }
            offset = record.next;
        }

        Record record{};
        record.next = heads_[bucket];
        record.value = value;
        record.key_len = static_cast<std::uint16_t>(key.size());
        record.active = 1;
        std::copy(key.begin(), key.end(), record.key);

        std::fseek(data_, 0, SEEK_END);
        const auto new_offset = static_cast<std::uint32_t>(std::ftell(data_));
        std::fwrite(&record, sizeof(record), 1, data_);
        heads_[bucket] = new_offset;
    }

    void erase(const std::string &key, std::int32_t value) {
        const std::uint32_t bucket = hash_key(key);
        std::uint32_t offset = heads_[bucket];
        while (offset != 0) {
            Record record = read_record(offset);
            if (record.active && record.value == value && key_equals(record, key)) {
                record.active = 0;
                write_record(offset, record);
                return;
            }
            offset = record.next;
        }
    }

    std::vector<std::int32_t> find(const std::string &key) const {
        const std::uint32_t bucket = hash_key(key);
        std::vector<std::int32_t> values;
        std::uint32_t offset = heads_[bucket];
        while (offset != 0) {
            const Record record = read_record(offset);
            if (record.active && key_equals(record, key)) {
                values.push_back(record.value);
            }
            offset = record.next;
        }
        std::sort(values.begin(), values.end());
        return values;
    }

private:
    std::vector<std::uint32_t> heads_;
    FILE *data_ = nullptr;

    static std::uint32_t hash_key(const std::string &key) {
        std::uint64_t hash = 1469598103934665603ull;
        for (unsigned char ch : key) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        return static_cast<std::uint32_t>(hash % kBucketCount);
    }

    static bool key_equals(const Record &record, const std::string &key) {
        return record.key_len == key.size() && std::char_traits<char>::compare(record.key, key.data(), record.key_len) == 0;
    }

    static void ensure_hash_file() {
        const std::uintmax_t expected = static_cast<std::uintmax_t>(kBucketCount) * sizeof(std::uint32_t);
        if (std::filesystem::exists(kHashFile) && std::filesystem::file_size(kHashFile) == expected) {
            return;
        }
        FILE *file = std::fopen(kHashFile, "wb");
        std::uint32_t zero = 0;
        for (std::uint32_t i = 0; i < kBucketCount; ++i) {
            std::fwrite(&zero, sizeof(zero), 1, file);
        }
        std::fclose(file);
    }

    static void ensure_data_file() {
        if (!std::filesystem::exists(kDataFile) || std::filesystem::file_size(kDataFile) == 0) {
            FILE *file = std::fopen(kDataFile, "wb");
            const char sentinel = '\0';
            std::fwrite(&sentinel, 1, 1, file);
            std::fclose(file);
        }
    }

    void load_heads() {
        FILE *file = std::fopen(kHashFile, "rb");
        std::fread(heads_.data(), sizeof(std::uint32_t), heads_.size(), file);
        std::fclose(file);
    }

    void flush_heads() const {
        FILE *file = std::fopen(kHashFile, "wb");
        std::fwrite(heads_.data(), sizeof(std::uint32_t), heads_.size(), file);
        std::fclose(file);
    }

    Record read_record(std::uint32_t offset) const {
        Record record{};
        std::fseek(data_, static_cast<long>(offset), SEEK_SET);
        std::fread(&record, sizeof(record), 1, data_);
        return record;
    }

    void write_record(std::uint32_t offset, const Record &record) {
        std::fseek(data_, static_cast<long>(offset), SEEK_SET);
        std::fwrite(&record, sizeof(record), 1, data_);
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
        } else {
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
