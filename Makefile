NINJA=build/lib/bin/ninja -C out/cur

all:
	@$(NINJA)

test:
	@$(NINJA)
	scripts/unix-test.sh

clean:
	@$(NINJA) -t clean

distclean:
	rm -rf out/

.PHONY: clean dist distclean all test
