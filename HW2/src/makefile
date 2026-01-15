# ====== config ======
CXX       := g++
CXXFLAGS  := -std=c++17 -O3 -Wall -Wextra
SRC_DIR   := .
BIN_DIR   := ../bin
TARGET    := $(BIN_DIR)/hw2

# 自動搜集所有 .cpp
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)

# 你的標頭（有改動檔名就調整這行或乾脆拿掉）
DEPS := 4way.h bucket.h initial_partition.h inst.h parse.h write.h

# ====== rules ======
all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)
