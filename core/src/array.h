#ifndef ARRAY_H
#define ARRAY_H

#include <cstdlib>
#include <cstring>

class DynArray {
public:
    char* data;
    int elem_size;
    int count;
    int capacity;

    DynArray(int element_size) {
        elem_size = element_size;
        count = 0;
        capacity = 16;
        data = (char*)malloc(capacity * elem_size);
    }

    ~DynArray() {
        free(data);
    }

    void push(const void* element) {
        if (count == capacity) {
            resize(capacity * 2);
        }
        memcpy(data + count * elem_size, element, elem_size);
        count++;
    }

    void* at(int index) {
        return data + index * elem_size;
    }

    void set(int index, const void* element) {
        memcpy(data + index * elem_size, element, elem_size);
    }

    int getCount() {
        return count;
    }

    void clear() {
        count = 0;
    }

    void resize(int new_capacity) {
        capacity = new_capacity;
        data = (char*)realloc(data, capacity * elem_size);
    }

    void removeLast() {
        if (count > 0) {
            count--;
        }
    }

    void sort(int (*compare)(const void*, const void*)) {
        if (count <= 1) {
            return;
        }
        quicksort(0, count - 1, compare);
    }

private:
    void swap(int i, int j) {
        char* tmp = (char*)malloc(elem_size);
        char* a = data + i * elem_size;
        char* b = data + j * elem_size;
        memcpy(tmp, a, elem_size);
        memcpy(a, b, elem_size);
        memcpy(b, tmp, elem_size);
        free(tmp);
    }

    void quicksort(int lo, int hi, int (*compare)(const void*, const void*)) {
        if (lo >= hi) {
            return;
        }

        int mid = lo + (hi - lo) / 2;
        char* pivot = (char*)malloc(elem_size);
        memcpy(pivot, data + mid * elem_size, elem_size);

        int i = lo;
        int j = hi;

        while (i <= j) {
            while (compare(data + i * elem_size, pivot) < 0) {
                i++;
            }
            while (compare(data + j * elem_size, pivot) > 0) {
                j--;
            }
            if (i <= j) {
                swap(i, j);
                i++;
                j--;
            }
        }

        free(pivot);

        quicksort(lo, j, compare);
        quicksort(i, hi, compare);
    }
};

#endif
