CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET := ../bin/hw4
SRCS := main.cpp router.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p ../bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
