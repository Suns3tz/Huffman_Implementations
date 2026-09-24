SUBDIRS = HuffFork HuffPthread HuffSerial GUI
TARGET = Huffman

.PHONY: all $(SUBDIRS) $(TARGET) clean

all: $(SUBDIRS) $(TARGET)

$(SUBDIRS):
	$(MAKE) -C $@

$(TARGET):
	@echo '#!/bin/sh' > $(TARGET)
	@echo 'cd $$(dirname $$0)/GUI && ./gui_app "$$@"' >> $(TARGET)
	@chmod +x $(TARGET)
	@echo "\n=============================================="
	@echo " Compilación exitosa. Ejecuta con: ./Huffman"
	@echo "==============================================\n"

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f $(TARGET)
