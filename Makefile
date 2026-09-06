#---------------------------------------------------------------------------------
# Immich 3DS - Native Immich Client for Nintendo 3DS
#---------------------------------------------------------------------------------

TARGET      := Immich3DS
TITLE       := Immich 3DS
DESCRIPTION := Native Photo Sync & Gallery Client for Immich
AUTHOR      := Immich3DS Team
ICON        := assets/icon.png
BANNER      := assets/banner.png
ROMFS_DIR   := romfs

#---------------------------------------------------------------------------------
# devkitPro Configuration
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(warning DEVKITPRO environment variable is not set. Try "make docker" or set DEVKITPRO=/opt/devkitpro)
endif

DEVKITARM ?= $(DEVKITPRO)/devkitARM
LIBCTRU   ?= $(DEVKITPRO)/libctru
PORTLIBS  ?= $(DEVKITPRO)/portlibs/3ds

# Architecture flags
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

# Common compiler flags
COMMON_FLAGS := -g -Wall -O2 -mword-relocations \
                -fomit-frame-pointer -ffunction-sections \
                $(ARCH) -DARM11 -D_3DS -D__3DS__

CFLAGS   := $(COMMON_FLAGS) -std=gnu11
CXXFLAGS := $(COMMON_FLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

INCLUDE  := -Iinclude -I$(PORTLIBS)/include -I$(LIBCTRU)/include
LIBDIRS  := -L$(PORTLIBS)/lib -L$(LIBCTRU)/lib

# Libraries to link (order is critical for mbedtls and curl)
LIBS     := -lcitro2d -lcitro3d -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lctru -lm

# Build tools
PREFIX   ?= arm-none-eabi-
CC       := $(PREFIX)gcc
CXX      := $(PREFIX)g++
LD       := $(PREFIX)g++
3DSXTOOL := 3dsxtool
SMDHTOOL := smdhtool
MAKEROM  := makerom
BANNERTOOL := bannertool
3DSLINK  := ./tools/3dslink

# Directories
BUILD_DIR := build
SRC_DIR   := source

# Source files
C_SRCS   := $(wildcard $(SRC_DIR)/*.c)
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cpp)

OBJS := $(patsunoff $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS)) \
        $(patsunoff $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CPP_SRCS))

# Correct substitution
OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(CPP_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

.PHONY: all clean cia send docker

all: $(TARGET).3dsx $(TARGET).smdh

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "  CXX   $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(TARGET).elf: $(OBJS)
	@echo "  LD    $@"
	@$(LD) $(ARCH) -Wl,-Map,$(BUILD_DIR)/$(TARGET).map -Wl,--gc-sections \
		-specs=3dsx.specs $(LIBDIRS) $(OBJS) $(LIBS) -o $@

$(TARGET).smdh: $(ICON)
	@echo "  SMDH  $@"
	@$(SMDHTOOL) --create "$(TITLE)" "$(DESCRIPTION)" "$(AUTHOR)" $(ICON) $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	@echo "  3DSX  $@"
	@if [ -d "$(ROMFS_DIR)" ]; then \
		$(3DSXTOOL) $< $@ --smdh=$(TARGET).smdh --romfs=$(ROMFS_DIR); \
	else \
		$(3DSXTOOL) $< $@ --smdh=$(TARGET).smdh; \
	fi
	@echo "Build complete: $(TARGET).3dsx"

#---------------------------------------------------------------------------------
# CIA Target (for permanent installation & FBI QR install)
#---------------------------------------------------------------------------------
$(BUILD_DIR)/banner.bin: $(BANNER) | $(BUILD_DIR)
	@echo "  BANNER $@"
	@$(BANNERTOOL) makebanner -i $(BANNER) -o $@

cia: $(TARGET).cia

$(TARGET).cia: $(TARGET).elf $(TARGET).smdh $(BUILD_DIR)/banner.bin Immich3DS.rsf
	@echo "  MAKEROM $@"
	@if [ -d "$(ROMFS_DIR)" ]; then \
		$(3DSXTOOL) $(TARGET).elf $(BUILD_DIR)/romfs.bin --romfs=$(ROMFS_DIR); \
		$(MAKEROM) -f cia -o $@ -elf $(TARGET).elf -rsf Immich3DS.rsf \
			-icon $(TARGET).smdh -banner $(BUILD_DIR)/banner.bin \
			-romfs $(BUILD_DIR)/romfs.bin; \
	else \
		$(MAKEROM) -f cia -o $@ -elf $(TARGET).elf -rsf Immich3DS.rsf \
			-icon $(TARGET).smdh -banner $(BUILD_DIR)/banner.bin; \
	fi
	@echo "CIA build complete: $(TARGET).cia"

#---------------------------------------------------------------------------------
# Wireless deployment via 3dslink
#---------------------------------------------------------------------------------
send: $(TARGET).3dsx
ifeq ($(strip $(IP)),)
	@echo "Error: Specify 3DS IP address: make send IP=192.168.x.x"
else
	@if command -v 3dslink >/dev/null 2>&1; then 		3dslink -a $(IP) $(TARGET).3dsx; 	else 		docker run --rm --net=host -v "$$(pwd)":/workspace -w /workspace devkitpro/devkitarm:latest 			bash -c "source /opt/devkitpro/3dsvars.sh && 3dslink -a $(IP) $(TARGET).3dsx"; 	fi
endif

#---------------------------------------------------------------------------------
# Docker build target (works out-of-the-box on macOS / Apple Silicon)
#---------------------------------------------------------------------------------
docker:
	@echo "Building Immich 3DS inside devkitPro Docker container..."
	docker run --rm -v "$$(pwd)":/workspace -w /workspace devkitpro/devkitarm:latest \
		bash -c "dkp-pacman -Syu --noconfirm 3ds-curl 3ds-mbedtls 3ds-zlib 3ds-citro2d 3ds-citro3d && make all"

docker-cia:
	@echo "Building CIA inside devkitPro Docker container..."
	docker run --rm -v "$$(pwd)":/workspace -w /workspace devkitpro/devkitarm:latest \
		bash -c "dkp-pacman -Syu --noconfirm 3ds-curl 3ds-mbedtls 3ds-zlib 3ds-citro2d 3ds-citro3d 3ds-tools makerom bannertool && make cia"

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia
