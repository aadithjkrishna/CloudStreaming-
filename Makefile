CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

TARGET = server

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o bin/$(TARGET)

bin/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./bin/$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

files: tests/files.cpp
	g++ tests/files.cpp -o tests/files && ./tests/files