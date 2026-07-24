# RetracRes — Makefile
# Requires MinGW-w64 (MSYS2): mingw-w64-x86_64-gcc / mingw-w64-i686-gcc

ARCHITECTURE ?= x64

ifeq ($(ARCHITECTURE),x86)
    MINGW32 = C:/msys64/mingw32/bin
    CXX = $(MINGW32)/g++
    ARCHITECTURE_FLAGS = -m32
    WINDRES = $(MINGW32)/windres --target=pe-i386
else ifeq ($(ARCHITECTURE),x64)
    MINGW64 = C:/msys64/mingw64/bin
    CXX = $(MINGW64)/g++
    ARCHITECTURE_FLAGS = -m64
    WINDRES = $(MINGW64)/windres --target=x86_64-w64-mingw32
else
    $(error Invalid architecture. Use ARCHITECTURE=x86 or ARCHITECTURE=x64.)
endif

CXXFLAGS = -Wall -Wextra -std=c++20 -DUNICODE -D_UNICODE -O2 $(ARCHITECTURE_FLAGS)
LDFLAGS = -lgdi32 -luser32 -lshell32 -lcomctl32 -luxtheme -lgdiplus -lcomdlg32 -ldwmapi -mwindows -municode -static -s $(ARCHITECTURE_FLAGS)

BIN = bin
OBJ = obj
SRC = src
INCLUDE = include
LIB_SIMPLEINI = lib/simpleini
RESOURCES = resources
RC = $(RESOURCES)/resources.rc
MANIFEST = $(RESOURCES)/RetracRes.exe.manifest
RES_OBJ = $(OBJ)/resources.o
FONT = font

SOURCES = $(wildcard $(SRC)/*.cpp)
OBJECTS = $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(SOURCES))
EXECUTABLE = RetracRes

LIBS = -lgdi32 -luser32 -lshell32 -lcomctl32 -luxtheme -lgdiplus -lcomdlg32 -ldwmapi -mwindows
INCLUDES = -I$(INCLUDE) -I$(LIB_SIMPLEINI)

.PHONY: all clean distclean

all: | $(BIN) $(OBJ)
	$(MAKE) $(EXECUTABLE)

$(BIN) $(OBJ):
	@if not exist "$@" mkdir "$@"

$(EXECUTABLE): $(OBJECTS) $(RES_OBJ) $(MANIFEST)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(RES_OBJ) -o $(BIN)/$(EXECUTABLE)_$(ARCHITECTURE).exe $(LIBS) $(INCLUDES) $(LDFLAGS) -static -s
	@mkdir -p "$(BIN)/font"
	@cp -f "$(FONT)/"* "$(BIN)/font/" 2>/dev/null || true

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(RES_OBJ): $(RC) $(MANIFEST)
	$(WINDRES) -I $(INCLUDE) --input $< --output $@ --output-format=coff

distclean: clean
	@if exist "$(BIN)\$(EXECUTABLE)_x86.exe" del /q "$(BIN)\$(EXECUTABLE)_x86.exe"
	@if exist "$(BIN)\$(EXECUTABLE)_x64.exe" del /q "$(BIN)\$(EXECUTABLE)_x64.exe"

clean:
	@if exist $(OBJ) for %%f in ("$(OBJ)\*.o") do del /q "%%f"
	@if exist "$(RES_OBJ)" del /q "$(RES_OBJ)"
