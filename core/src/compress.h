#ifndef COMPRESS_H
#define COMPRESS_H

int vbyte_encode(unsigned int value, unsigned char* output);
int vbyte_decode(const unsigned char* input, unsigned int* value);
int vbyte_encode_array(const unsigned int* values, int count, unsigned char* output);
int vbyte_decode_array(const unsigned char* input, int count, unsigned int* values);
int vbyte_encode_delta(const unsigned int* sorted_ids, int count, unsigned char* output);
int vbyte_decode_delta(const unsigned char* input, int count, unsigned int* ids);

#endif
