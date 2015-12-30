NINJA=ninja -C out/cur

all:
	@$(NINJA)

clean:
	@$(NINJA) -t clean

distclean:
	rm -rf out/

.PHONY: clean dist distclean all test
