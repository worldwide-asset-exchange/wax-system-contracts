DEV_VERSION=ce-v1.0.3wax01-v4.1.1wax01
DEV_DOCKER_IMAGE=waxteam/waxdev:$(DEV_VERSION)
DEV_DOCKER_CONTAINER=contracts-development
DEV_DOCKER_COMMON=-v `pwd`:/opt/contracts \
			--name $(DEV_DOCKER_CONTAINER) -w /opt/contracts $(DEV_DOCKER_IMAGE)
PARALLEL ?= $(shell nproc 2>/dev/null || echo 2)

get-latest:
	docker pull $(DEV_DOCKER_IMAGE)

build:
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dleap_DIR="${LEAP_BUILD_PATH}/lib/cmake/leap" -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" -DBOOST_ROOT="${HOME}/boost1.79" ..

.PHONY: compile
compile: build
	cd build && make -j $(PARALLEL)

.PHONY: clean
clean:
	-rm -rf build

.PHONY: test
test: compile
	./build/tests/unit_test --log_level=all

.PHONY:dev-docker-stop
dev-docker-stop:
	-docker rm -f $(DEV_DOCKER_CONTAINER)

.PHONY:dev-docker-start
dev-docker-start: dev-docker-stop get-latest
	docker run -it $(DEV_DOCKER_COMMON) bash

# Useful for travis CI
.PHONY:dev-docker-all
dev-docker-all: dev-docker-stop get-latest
	docker run $(DEV_DOCKER_COMMON) bash -c "make clean test"