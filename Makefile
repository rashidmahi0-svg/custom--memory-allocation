CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
SRC      = src/allocator.cpp src/buddy_allocator.cpp src/leak_detector.cpp src/main.cpp
OBJ      = $(SRC:.cpp=.o)
TARGET   = allocator_demo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
