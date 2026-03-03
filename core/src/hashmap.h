#ifndef HASHMAP_H
#define HASHMAP_H

#include <cstdlib>
#include <cstring>

struct HashEntry {
    char* key;
    int key_len;
    unsigned int value;
    unsigned char flags;
};

class HashMap {
public:
    HashEntry* buckets;
    int capacity;
    int count;

    HashMap() {
        capacity = 256;
        count = 0;
        buckets = (HashEntry*)malloc(capacity * sizeof(HashEntry));
        for (int i = 0; i < capacity; i++) {
            buckets[i].key = nullptr;
            buckets[i].key_len = 0;
            buckets[i].value = 0;
            buckets[i].flags = 0;
        }
    }

    HashMap(int initial_capacity) {
        capacity = initial_capacity;
        count = 0;
        buckets = (HashEntry*)malloc(capacity * sizeof(HashEntry));
        for (int i = 0; i < capacity; i++) {
            buckets[i].key = nullptr;
            buckets[i].key_len = 0;
            buckets[i].value = 0;
            buckets[i].flags = 0;
        }
    }

    ~HashMap() {
        for (int i = 0; i < capacity; i++) {
            if (buckets[i].flags == 1) {
                free(buckets[i].key);
            }
        }
        free(buckets);
    }

    void put(const char* key, unsigned int value) {
        if (count * 10 > capacity * 7) {
            growAndRehash();
        }

        int len = 0;
        while (key[len] != '\0') {
            len++;
        }

        int slot = findSlot(key, len);

        if (buckets[slot].flags == 1) {
            buckets[slot].value = value;
            return;
        }

        buckets[slot].key = (char*)malloc(len + 1);
        memcpy(buckets[slot].key, key, len);
        buckets[slot].key[len] = '\0';
        buckets[slot].key_len = len;
        buckets[slot].value = value;
        buckets[slot].flags = 1;
        count++;
    }

    bool get(const char* key, unsigned int* out_value) {
        int len = 0;
        while (key[len] != '\0') {
            len++;
        }

        unsigned int hash = murmur3(key, len);
        int idx = hash % capacity;

        for (int i = 0; i < capacity; i++) {
            int slot = (idx + i) % capacity;

            if (buckets[slot].flags == 0) {
                return false;
            }

            if (buckets[slot].flags == 1 &&
                buckets[slot].key_len == len &&
                memcmp(buckets[slot].key, key, len) == 0) {
                *out_value = buckets[slot].value;
                return true;
            }
        }
        return false;
    }

    bool contains(const char* key) {
        unsigned int dummy;
        return get(key, &dummy);
    }

    void remove(const char* key) {
        int len = 0;
        while (key[len] != '\0') {
            len++;
        }

        unsigned int hash = murmur3(key, len);
        int idx = hash % capacity;

        for (int i = 0; i < capacity; i++) {
            int slot = (idx + i) % capacity;

            if (buckets[slot].flags == 0) {
                return;
            }

            if (buckets[slot].flags == 1 &&
                buckets[slot].key_len == len &&
                memcmp(buckets[slot].key, key, len) == 0) {
                free(buckets[slot].key);
                buckets[slot].key = nullptr;
                buckets[slot].key_len = 0;
                buckets[slot].flags = 2;
                count--;
                return;
            }
        }
    }

    int getCount() {
        return count;
    }

private:
    unsigned int murmur3(const char* key, int len) {
        unsigned int seed = 42;
        unsigned int c1 = 0xcc9e2d51;
        unsigned int c2 = 0x1b873593;
        unsigned int h = seed;

        int nblocks = len / 4;
        const unsigned char* data = (const unsigned char*)key;

        for (int i = 0; i < nblocks; i++) {
            unsigned int k =
                (unsigned int)data[i * 4 + 0]       |
                (unsigned int)data[i * 4 + 1] << 8  |
                (unsigned int)data[i * 4 + 2] << 16 |
                (unsigned int)data[i * 4 + 3] << 24;

            k *= c1;
            k = (k << 15) | (k >> 17);
            k *= c2;

            h ^= k;
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        }

        const unsigned char* tail = data + nblocks * 4;
        unsigned int k1 = 0;

        switch (len & 3) {
            case 3: k1 ^= (unsigned int)tail[2] << 16;
            case 2: k1 ^= (unsigned int)tail[1] << 8;
            case 1: k1 ^= (unsigned int)tail[0];
                    k1 *= c1;
                    k1 = (k1 << 15) | (k1 >> 17);
                    k1 *= c2;
                    h ^= k1;
        }

        h ^= (unsigned int)len;

        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;

        return h;
    }

    int findSlot(const char* key, int len) {
        unsigned int hash = murmur3(key, len);
        int idx = hash % capacity;
        int first_deleted = -1;

        for (int i = 0; i < capacity; i++) {
            int slot = (idx + i) % capacity;

            if (buckets[slot].flags == 0) {
                return (first_deleted != -1) ? first_deleted : slot;
            }

            if (buckets[slot].flags == 2 && first_deleted == -1) {
                first_deleted = slot;
            }

            if (buckets[slot].flags == 1 &&
                buckets[slot].key_len == len &&
                memcmp(buckets[slot].key, key, len) == 0) {
                return slot;
            }
        }

        return first_deleted;
    }

    void growAndRehash() {
        int old_capacity = capacity;
        HashEntry* old_buckets = buckets;

        capacity = old_capacity * 2;
        buckets = (HashEntry*)malloc(capacity * sizeof(HashEntry));
        for (int i = 0; i < capacity; i++) {
            buckets[i].key = nullptr;
            buckets[i].key_len = 0;
            buckets[i].value = 0;
            buckets[i].flags = 0;
        }

        count = 0;
        for (int i = 0; i < old_capacity; i++) {
            if (old_buckets[i].flags == 1) {
                put(old_buckets[i].key, old_buckets[i].value);
                free(old_buckets[i].key);
            }
        }

        free(old_buckets);
    }
};

#endif
