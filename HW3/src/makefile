
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2

TARGET = ../bin/hw3

SRCS = main.cpp

HDRS = db.h parse_def.h parse_lef.h placer.h


all: $(TARGET)


$(TARGET): $(SRCS) $(HDRS)
	@echo "Compiling and linking..."
	@mkdir -p ../bin  # 確保 ../bin/ 資料夾存在
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)
	@echo "Done. Executable is at $(TARGET)"


clean:
	@echo "Cleaning up..."
	rm -f $(TARGET)  # 刪除 bin/hw3 執行檔
	@echo "Cleaned."


.PHONY: all clean