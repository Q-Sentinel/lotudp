# Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -I/opt/homebrew/include  # Add paths to include folders

# Source files
SRC = src/main.cpp src/server.cpp

# Object files
OBJ = $(SRC:.cpp=.o)

# Executable
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
