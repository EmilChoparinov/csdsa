#==============================================================================#
#	Author: E.D Choparinov, Amsterdam
#	Created On: February 13 2024
#	
#	This Makefile contains commands for building and testing the libcsdsa.
#
#	USAGE:
#	make all    | Builds into a *.a binary for linking.
#	make pkg	| Builds into a zip containing the *.a binary and *.h files.
#	make exec   | Builds the exec which can be used for development and testing.
#	make test   | Runs all tests in the test directory.
#	make env	| Build a new docker image compatible with compiling.
#	make docker | Enters a docker environment compatible with compiling.
#	make memtst | Run all tests in the test directory. Activated in docker. Make
#				  sure to run 'make env' first.
#==============================================================================#

#------------------------------------------------------------------------------#
# DIRECTORY PATH CONFIGURATIONS												   #
#------------------------------------------------------------------------------#
#	Directory Configurations
#	INC_DIRS:
#     - Location where *.h files appear and must be included.
#	  - Note: May contain many.
#	SRC_DIR:
#	  - The source path containing the framework.
#	OBJ_DIR:
#	  - The directory to output intermediary *.o.
#	LIBS_DIR:
#	  - The path to all libraries necessary for compiling.
#	TST_DIR:
#	  - The test path containing individual files containing various 
#		unit testing components for the framework and structures.
#	BUILD_DIR:
#	  - The build path of the final executable.
#	TST_BINS_DIR:
#	  - The directory to output each test binary
#------------------------------------------------------------------------------#
INC_DIR = -I./include/ -I./src/structures/ -I./src/core/ -I./src/utils
SRC_DIR = src
OBJ_DIR = obj
DEM_DIR = demo
DEM_OBJ = demo/objs
DEM_INC = -I./src/
TST_INC = -I./tests/unity -I./src/
TST_DIR = tests
UNITY_DIR = tests/unity
BUILD_DIR = .
TST_BINS_DIR = tests/bin
TST_OBJ_DIR  = tests/objs

#------------------------------------------------------------------------------#
# COMPILER CONFIGURATIONS													   #
#------------------------------------------------------------------------------#
#	CC:
#	  - Compiler to use
#	EXT:
#	  - File extension used in source.
#	CXXFLAGS:
#	  - Flags used for compilation. This automatically includes -I directories 
# 		listed in the INC_DIR variable.
#	TESTFLAGS:
#	  - Flags only used specifically for compiling test files
#	BUILDFLAGS:
#	  - Extra flags used for interacting with the build.
#	LDFLAGS:
#	  - 
#------------------------------------------------------------------------------#
CC = clang
EXT = .c
EXT_HDR = .h
EXT_ARCHIVE = .a
CXXFLAGS = -gdwarf-4 -Wall -Werror -UDEBUG $(INC_DIR)
TESTFLAGS = -DUNITY_OUTPUT_COLOR
BUILDFLAGS = -D_DEBUG

#------------------------------------------------------------------------------#
# PROJECT CONFIGURATIONS													   #
#------------------------------------------------------------------------------#
#	BUILD_FILE_NAME:
#	  - The name of the output file.
#	BUILD_LIB_FILE:
#	  - The full path to where the output file will live.
#------------------------------------------------------------------------------#
BUILD_FILE_NAME = libcsdsa
BUILD_LIB_FILE  = $(BUILD_DIR)/$(BUILD_FILE_NAME)$(EXT_ARCHIVE)
PACKG_ZIP_FILE  = $(BUILD_FILE_NAME).zip
DEMO_FILE_NAME  = run_demo
UNITY_FILE_NAME = unity

#------------------------------------------------------------------------------#
# PROJECT FILE COLLETION													   #
#------------------------------------------------------------------------------#
#	SRC_FILES:
#	  - Contains a list of all files in the SRC_DIR
#	LIB_FILES:
#	  - Contains a list of all files in the LIB_DIR
#	TST_FILES:
#	  - Contains a list of all files in the TST_DIR
#	OBJ_FILES:
#	  - Contains a list of all output locations for *.c files in the project.
#	  - OBJ_FILES is essentially a name map where 
#		[path_a]/[src].c -> [path_b]/[src].o
#	  - Note: Since libs must be included, the LIB_DIR files will be placed 
#		always in obj/lib/. Therefore the lib keyword is reserved.
#	TST_BINS:
#	  - Contains a list of all output locations for the testing binaries.
#------------------------------------------------------------------------------#
SRC_FILES = $(shell find $(SRC_DIR) -name "*$(EXT)")
HDR_FILES = $(shell find $(SRC_DIR) -name "*$(EXT_HDR)")
TST_FILES = $(wildcard $(TST_DIR)/*.c)
UNITY_FILES = $(wildcard $(UNITY_DIR)/*.c)
TST_OBJ_FILES =  $(TST_FILES:$(TST_DIR)/%$(EXT)=$(TST_BINS_DIR)/%.o)
DEM_FILES = $(wildcard $(DEM_DIR)/*.c)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%$(EXT)=$(OBJ_DIR)/%.o) 
TST_BINS  = $(patsubst $(TST_DIR)/%.c, $(TST_BINS_DIR)/%, $(TST_FILES))

#------------------------------------------------------------------------------#
# MAKE ALL					 												   #
#------------------------------------------------------------------------------#
all: notif $(BUILD_FILE_NAME)

notif:
	@echo Building to ${BUILD_LIB_FILE}...
	@mkdir -p ${BUILD_DIR}

$(BUILD_FILE_NAME): $(OBJ_FILES)
	@echo Bundling...
	ar cr $(BUILD_LIB_FILE) $^
	@echo Done! GLHF

$(OBJ_DIR)/%.o: $(SRC_DIR)/%$(EXT)
	@mkdir -p $(@D)
	@echo + $< -\> $@
	@$(CC) $(BUILDFLAGS) $(CXXFLAGS) -o $@ -c $<

$(OBJ_DIR)/tests/libs/%.o: $(LIBS_DIR)/%$(EXT)
	@mkdir -p $(@D)
	@echo + $< -\> $@
	@$(CC) $(BUILDFLAGS) $(CXXFLAGS) -o $@ -c $<

#------------------------------------------------------------------------------#
# MAKE PKG					 												   #
#------------------------------------------------------------------------------#
pkg: all 
	@echo "Packaging..."
	zip -j $(PACKG_ZIP_FILE) $(HDR_FILES) $(BUILD_LIB_FILE)

#------------------------------------------------------------------------------#
# MAKE TEST					 												   #
#------------------------------------------------------------------------------#
test: $(BUILD_FILE_NAME) build_unity $(TST_DIR) $(TST_BINS_DIR) $(TST_BINS) $(TST_OBJ_DIR)
	@echo Running Tests...
	@echo
	@for test in $(TST_BINS) ; do ./$$test --verbose && echo ; done

build_unity:
	$(CC) $(BUILDFLAGS) $(CXXFLAGS) -o $(UNITY_FILE_NAME).o -c $(UNITY_FILES)

$(TST_OBJ_DIR)/%.o: $(TST_DIR)%.c
	@mkdir -p $(@D)
	@echo + $< -\> $@
	$(CC) $(BUILDFLAGS) $(CXXFLAGS) -o $@ $< $(UNITY_FILE_NAME).o $(BUILD_LIB_FILE)

$(TST_BINS_DIR)/%: $(TST_DIR)/%.c
	@echo + $< -\> $@
	$(CC) $(BUILDFLAGS) $(CXXFLAGS) $(TST_INC) $(TESTFLAGS) $< $(UNITY_FILE_NAME).o $(LIB_TST_FILES) $(BUILD_LIB_FILE) -o $@

$(TST_DIR):
	@echo in TST_DIR
	@mkdir $@

$(TST_BINS_DIR):
	@mkdir $@

$(TST_OBJ_DIR):
	@mkdir $@

#------------------------------------------------------------------------------#
# MAKE EXEC																	   #
#------------------------------------------------------------------------------#
exec: $(BUILD_FILE_NAME) $(DEM_DIR) $(DEM_OBJ) 
	$(CC) $(BUILDFLAGS) $(CXXFLAGS) $(DEM_INC) $(DEM_FILES) $(BUILD_LIB_FILE) -o $(DEMO_FILE_NAME)

$(DEM_OBJ):
	@echo in DEM_OBJ
	@mkdir $@

$(DEM_DIR):
	@echo in DEM_DIR
	@mkdir $@

#------------------------------------------------------------------------------#
# MAKE MEMTST					 										   	   #
#------------------------------------------------------------------------------#
memtst:
	@docker run --rm -it \
		-v ".:/virtual" \
		--name csdsa-container \
		csdsa-image \
		/bin/bash -c 'make memtst_containerized clean'

memtst_containerized: $(BUILD_FILE_NAME) $(TST_DIR) $(TST_BINS_DIR) $(TST_BINS)
	@echo Running Valgrind with Tests...
	@for test in $(TST_BINS) ; do valgrind 									   \
		 --show-leak-kinds=all										   	       \
		 --leak-check=full 													   \
		 --track-origins=yes 												   \
		 ./$$test --verbose 											       \
		 ; done

#------------------------------------------------------------------------------#
# MAKE DOCKER					 										   	   #
#------------------------------------------------------------------------------#
.PHONY: docker
docker:
	@echo "Entering Container"
	@docker run --rm -it \
		-v "$$(pwd):/virtual" \
		--name csdsa-container \
		csdsa-image \
		/bin/bash
	@echo "Done!"

#------------------------------------------------------------------------------#
# MAKE ENV						 										   	   #
#------------------------------------------------------------------------------#
.PHONY: env
env:
	@echo "Calling 'make clean' to ensure source is clean before building!"
	@make clean
	@echo "Done!"
	@echo "Removing container and image if they already exist..."
	@docker stop csdsa-container || true
	@docker rm csdsa-container || true
	@docker rmi csdsa-image || true
	@echo "Done!"
	@echo "Building image..."
	@docker build -t csdsa-image .
	@echo "Done!"

#------------------------------------------------------------------------------#
# MAKE CLEAN																   #
#------------------------------------------------------------------------------#
.PHONY: clean
clean:
	@echo "Cleaning generated files..."
	rm -rf $(BUILD_LIB_FILE) $(OBJ_DIR) $(TST_BINS_DIR) $(PACKG_ZIP_FILE)
	rm -rf $(DEMO_FILE_NAME) $(UNITY_FILE_NAME).o
