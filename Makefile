build:
	mkdir -p build
	cd build && cmake ..

.PHONY: compile
compile: build
	cd build && make -j`getconf _NPROCESSORS_ONLN`

.PHONY: clean
clean:
	rm -rf build

.PHONY: test
test: compile
	./build/tests/unit_test

.PHONY:dev-docker-stop
dev-docker-stop:
	-docker rm -f contracts-development

.PHONY:dev-docker-start
dev-docker-start: dev-docker-stop
	docker run -it -v `pwd`:/opt/contracts --name contracts-development -w /opt/contracts waxteam/dev:v1.6.1 bash