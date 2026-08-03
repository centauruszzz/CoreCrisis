GREEN=\033[0;1;92m
NC=\033[0m

build: FORCE
	rm -fr logs # Old version log files
	mkdir -p build
	rm -fr build/*
	

	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="--coverage -g" -DCMAKE_CXX_FLAGS="--coverage -g" -DCMAKE_EXE_LINKER_FLAGS="--coverage" -DCMAKE_SHARED_LINKER_FLAGS="--coverage" -G "CodeBlocks - Unix Makefiles" . -B cmake-build-release
	cmake --build cmake-build-release --target all
	
	cp cmake-build-release/nr-gnb build/
	cp cmake-build-release/nr-ue build/
	cp cmake-build-release/nr-cli build/
	cp cmake-build-release/libdevbnd.so build/
	cp tools/nr-binder build/

	# cmake -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - Unix Makefiles" . -B cmake-build-debug
	# cmake --build cmake-build-debug --target all

	# cp cmake-build-debug/nr-gnb build/
	# cp cmake-build-debug/nr-ue build/
	# cp cmake-build-debug/nr-cli build/
	# cp cmake-build-debug/libdevbnd.so build/
	# cp tools/nr-binder build/

	@printf "${GREEN}UERANSIM successfully built with coverage support.${NC}\n"

# Reset gcov counters before a fresh fuzzing/capture session
coverage-reset:
	lcov --zerocounters --directory cmake-build-release

# Capture coverage from the instrumented binaries and generate an HTML report
coverage: FORCE
	lcov --capture --directory cmake-build-release --output-file build/coverage.info --rc lcov_branch_coverage=1
	genhtml build/coverage.info --branch-coverage --output-directory build/coverage-report
	@printf "${GREEN}Coverage report: build/coverage-report/index.html${NC}\n"

FORCE:
