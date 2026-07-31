#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kBucketCount = 131071;
constexpr std::size_t kMaxKeyLength = 64;
constexpr const char *kHashFile = "hash.bin";
constexpr const char *kDataFile = "data.bin";

struct Record {
    std::uint64_t next;
    std::int32_t value;
    std::uint16_t key_len;
    std::uint8_t active;
    std::uint8_t padding;
    char key[kMaxKeyLength];
};

class FileStore {
public:
    FileStore() {
        ensure_hash_file();
        ensure_data_file();
        hash_ = std::fopen(kHashFile, "r+b");
        data_ = std::fopen(kDataFile, "r+b");
    }

    ~FileStore() {
        if (hash_ != nullptr) {
            std::fclose(hash_);
        }
        if (data_ != nullptr) {
            std::fclose(data_);
        }
    }

    void insert(const std::string &key, std::int32_t value) {
        const std::uint64_t bucket = hash_key(key);
        std::uint64_t offset = read_bucket_head(bucket);
        while (offset != 0) {
            const Record record = read_record(offset);
            if (record.active && record.value == value && key_equals(record, key)) {
                return;
            }
            offset = record.next;
        }

        Record record{};
        record.next = read_bucket_head(bucket);
        record.value = value;
        record.key_len = static_cast<std::uint16_t>(key.size());
        record.active = 1;
        std::copy(key.begin(), key.end(), record.key);

        std::fseek(data_, 0, SEEK_END);
        const std::uint64_t new_offset = static_cast<std::uint64_t>(std::ftell(data_));
        std::fwrite(&record, sizeof(record), 1, data_);
        write_bucket_head(bucket, new_offset);
    }

    void erase(const std::string &key, std::int32_t value) {
        const std::uint64_t bucket = hash_key(key);
        std::uint64_t offset = read_bucket_head(bucket);
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
        const std::uint64_t bucket = hash_key(key);
        std::vector<std::int32_t> values;
        std::uint64_t offset = read_bucket_head(bucket);
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
    FILE *hash_ = nullptr;
    FILE *data_ = nullptr;

    static std::uint64_t hash_key(const std::string &key) {
        std::uint64_t hash = 1469598103934665603ull;
        for (unsigned char ch : key) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        return hash % kBucketCount;
    }

    static bool key_equals(const Record &record, const std::string &key) {
        return record.key_len == key.size() && std::char_traits<char>::compare(record.key, key.data(), record.key_len) == 0;
    }

    static void ensure_hash_file() {
        if (std::filesystem::exists(kHashFile)) {
            return;
        }
        FILE *file = std::fopen(kHashFile, "wb");
        std::uint64_t zero = 0;
        for (std::uint64_t i = 0; i < kBucketCount; ++i) {
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

    std::uint64_t read_bucket_head(std::uint64_t bucket) const {
        std::uint64_t head = 0;
        std::fseek(hash_, static_cast<long>(bucket * sizeof(head)), SEEK_SET);
        std::fread(&head, sizeof(head), 1, hash_);
        return head;
    }

    void write_bucket_head(std::uint64_t bucket, std::uint64_t head) {
        std::fseek(hash_, static_cast<long>(bucket * sizeof(head)), SEEK_SET);
        std::fwrite(&head, sizeof(head), 1, hash_);
    }

    Record read_record(std::uint64_t offset) const {
        Record record{};
        std::fseek(data_, static_cast<long>(offset), SEEK_SET);
        std::fread(&record, sizeof(record), 1, data_);
        return record;
    }

    void write_record(std::uint64_t offset, const Record &record) {
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
