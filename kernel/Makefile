TARGET = kernel.elf # 成果物
OBJS = main.o graphics.o font.o		# 必要なオブジェクト群
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
			-fno-exceptions -fno-rtti -std=c++17	# コンパイルオプション
LDFLAGS	 += --entry KernelMain -z norelro --image-base 0x100000 --static # リンクオプション


.PHONY: all
all: $(TARGET)

.PHONY: clean
clean: 
		rm -rf *.o

kernel.elf: $(OBJS) Makefile
		ld.lld $(LDFLAGS) -o kernel.elf $(OBJS)

%.o: %.cpp Makefile
		clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

.%.d: %.cpp	
	clang++ $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)