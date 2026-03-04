TARGET = kernel.elf # 成果物
OBJS = main.o		# 必要なオブジェクト群
CXXFLAGS += -O2 -Wall -g --target=x86_64-eld -ffreestanding -mno-red-zone \
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