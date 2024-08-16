# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++11 -pthread -Wall -Wextra
LDFLAGS = -pthread

# Targets
SERVER = server
CLIENT = client

# Source files
SERVER_SRC = server.cpp
CLIENT_SRC = client.cpp

# Object files
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)

# Default target
all: $(SERVER) $(CLIENT)

# Link the server executable
$(SERVER): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJ) $(LDFLAGS)

# Link the client executable
$(CLIENT): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJ) $(LDFLAGS)

# # Compile server source file into object file
# $(SERVER_OBJ): $(SERVER_SRC)
# 	$(CXX) $(CXXFLAGS) -c $(SERVER_SRC) -o $(SERVER_OBJ)

# # Compile client source file into object file
# $(CLIENT_OBJ): $(CLIENT_SRC)
# 	$(CXX) $(CXXFLAGS) -c $(CLIENT_SRC) -o $(CLIENT_OBJ)

# Clean up build files
clean:
	rm -f $(SERVER) $(CLIENT) $(SERVER_OBJ) $(CLIENT_OBJ)

# Phony targets
.PHONY: all clean
