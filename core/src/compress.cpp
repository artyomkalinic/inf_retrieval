#include "compress.h"

int vbyte_encode(unsigned int value, unsigned char* output) {
    int bytes_written = 0;
    while (value >= 128) {
        output[bytes_written] = (unsigned char)(value & 0x7F) | 0x80;
        value >>= 7;
        bytes_written++;
    }
    output[bytes_written] = (unsigned char)(value & 0x7F);
    bytes_written++;
    return bytes_written;
}

int vbyte_decode(const unsigned char* input, unsigned int* value) {
    *value = 0;
    int shift = 0;
    int bytes_read = 0;

    while (true) {
        unsigned char b = input[bytes_read];
        *value |= (unsigned int)(b & 0x7F) << shift;
        bytes_read++;
        if ((b & 0x80) == 0) {
            break;
        }
        shift += 7;
    }
    return bytes_read;
}

int vbyte_encode_array(const unsigned int* values, int count,
                       unsigned char* output) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += vbyte_encode(values[i], output + total);
    }
    return total;
}

int vbyte_decode_array(const unsigned char* input, int count,
                       unsigned int* values) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += vbyte_decode(input + total, &values[i]);
    }
    return total;
}

int vbyte_encode_delta(const unsigned int* sorted_ids, int count,
                       unsigned char* output) {
    int total = 0;
    unsigned int prev = 0;
    for (int i = 0; i < count; i++) {
        unsigned int delta = sorted_ids[i] - prev;
        total += vbyte_encode(delta, output + total);
        prev = sorted_ids[i];
    }
    return total;
}

int vbyte_decode_delta(const unsigned char* input, int count,
                       unsigned int* ids) {
    int total = 0;
    unsigned int prev = 0;
    for (int i = 0; i < count; i++) {
        unsigned int delta;
        total += vbyte_decode(input + total, &delta);
        ids[i] = prev + delta;
        prev = ids[i];
    }
    return total;
}
