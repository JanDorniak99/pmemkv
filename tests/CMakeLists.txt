# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

#
# tests/CMakeLists.txt - prepares all the tests; it specifies which tests are enabled based on
#	options and available programs. Build tests with 'make tests' command, execute them using 'ctest'
#
include(ctest_helpers.cmake)

# ----------------------------------------------------------------- #
## Setup
# ----------------------------------------------------------------- #
add_custom_target(tests)

# Find valgrind
if(PKG_CONFIG_FOUND)
	pkg_check_modules(VALGRIND QUIET valgrind)
else()
	find_package(VALGRIND QUIET)
endif()

if(NOT VALGRIND_FOUND AND TESTS_USE_VALGRIND)
	message(FATAL_ERROR "Valgrind not found, but flag TESTS_USE_VALGRIND was set.")
elseif(NOT VALGRIND_FOUND)
	message(WARNING "Valgrind not found. Valgrind tests will not be performed.")
endif()

# Find libpmem & libpmemobj
if(PKG_CONFIG_FOUND)
	pkg_check_modules(LIBPMEMOBJ REQUIRED libpmemobj>=${LIBPMEMOBJ_REQUIRED_VERSION})
	pkg_check_modules(LIBPMEM REQUIRED libpmem>=${LIBPMEM_REQUIRED_VERSION})
else()
	find_package(LIBPMEMOBJ REQUIRED ${LIBPMEMOBJ_REQUIRED_VERSION})
	find_package(LIBPMEM REQUIRED ${LIBPMEM_REQUIRED_VERSION})
endif()

find_pmemcheck()
find_libunwind()
find_pmreorder()
find_pmempool()
find_gdb()

# Add checks when DEVELOPER_MODE is ON
add_cppstyle(tests ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
		${CMAKE_CURRENT_SOURCE_DIR}/c_api/*.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/*.h*
		${CMAKE_CURRENT_SOURCE_DIR}/common/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/compatibility/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/comparator/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/config/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/*.h*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/all/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/concurrent/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/memkind/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/persistent/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/pmemobj/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/pmemobj/*.h*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/pmreorder/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/sorted/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/sorted/*.h*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/transaction/*.c*
		${CMAKE_CURRENT_SOURCE_DIR}/result/*.c*)

add_check_whitespace(tests ${CMAKE_CURRENT_SOURCE_DIR}/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/c_api/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/common/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/compatibility/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/comparator/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/config/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/all/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/concurrent/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/memkind/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/persistent/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/pmemobj/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/pmreorder/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/sorted/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/engine_scenarios/transaction/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/result/*.*)

if(TESTS_JSON AND NOT BUILD_JSON_CONFIG)
	message(FATAL_ERROR "Tests requiring 'libpmemkv_json_config' library "
		"are enabled (with TESTS_JSON option), but the library is not built. "
		"If you want to run them use BUILD_JSON_CONFIG=ON option.")
elseif(NOT TESTS_JSON)
	message(WARNING
		"Most of pmemkv's tests require the 'libpmemkv_json_config' library. "
		"To enable these tests use TESTS_JSON option (and turn BUILD_JSON_CONFIG on).")
endif()

add_library(test_backtrace STATIC common/test_backtrace.c)
if(LIBUNWIND_FOUND)
	target_compile_definitions(test_backtrace PUBLIC USE_LIBUNWIND=1)
endif()

add_executable(check_is_pmem common/check_is_pmem.cpp)
target_link_libraries(check_is_pmem ${LIBPMEM_LIBRARIES})

if(COVERAGE AND VALGRIND_FOUND)
	message(STATUS "This is the Coverage build, skipping Valgrind tests")
endif()

# ----------------------------------------------------------------- #
## Common tests
# ----------------------------------------------------------------- #
build_test(wrong_engine_name_test wrong_engine_name_test.cc)
add_test_generic(NAME wrong_engine_name_test TRACERS none)

if(BUILD_EXAMPLES AND ENGINE_CMAP)
	add_dependencies(tests example-pmemkv_basic_c
		example-pmemkv_basic_cpp
		example-pmemkv_pmemobj_cpp)
	add_test_generic(NAME example-pmemkv_basic_c TRACERS none)
	add_test_generic(NAME example-pmemkv_basic_cpp TRACERS none)
	add_test_generic(NAME example-pmemkv_pmemobj_cpp TRACERS none)

	if(BUILD_JSON_CONFIG)
		add_dependencies(tests example-pmemkv_config_c)
		add_test_generic(NAME example-pmemkv_config_c TRACERS none)
	endif()
elseif(BUILD_EXAMPLES AND NOT ENGINE_CMAP)
	message(WARNING
		"Examples use cmap engine, which is disabled, hence their execution "
		"is also disabled. If you want to run them use -DENGINE_CMAP=ON option.")
endif()

build_test(config_c config/config_c.c)
add_test_generic(NAME config_c TRACERS none memcheck)

build_test(config_cpp config/config_cpp.cc)
add_test_generic(NAME config_cpp TRACERS none memcheck)

build_test(blackhole_test engines/blackhole/blackhole_test.cc)
add_test_generic(NAME blackhole_test TRACERS none memcheck)

build_test_ext(NAME json_to_config SRC_FILES config/json_to_config.cc LIBS json)
add_test_generic(NAME json_to_config TRACERS none memcheck)

build_test(result result/result.cpp)
add_test_generic(NAME result TRACERS none memcheck)

# ----------------------------------------------------------------- #
## Test scenarios (parametrized at least with engine name)
# ----------------------------------------------------------------- #
# Tests for all engines
build_test_ext(NAME put_get_remove SRC_FILES engine_scenarios/all/put_get_remove.cc LIBS json)
build_test_ext(NAME put_get_remove_charset_params SRC_FILES engine_scenarios/all/put_get_remove_charset_params.cc LIBS json)
build_test_ext(NAME put_get_remove_long_key SRC_FILES engine_scenarios/all/put_get_remove_long_key.cc LIBS json)
build_test_ext(NAME put_get_remove_params SRC_FILES engine_scenarios/all/put_get_remove_params.cc LIBS json)
build_test_ext(NAME put_get_std_map SRC_FILES engine_scenarios/all/put_get_std_map.cc LIBS json)
build_test_ext(NAME iterate SRC_FILES engine_scenarios/all/iterate.cc LIBS json)
build_test_ext(NAME error_handling_oom SRC_FILES engine_scenarios/all/error_handling_oom.cc LIBS json)

# Tests for concurrent engines
build_test_ext(NAME concurrent_iterate_params SRC_FILES engine_scenarios/concurrent/iterate_params.cc LIBS json)
build_test_ext(NAME concurrent_put_get_remove_params SRC_FILES engine_scenarios/concurrent/put_get_remove_params.cc LIBS json)
build_test_ext(NAME concurrent_put_get_remove_gen_params SRC_FILES engine_scenarios/concurrent/put_get_remove_gen_params.cc LIBS json)
build_test_ext(NAME concurrent_put_get_remove_single_op_params SRC_FILES engine_scenarios/concurrent/put_get_remove_single_op_params.cc LIBS json)
build_test_ext(NAME iterator_concurrent SRC_FILES engine_scenarios/concurrent/iterator_concurrent.cc LIBS json)

# Tests for peristent engines
build_test_ext(NAME persistent_not_found_verify SRC_FILES engine_scenarios/persistent/not_found_verify.cc LIBS json)
build_test_ext(NAME persistent_overwrite_verify SRC_FILES engine_scenarios/persistent/overwrite_verify.cc LIBS json)
build_test_ext(NAME persistent_put_remove_verify SRC_FILES engine_scenarios/persistent/put_remove_verify.cc LIBS json)
build_test_ext(NAME persistent_put_verify_asc_params SRC_FILES engine_scenarios/persistent/put_verify_asc_params.cc LIBS json)
build_test_ext(NAME persistent_put_verify_desc_params SRC_FILES engine_scenarios/persistent/put_verify_desc_params.cc LIBS json)
build_test_ext(NAME persistent_put_verify SRC_FILES engine_scenarios/persistent/put_verify.cc LIBS json)
build_test_ext(NAME persistent_put_get_std_map_multiple_reopen SRC_FILES engine_scenarios/persistent/put_get_std_map_multiple_reopen.cc LIBS json)
build_test_ext(NAME pmreorder_insert SRC_FILES engine_scenarios/pmreorder/insert.cc LIBS json)
build_test_ext(NAME pmreorder_erase SRC_FILES engine_scenarios/pmreorder/erase.cc LIBS json)
build_test_ext(NAME pmreorder_iterator SRC_FILES engine_scenarios/pmreorder/iterator.cc LIBS json)
build_test_ext(NAME pmreorder_recover SRC_FILES engine_scenarios/pmreorder/recover.cc LIBS json)

# Tests for sorted engines
build_test_ext(NAME sorted_iterate SRC_FILES engine_scenarios/sorted/iterate.cc LIBS json)
build_test_ext(NAME sorted_get_all_gen_params SRC_FILES engine_scenarios/sorted/get_all_gen_params.cc LIBS json)
build_test_ext(NAME sorted_get_above_gen_params SRC_FILES engine_scenarios/sorted/get_above_gen_params.cc LIBS json)
build_test_ext(NAME sorted_get_equal_above_gen_params SRC_FILES engine_scenarios/sorted/get_equal_above_gen_params.cc LIBS json)
build_test_ext(NAME sorted_get_below_gen_params SRC_FILES engine_scenarios/sorted/get_below_gen_params.cc LIBS json)
build_test_ext(NAME sorted_get_equal_below_gen_params SRC_FILES engine_scenarios/sorted/get_equal_below_gen_params.cc LIBS json)
build_test_ext(NAME sorted_get_between_gen_params SRC_FILES engine_scenarios/sorted/get_between_gen_params.cc LIBS json)

# Tests for pmemobj engines
build_test_ext(NAME pmemobj_error_handling_create SRC_FILES engine_scenarios/pmemobj/error_handling_create.cc LIBS json)
build_test_ext(NAME pmemobj_error_handling_defrag SRC_FILES engine_scenarios/pmemobj/error_handling_defrag.cc LIBS json)
build_test_ext(NAME pmemobj_error_handling_tx_path SRC_FILES engine_scenarios/pmemobj/error_handling_tx_path.cc LIBS json)
build_test_ext(NAME pmemobj_put_get_std_map_defrag SRC_FILES engine_scenarios/pmemobj/put_get_std_map_defrag.cc LIBS json)
build_test_ext(NAME pmemobj_error_handling_tx_oom SRC_FILES engine_scenarios/pmemobj/error_handling_tx_oom.cc engine_scenarios/pmemobj/mock_tx_alloc.cc LIBS json dl_libs)
build_test_ext(NAME pmemobj_error_handling_tx_oid SRC_FILES engine_scenarios/pmemobj/error_handling_tx_oid.cc LIBS json libpmemobj_cpp)
build_test_ext(NAME pmemobj_put_get_std_map_oid SRC_FILES engine_scenarios/pmemobj/put_get_std_map_oid.cc LIBS json libpmemobj_cpp)

# Tests for memkind engines
build_test_ext(NAME memkind_error_handling SRC_FILES engine_scenarios/memkind/error_handling.cc LIBS json)

# Tests for C API
build_test(c_api_null_db_config c_api/null_db_config.c)

# Tests for comparator
build_test_ext(NAME comparator_basic_c SRC_FILES comparator/basic.c LIBS json)
build_test_ext(NAME comparator_basic_persistent_c SRC_FILES comparator/basic_persistent.c LIBS json)
build_test_ext(NAME comparator_custom_reopen_c SRC_FILES comparator/custom_reopen.c LIBS json)
build_test_ext(NAME comparator_default_reopen_c SRC_FILES comparator/default_reopen.c LIBS json)
build_test_ext(NAME comparator_custom_reopen_cpp SRC_FILES comparator/custom_reopen.cc LIBS json)
build_test_ext(NAME comparator_default_reopen_cpp SRC_FILES comparator/default_reopen.cc LIBS json)

# Tests for transaction
build_test_ext(NAME transaction_put SRC_FILES engine_scenarios/transaction/put.cc LIBS json)
build_test_ext(NAME transaction_remove SRC_FILES engine_scenarios/transaction/remove.cc LIBS json)
build_test_ext(NAME transaction_put_pmreorder SRC_FILES engine_scenarios/transaction/put_pmreorder.cc LIBS json)

# Tests for iterator
build_test_ext(NAME iterator_basic SRC_FILES engine_scenarios/all/iterator_basic.cc LIBS json)
build_test_ext(NAME iterator_sorted SRC_FILES engine_scenarios/sorted/iterator_sorted.cc LIBS json)

################################################################################
#################################### ROBINHOOD #################################
# XXX: Parameterize other tests to work with fixed size keys/values.
if (ENGINE_ROBINHOOD)
	if(PMREORDER_SUPPORTED)
		add_engine_test(ENGINE robinhood
				BINARY pmreorder_recover
				TRACERS none
				SCRIPT pmemobj_based/pmreorder/recover.cmake)
	endif()
endif()
################################################################################
