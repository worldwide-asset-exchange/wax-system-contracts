CMAKE_CPU_SETUP := $(shell expr $(shell getconf _NPROCESSORS_ONLN) + 2)

DOCKER_CONTAINER=contracts-development

DOCKER_COMMON=-v `pwd`:/opt/contracts \
			--name $(DOCKER_CONTAINER) -w /opt/contracts waxteam/dev:wax-1.8.1-1.0.0-rc6

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
	docker run --tty --user $(shell id -u):$(shell id -g) $(DOCKER_COMMON) bash -c "\
		   mkdir -p build && \
		   cd ./build && cmake .. && \
		   make -j$(CMAKE_CPU_SETUP) && \
           tests/unit_test --run_test=eosio_system_tests --log_level=all"
		   #tests/unit_test --run_test=eosio_system_tests/claim_half_period_twice --log_level=all"
