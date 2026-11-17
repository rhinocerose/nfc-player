PORT ?= /dev/ttyACM0

.PHONY: setup build flash monitor clean board

setup:
	./scripts/setup.sh

build:
	./scripts/build.sh

flash:
	./scripts/flash.sh $(PORT)

monitor:
	./scripts/monitor.sh $(PORT)

clean:
	rm -rf .build

board:
	./scripts/board.sh
