CXX = g++
CXXFLAGS = -std=c++17 -Wall -I../include

BIN_DIR = ../bin
DISK_DIR = ../disk
TEST_DIR = ../test

TARGET_MKFS = $(BIN_DIR)/mkfs
TARGET_TEST = $(BIN_DIR)/test_filesystem
TARGET_SNAPSHOT_TEST = $(BIN_DIR)/test_snapshot
TARGET_SNAPSHOT_TOOL = $(BIN_DIR)/snapshot_tool
TARGET_CACHE_TEST = $(BIN_DIR)/test_block_cache

SRC = disk.cpp inode.cpp directory.cpp path.cpp block_cache.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET_MKFS) $(TARGET_TEST) $(TARGET_SNAPSHOT_TEST) $(TARGET_SNAPSHOT_TOOL) $(TARGET_CACHE_TEST)

$(TARGET_MKFS): $(OBJ) ../scripts/mkfs.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_TEST): $(OBJ) $(TEST_DIR)/test_filesystem.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_SNAPSHOT_TEST): $(OBJ) $(TEST_DIR)/test_snapshot.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_SNAPSHOT_TOOL): $(OBJ) ../scripts/snapshot_tool.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_CACHE_TEST): $(OBJ) $(TEST_DIR)/test_block_cache.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../scripts/%.o: ../scripts/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o ../scripts/*.o $(TEST_DIR)/*.o
	rm -f $(TARGET_MKFS) $(TARGET_TEST) $(TARGET_SNAPSHOT_TEST) $(TARGET_SNAPSHOT_TOOL) $(TARGET_CACHE_TEST)

.PHONY: all clean