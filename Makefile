PHONY := all package clean
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

CC := arm-vita-eabi-gcc
CXX := arm-vita-eabi-g++
STRIP := arm-vita-eabi-strip

PROJECT_TITLE := GXM Graphics
PROJECT_TITLEID := BNDT00005

PROJECT := gxm_graphics
CXXFLAGS += -std=c++11

LIBS := -lSceDisplay_stub -lSceGxm_stub -lScePgf_stub -lSceSysmodule_stub \
	-lSceKernel_stub -lSceCtrl_stub -lSceCommonDialog_stub

SRC_C :=$(call rwildcard, src/, *.c)
SRC_CPP :=$(call rwildcard, src/, *.cpp)
#I commented here to show a friend how git works

OBJ_DIRS := $(addprefix out/, $(dir $(SRC_C:src/%.c=%.o))) $(addprefix out/, $(dir $(SRC_CPP:src/%.cpp=%.o)))
#SHADER_DIRS := $(addprefix out/shaders/, $(dir $(SRC_SHADER:src/shaders/%.cg=%.gxp)))  $(addprefix out/shaders/bin/, $(dir $(SRC_SHADER_BIN:src/shaders/%.gxp=%.obj)))

OBJS := $(addprefix out/, $(SRC_C:src/%.c=%.o)) $(addprefix out/, $(SRC_CPP:src/%.cpp=%.o))
#SHADERS := $(addprefix out/shaders/, $(SRC_SHADER:src/shaders/%.cg=%.gxp))
#SHADER_BINS := $(addprefix out/shaders/bin/, $(SRC_SHADER_BIN:out/shaders/%.gxp=%.obj))
SHADER_BINS =	src/shaders/compiled/clear_v_gxp.o \
				src/shaders/compiled/clear_f_gxp.o \
				src/shaders/compiled/color_v_gxp.o \
				src/shaders/compiled/color_f_gxp.o


all: package

package: $(PROJECT).vpk

$(PROJECT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/pic0.png=sce_sys/pic0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/logo0.png=sce_sys/livearea/contents/logo0.png \
		--add sce_sys/livearea/contents/logo1.png=sce_sys/livearea/contents/logo1.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	$(PROJECT).vpk
	
eboot.bin: $(PROJECT).velf
	vita-make-fself $(PROJECT).velf eboot.bin

param.sfo:
	vita-mksfoex -s TITLE_ID="$(PROJECT_TITLEID)" "$(PROJECT_TITLE)" param.sfo

$(PROJECT).velf: $(PROJECT).elf
	$(STRIP) -g $<
	vita-elf-create $< $@

$(PROJECT).elf: $(SHADER_BINS) $(OBJS)
	$(CXX) -Wl,-q -o $@ $^ $(LIBS)

#$(SHADER_BINS) : $(SHADERS)

#shaders: $(SHADERS)

$(OBJ_DIRS):
	mkdir -p $@
#$(SHADER_DIRS):
#	mkdir -p $@
#	mkdir -p out/shaders/bin/fragmentShaders
#	mkdir -p out/shaders/bin/vertexShaders

out/%.o : src/%.cpp | $(OBJ_DIRS)
	arm-vita-eabi-g++ -c $(CXXFLAGS) -o $@ $<
out/%.o : src/%.c | $(OBJ_DIRS)
	arm-vita-eabi-g++ -c -o $@ $<

#out/shaders/fragmentShaders/%.gxp : src/shaders/fragmentShaders/%.cg | $(SHADER_DIRS)
#	psp2cgc --cache --profile sce_fp_psp2 $< -o $@
#out/shaders/vertexShaders/%.gxp : src/shaders/vertexShaders/%.cg | $(SHADER_DIRS)
#	psp2cgc --cache --profile sce_vp_psp2 $< -o $@

TEMPPATH1 := NULL
out/shaders/bin/%.obj : out/shaders/%.gxp | $(SHADER_BIN_DIRS)
	psp2bin $< -b2e PSP2,_binary_$(notdir $*)_gxp_start,_binary_$(notdir $*)_gxp_size,4 -o $@


clean:
	rm -f $(PROJECT).velf $(PROJECT).elf $(PROJECT).vpk param.sfo eboot.bin $(OBJS)
	rm -r $(abspath $(OBJ_DIRS))
