// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include <vector>

#include "../common/unittest.hpp"

template <bool IsConst>
using iterator = typename std::conditional<IsConst, pmem::kv::db::read_iterator,
					   pmem::kv::db::write_iterator>::type;

using pair = std::pair<std::string, std::string>;

template <bool IsConst = true>
using key_result = std::pair<typename std::conditional<IsConst, pmem::kv::string_view,
						       pmem::kv::db::accessor>::type,
			     pmem::kv::status>;

static std::vector<pair> keys{{"aaa", "1"}, {"bbb", "2"}, {"ccc", "3"},	 {"rrr", "4"},
			      {"sss", "5"}, {"ttt", "6"}, {"yyy", "记!"}};

static void insert_keys(pmem::kv::db &kv)
{
	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(kv.put(p.first, p.second), pmem::kv::status::OK);
	});
}

static void verify_key(key_result<> result, pmem::kv::string_view expected)
{
	ASSERT_STATUS(result.second, pmem::kv::status::OK);
	UT_ASSERTeq(expected.compare(result.first), 0);
}

template <bool IsConst>
static typename std::enable_if_t<IsConst> verify_value(key_result<IsConst> result,
						       pmem::kv::string_view expected)
{
	ASSERT_STATUS(result.second, pmem::kv::status::OK);
	UT_ASSERTeq(expected.compare(result.first), 0);
}

template <bool IsConst>
static typename std::enable_if_t<!IsConst> verify_value(key_result<IsConst> result,
							pmem::kv::string_view expected)
{
	ASSERT_STATUS(result.second, pmem::kv::status::OK);
	auto read_res = result.first.read_range(0, 1);
	std::cerr << "Aaaaaaaaaaaaaa" << std::endl;
	ASSERT_STATUS(read_res.second, pmem::kv::status::OK);
	UT_ASSERTeq(expected.compare(pmem::kv::string_view{read_res.first.begin()}), 0);
}

template <bool IsConst>
typename std::enable_if<IsConst, iterator<IsConst>>::type new_iterator(pmem::kv::db &kv)
{
	return kv.new_read_iterator();
}

template <bool IsConst>
typename std::enable_if<!IsConst, iterator<IsConst>>::type new_iterator(pmem::kv::db &kv)
{
	return kv.new_write_iterator();
}

template <bool IsConst>
static void seek_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek(p.first), pmem::kv::status::NOT_FOUND);
		ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);
		// ASSERT VAL
	});

	insert_keys(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek(p.first), pmem::kv::status::OK);
		verify_key(it.key(), p.first);
		verify_value<IsConst>(it.value(), p.second);
	});
}

template <bool IsConst>
static void seek_lower_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_lower(p.first), pmem::kv::status::NOT_FOUND);
		ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);
	});

	insert_keys(kv);

	ASSERT_STATUS(it.seek_lower(keys[0].first), pmem::kv::status::NOT_FOUND);

	auto prev_key = keys.begin();
	std::for_each(keys.begin() + 1, keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_lower(p.first), pmem::kv::status::OK);
		verify_key(it.key(), (prev_key++)->first);
		// XXX: ASSERT(it.value())
	});
}

template <bool IsConst>
static void seek_lower_eq_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_lower_eq(p.first), pmem::kv::status::NOT_FOUND);
		ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);
	});

	insert_keys(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_lower_eq(p.first), pmem::kv::status::OK);
		verify_key(it.key(), p.first);
		// XXX: ASSERT(it.value())
	});
}

template <bool IsConst>
static void seek_higher_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_higher(p.first), pmem::kv::status::NOT_FOUND);
		ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);
	});

	insert_keys(kv);

	auto next_key = keys.begin() + 1;
	std::for_each(keys.begin(), keys.end() - 1, [&](pair p) {
		ASSERT_STATUS(it.seek_higher(p.first), pmem::kv::status::OK);
		verify_key(it.key(), (next_key++)->first);
		// XXX: ASSERT(it.value())
	});

	ASSERT_STATUS(it.seek_higher(keys[keys.size() - 1].first),
		      pmem::kv::status::NOT_FOUND);
}

template <bool IsConst>
static void seek_higher_eq_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_higher_eq(p.first), pmem::kv::status::NOT_FOUND);
		ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);
	});

	insert_keys(kv);

	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek_higher_eq(p.first), pmem::kv::status::OK);
		verify_key(it.key(), p.first);
		// XXX: ASSERT(it.value())
	});
}

template <bool IsConst>
static void seek_to_first_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	ASSERT_STATUS(it.seek_to_first(), pmem::kv::status::NOT_FOUND);
	ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);

	insert_keys(kv);

	ASSERT_STATUS(it.seek_to_first(), pmem::kv::status::OK);

	auto first_key = keys.begin();
	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek(p.first), pmem::kv::status::OK);
		ASSERT_STATUS(it.seek_to_first(), pmem::kv::status::OK);
		verify_key(it.key(), first_key->first);
		// XXX: ASSERT(it.value())
	});
}

template <bool IsConst>
static void seek_to_last_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	ASSERT_STATUS(it.seek_to_last(), pmem::kv::status::NOT_FOUND);
	ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);

	insert_keys(kv);

	ASSERT_STATUS(it.seek_to_last(), pmem::kv::status::OK);

	auto last_key = --keys.end();
	std::for_each(keys.begin(), keys.end(), [&](pair p) {
		ASSERT_STATUS(it.seek(p.first), pmem::kv::status::OK);
		ASSERT_STATUS(it.seek_to_last(), pmem::kv::status::OK);
		verify_key(it.key(), last_key->first);
		// XXX: ASSERT(it.value())
	});
}

template <bool IsConst>
static void next_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	ASSERT_STATUS(it.next(), pmem::kv::status::NOT_FOUND);
	ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);

	insert_keys(kv);

	ASSERT_STATUS(it.seek_to_first(), pmem::kv::status::OK);

	std::for_each(keys.begin(), keys.end() - 1, [&](pair p) {
		verify_key(it.key(), p.first);
		// XXX: ASSERT(it.value())
		ASSERT_STATUS(it.next(), pmem::kv::status::OK);
	});

	verify_key(it.key(), (--keys.end())->first);
	// XXX: ASSERT(it.value())
	ASSERT_STATUS(it.next(), pmem::kv::status::NOT_FOUND);
}

template <bool IsConst>
static void prev_test(pmem::kv::db &kv)
{
	auto it = new_iterator<IsConst>(kv);

	ASSERT_STATUS(it.prev(), pmem::kv::status::NOT_FOUND);
	ASSERT_STATUS(it.key().second, pmem::kv::status::NOT_FOUND);

	insert_keys(kv);

	ASSERT_STATUS(it.seek_to_last(), pmem::kv::status::OK);

	std::for_each(keys.rbegin(), keys.rend() - 1, [&](pair p) {
		verify_key(it.key(), p.first);
		// XXX: ASSERT(it.value())
		ASSERT_STATUS(it.prev(), pmem::kv::status::OK);
	});

	verify_key(it.key(), keys.begin()->first);
	// XXX: ASSERT(it.value())
	ASSERT_STATUS(it.prev(), pmem::kv::status::NOT_FOUND);
}

static void test(int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s engine json_config [if_test_prev]", argv[0]);

	run_engine_tests(argv[1], argv[2],
			 {seek_test<true>, seek_test<false>, //seek_lower_test<true>,
			  /*seek_lower_test<false>, seek_lower_eq_test<true>,
			  seek_lower_eq_test<false>, seek_higher_test<true>,
			  seek_higher_test<false>, seek_higher_eq_test<true>,
			  seek_higher_eq_test<false>, seek_to_first_test<true>,
			  seek_to_first_test<false>, seek_to_last_test<true>,
			  seek_to_last_test<false>, next_test<true>, next_test<false>*/});

	// if (argc < 4 || std::string(argv[3]).compare("false") != 0)
	// run_engine_tests(argv[1], argv[2], {prev_test<true>, prev_test<false>});
}

int main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
