CMAKE_CPU_SETUP := $(shell expr $(shell getconf _NPROCESSORS_ONLN) + 2)

DOCKER_CONTAINER=contracts-development

DOCKER_COMMON=-v `pwd`:/opt/contracts \
			--name $(DOCKER_CONTAINER) -w /opt/contracts waxteam/dev:wax-1.6.1-1.0.0

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
	-docker rm -f $(DOCKER_CONTAINER)

.PHONY:dev-docker-start
dev-docker-start: dev-docker-stop
	docker run -it $(DOCKER_COMMON) bash

# Useful for wax-docker project
.PHONY:dev-docker-all
dev-docker-all: dev-docker-stop 
	docker run --user $(shell id -u):$(shell id -g) $(DOCKER_COMMON) bash -c "\
        rm -rf build && \
        cmake . -B./build -GNinja && \
        cmake --build ./build && \
		build/tests/unit_test --show_progress"
