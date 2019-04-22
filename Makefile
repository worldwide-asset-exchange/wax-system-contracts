CMAKE_CPU_SETUP := $(shell expr $(shell getconf _NPROCESSORS_ONLN) + 2)

build:
	mkdir -p build
	cd build && cmake ..

.PHONY: compile
compile: build
	cd build && make -j$(CMAKE_CPU_SETUP)

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: compile
	./build/tests/unit_test --log_level=all

.PHONY:dev-docker-stop
dev-docker-stop:
	-docker rm -f contracts-development

.PHONY:dev-docker-start
dev-docker-start: dev-docker-stop
	docker run -it -v `pwd`:/opt/contracts --name contracts-development -w /opt/contracts waxteam/dev:v1.6.1 bash
