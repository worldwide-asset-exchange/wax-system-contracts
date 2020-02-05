CMAKE_CPU_SETUP := $(shell expr $(shell nproc) + 2)

# DEV_DOCKER_VERSION=wax-1.6.1-1.2.1
DEV_DOCKER_VERSION=wax-1.6.1-1.2.0-internal2
CONTAINER=wax-system-contracts

DOCKER_COMMON=-v `pwd`:`pwd` --name $(CONTAINER) -w `pwd` waxteam/dev:$(DEV_DOCKER_VERSION)

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
	-docker rm $(CONTAINER)

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


dev-build: dev-docker-stop
	docker run --user $(shell id -u):$(shell id -g) $(DOCKER_COMMON) bash -c "\
        cmake . -B./build -GNinja && \
        cmake --build ./build"

dev-test: dev-docker-stop
	docker run --user $(shell id -u):$(shell id -g) $(DOCKER_COMMON) bash -c "\
        build/tests/unit_test --run_test=eosio_system_tests/producer_pay --log_level=all -- --verbose"
