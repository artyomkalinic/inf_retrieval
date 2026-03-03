CC = g++
CFLAGS = -Wall -O2 -std=c++17
SRC = src
BUILD = build

COMMON_OBJS = $(BUILD)/tokenizer.o $(BUILD)/stemmer.o $(BUILD)/compress.o \
              $(BUILD)/index_builder.o $(BUILD)/index_reader.o $(BUILD)/search.o \
              $(BUILD)/query_parser.o $(BUILD)/tfidf.o

all: $(BUILD) indexer searcher tester

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tokenizer.o: $(SRC)/tokenizer.cpp $(SRC)/tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/stemmer.o: $(SRC)/stemmer.cpp $(SRC)/stemmer.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/compress.o: $(SRC)/compress.cpp $(SRC)/compress.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/index_builder.o: $(SRC)/index_builder.cpp $(SRC)/index_builder.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/index_reader.o: $(SRC)/index_reader.cpp $(SRC)/index_reader.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/search.o: $(SRC)/search.cpp $(SRC)/search.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/query_parser.o: $(SRC)/query_parser.cpp $(SRC)/query_parser.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/tfidf.o: $(SRC)/tfidf.cpp $(SRC)/tfidf.h
	$(CC) $(CFLAGS) -c $< -o $@

indexer: $(COMMON_OBJS) $(BUILD)/main_index.o
	$(CC) $(CFLAGS) $^ -o $(BUILD)/$@

$(BUILD)/main_index.o: $(SRC)/main_index.cpp
	$(CC) $(CFLAGS) -c $< -o $@

searcher: $(COMMON_OBJS) $(BUILD)/main_search.o
	$(CC) $(CFLAGS) $^ -o $(BUILD)/$@

$(BUILD)/main_search.o: $(SRC)/main_search.cpp
	$(CC) $(CFLAGS) -c $< -o $@

tester: $(COMMON_OBJS) $(BUILD)/main_test.o
	$(CC) $(CFLAGS) $^ -o $(BUILD)/$@

$(BUILD)/main_test.o: $(SRC)/main_test.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean tester
