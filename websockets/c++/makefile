# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Iinclude -I/opt/homebrew/include  # Add paths to include folders

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source files and object files
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))

# Executable target
TARGET = server

# Build the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

# Create the object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Clean up build files
clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: clean
