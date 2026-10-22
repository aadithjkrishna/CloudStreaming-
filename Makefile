CXX       := g++
TARGET    := bin/server

# Local project configuration
SRC_DIR   := src
OBJ_DIR   := obj
BIN_DIR   := bin
TEST_DIR  := tests

# Compiler and linker configuration
CPPFLAGS  += -Iinclude
CXXFLAGS  += -std=c++17 -Wall -Wextra -MMD -MP
LDLIBS    += -ltag

# Remote deployment configuration
REMOTE     ?= aadith@100.120.9.99
REMOTE_DIR ?= MusicServer
SSH        ?= ssh
RSYNC      ?= rsync
JOBS       ?= 4
TMUX_SESSION ?= musicserver

# Discover source files and corresponding object files
SRC       := $(wildcard $(SRC_DIR)/*.cpp)
OBJ       := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
DEP       := $(OBJ:.o=.d)

.PHONY: all run clean files audio cloud cloud-run cloud-attach \
        cloud-logs cloud-stop cloud-status cloud-clean
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LDFLAGS) $(OBJ) -o $@ $(LDLIBS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Include generated header dependencies
-include $(DEP)

# Run locally
run: $(TARGET)
	./$(TARGET)

# Clean local build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f $(TEST_DIR)/files $(TEST_DIR)/audioData

# Existing test utilities
files: $(TEST_DIR)/files.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(TEST_DIR)/files
	./$(TEST_DIR)/files

audio: $(TEST_DIR)/audioData.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(TEST_DIR)/audioData -ltag
	./$(TEST_DIR)/audioData ./audios/"Uyiril_Thodum.flac"

# Sync the project, then build on the remote machine
cloud:
	@command -v $(RSYNC) >/dev/null || \
		{ echo "Error: rsync is not installed locally"; exit 1; }
	@echo "==> Preparing remote directory..."
	$(SSH) $(REMOTE) 'mkdir -p "$(REMOTE_DIR)"'
	@echo "==> Syncing project to $(REMOTE):$(REMOTE_DIR)/..."
	$(RSYNC) -az \
		--exclude='/.git/' \
		--exclude='/obj/' \
		--exclude='/bin/' \
		--exclude='/tests/files' \
		--exclude='/tests/audioData' \
		./ $(REMOTE):$(REMOTE_DIR)/
	@echo "==> Building on remote server..."
	$(SSH) $(REMOTE) 'cd "$(REMOTE_DIR)" && make -j$(JOBS)'
	@echo "==> Remote build complete."

# Sync code only, then build on the remote machine
cloud:
	@command -v $(RSYNC) >/dev/null || \
		{ echo "Error: rsync is not installed locally"; exit 1; }
	@echo "==> Preparing remote directory..."
	$(SSH) $(REMOTE) 'mkdir -p "$(REMOTE_DIR)"'

	@echo "==> Syncing source code only..."
	$(RSYNC) -az \
		--exclude='/.git/' \
		--exclude='/obj/' \
		--exclude='/bin/' \
		--exclude='/assets/' \
		--exclude='/audios/' \
		--exclude='/covers/' \
		--exclude='/cover_images/' \
		--exclude='/images/' \
		--exclude='/assets/' \
		--exclude='/tests/files' \
		--exclude='/tests/audioData' \
		--include='/Makefile' \
		--include='/src/***' \
		--include='/include/***' \
		--include='/tests/***' \
		--include='/*.mk' \
		--include='/*.cmake' \
		--include='/CMakeLists.txt' \
		--exclude='*' \
		./ $(REMOTE):$(REMOTE_DIR)/

	@echo "==> Building on remote server..."
	$(SSH) $(REMOTE) 'cd "$(REMOTE_DIR)" && make -j$(JOBS)'
	@echo "==> Remote build complete."
# Attach to the running server's tmux session
cloud-attach:
	$(SSH) -t $(REMOTE) 'tmux attach-session -t "$(TMUX_SESSION)"'

# Show recent server output
cloud-logs:
	$(SSH) $(REMOTE) 'tmux capture-pane -p -t "$(TMUX_SESSION)" -S -100'

# Stop the server session
cloud-stop:
	$(SSH) $(REMOTE) 'tmux kill-session -t "$(TMUX_SESSION)"'

# Check whether the session is running
cloud-status:
	$(SSH) $(REMOTE) 'tmux has-session -t "$(TMUX_SESSION)" 2>/dev/null && echo "Server session exists" || echo "Server session is not running"'
# Clean build artifacts on the remote machine
cloud-clean:
	$(SSH) $(REMOTE) 'cd "$(REMOTE_DIR)" && make clean'