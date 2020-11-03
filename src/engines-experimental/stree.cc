// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <iostream>
#include <unistd.h>

#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/transaction.hpp>

#include "../out.h"
#include "stree.h"

using pmem::detail::conditional_add_to_tx;
using pmem::obj::make_persistent_atomic;
using pmem::obj::transaction;

namespace pmem
{
namespace kv
{

stree::stree(std::unique_ptr<internal::config> cfg)
    : pmemobj_engine_base(cfg, "pmemkv_stree"), config(std::move(cfg))
{
	Recover();
	LOG("Started ok");
}

stree::~stree()
{
	LOG("Stopped ok");
}

std::string stree::name()
{
	return "stree";
}

status stree::count_all(std::size_t &cnt)
{
	LOG("count_all");
	check_outside_tx();

	cnt = my_btree->size();

	return status::OK;
}

template <typename It>
static std::size_t size(It first, It last)
{
	auto dist = std::distance(first, last);
	assert(dist >= 0);

	return static_cast<std::size_t>(dist);
}

/* above key, key exclusive */
status stree::count_above(string_view key, std::size_t &cnt)
{
	LOG("count_above key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->upper_bound(key);
	auto last = my_btree->end();

	cnt = size(first, last);

	return status::OK;
}

/* above or equal to key, key inclusive */
status stree::count_equal_above(string_view key, std::size_t &cnt)
{
	LOG("count_equal_above key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->lower_bound(key);
	auto last = my_btree->end();

	cnt = size(first, last);

	return status::OK;
}

/* below key, key exclusive */
status stree::count_below(string_view key, std::size_t &cnt)
{
	LOG("count_below key<" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->begin();
	auto last = my_btree->lower_bound(key);

	cnt = size(first, last);

	return status::OK;
}

/* below or equal to key, key inclusive */
status stree::count_equal_below(string_view key, std::size_t &cnt)
{
	LOG("count_equal_below key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->begin();
	auto last = my_btree->upper_bound(key);

	cnt = size(first, last);

	return status::OK;
}

status stree::count_between(string_view key1, string_view key2, std::size_t &cnt)
{
	LOG("count_between key range=[" << std::string(key1.data(), key1.size()) << ","
					<< std::string(key2.data(), key2.size()) << ")");
	check_outside_tx();

	if (my_btree->key_comp()(key1, key2)) {
		auto first = my_btree->upper_bound(key1);
		auto last = my_btree->lower_bound(key2);

		cnt = size(first, last);
	} else {
		cnt = 0;
	}

	return status::OK;
}

status stree::iterate(container_iterator first, container_iterator last,
		      get_kv_callback *callback, void *arg)
{
	for (auto it = first; it != last; ++it) {
		auto ret = callback(it->first.c_str(), it->first.size(),
				    it->second.c_str(), it->second.size(), arg);

		if (ret != 0)
			return status::STOPPED_BY_CB;
	}

	return status::OK;
}

status stree::get_all(get_kv_callback *callback, void *arg)
{
	LOG("get_all");
	check_outside_tx();

	auto first = my_btree->begin();
	auto last = my_btree->end();

	return iterate(first, last, callback, arg);
}

/* (key, end), above key */
status stree::get_above(string_view key, get_kv_callback *callback, void *arg)
{
	LOG("get_above start key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->upper_bound(key);
	auto last = my_btree->end();

	return iterate(first, last, callback, arg);
}

/* [key, end), above or equal to key */
status stree::get_equal_above(string_view key, get_kv_callback *callback, void *arg)
{
	LOG("get_equal_above start key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->lower_bound(key);
	auto last = my_btree->end();

	return iterate(first, last, callback, arg);
}

/* [start, key], below or equal to key */
status stree::get_equal_below(string_view key, get_kv_callback *callback, void *arg)
{
	LOG("get_equal_below start key>=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->begin();
	auto last = my_btree->upper_bound(key);

	return iterate(first, last, callback, arg);
}

/* [start, key), less than key, key exclusive */
status stree::get_below(string_view key, get_kv_callback *callback, void *arg)
{
	LOG("get_below key<" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto first = my_btree->begin();
	auto last = my_btree->lower_bound(key);

	return iterate(first, last, callback, arg);
}

/* get between (key1, key2), key1 exclusive, key2 exclusive */
status stree::get_between(string_view key1, string_view key2, get_kv_callback *callback,
			  void *arg)
{
	LOG("get_between key range=[" << std::string(key1.data(), key1.size()) << ","
				      << std::string(key2.data(), key2.size()) << ")");
	check_outside_tx();

	if (my_btree->key_comp()(key1, key2)) {
		auto first = my_btree->upper_bound(key1);
		auto last = my_btree->lower_bound(key2);

		return iterate(first, last, callback, arg);
	}

	return status::OK;
}

status stree::exists(string_view key)
{
	LOG("exists for key=" << std::string(key.data(), key.size()));
	check_outside_tx();

	internal::stree::btree_type::iterator it = my_btree->find(key);
	if (it == my_btree->end()) {
		LOG("  key not found");
		return status::NOT_FOUND;
	}
	return status::OK;
}

status stree::get(string_view key, get_v_callback *callback, void *arg)
{
	LOG("get using callback for key=" << std::string(key.data(), key.size()));
	check_outside_tx();

	internal::stree::btree_type::iterator it = my_btree->find(key);
	if (it == my_btree->end()) {
		LOG("  key not found");
		return status::NOT_FOUND;
	}

	callback(it->second.c_str(), it->second.size(), arg);
	return status::OK;
}

status stree::put(string_view key, string_view value)
{
	LOG("put key=" << std::string(key.data(), key.size())
		       << ", value.size=" << std::to_string(value.size()));
	check_outside_tx();

	auto result = my_btree->try_emplace(key, value);
	if (!result.second) { // key already exists, so update
		typename internal::stree::btree_type::value_type &entry = *result.first;
		transaction::manual tx(pmpool);
		entry.second = value;
		transaction::commit();
	}
	return status::OK;
}

status stree::remove(string_view key)
{
	LOG("remove key=" << std::string(key.data(), key.size()));
	check_outside_tx();

	auto result = my_btree->erase(key);
	return (result == 1) ? status::OK : status::NOT_FOUND;
}

void stree::Recover()
{
	if (!OID_IS_NULL(*root_oid)) {
		my_btree = (internal::stree::btree_type *)pmemobj_direct(*root_oid);
		my_btree->key_comp().runtime_initialize(
			internal::extract_comparator(*config));
	} else {
		pmem::obj::transaction::run(pmpool, [&] {
			pmem::obj::transaction::snapshot(root_oid);
			*root_oid =
				pmem::obj::make_persistent<internal::stree::btree_type>()
					.raw();
			my_btree =
				(internal::stree::btree_type *)pmemobj_direct(*root_oid);
			my_btree->key_comp().initialize(
				internal::extract_comparator(*config));
		});
	}
}

internal::iterator<false> *stree::new_iterator()
{
	return new stree_iterator<false>{my_btree};
}

internal::iterator<true> *stree::new_const_iterator()
{
	return new stree_iterator<true>{my_btree};
}

template <bool IsConst>
stree::stree_iterator<IsConst>::stree_iterator(container_type *c)
    : container(c), _it(container->begin()), pop(pmem::obj::pool_by_vptr(c))
{
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek(string_view key)
{
	_it = container->find(key);
	if (_it != container->end())
		return status::OK;

	return status::NOT_FOUND;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_lower(string_view key)
{
	_it = container->lower_bound(key);
	if (_it == container->begin()) {
		_it = container->end();
		return status::NOT_FOUND;
	}

	--_it;

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_lower_eq(string_view key)
{
	_it = container->lower_bound(key);
	if (container->size() == 0 ||
	    (_it == container->begin() &&
	     key.compare(string_view{_it->first.data(), _it->first.size()}) != 0)) {
		_it = container->end();
		return status::NOT_FOUND;
	}

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_higher(string_view key)
{
	_it = container->upper_bound(key);
	if (_it == container->end())
		return status::NOT_FOUND;

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_higher_eq(string_view key)
{
	_it = container->upper_bound(key);
	if (container->size() == 0 ||
	    (_it-- == container->end() &&
	     key.compare(string_view{_it->first.data(), _it->first.size()}) != 0)) {
		_it = container->end();
		return status::NOT_FOUND;
	}

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_to_first()
{
	if (container->size() == 0)
		return status::NOT_FOUND;

	_it = container->begin();

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::seek_to_last()
{
	if (container->size() == 0)
		return status::NOT_FOUND;

	_it = container->end();
	--_it;

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::next()
{
	if (_it == container->end() || ++_it == container->end())
		return status::NOT_FOUND;

	return status::OK;
}

template <bool IsConst>
status stree::stree_iterator<IsConst>::prev()
{
	if (_it == container->begin())
		return status::NOT_FOUND;

	--_it;

	return status::OK;
}

template <bool IsConst>
std::pair<string_view, status> stree::stree_iterator<IsConst>::key()
{
	if (_it == container->end())
		return {{}, status::NOT_FOUND};

	return {_it->first.cdata(), status::OK};
}

template <>
std::pair<string_view, status> stree::stree_iterator<true>::value()
{
	if (_it == container->end())
		return {{}, status::NOT_FOUND};

	return {{_it->second.data()}, status::OK};
}

template <>
std::pair<internal::accessor_base *, status> stree::stree_iterator<false>::value()
{
	if (_it == container->end())
		return {{}, status::NOT_FOUND};

	return {new stree_accessor{_it, pop}, status::OK};
}

stree::stree_accessor::stree_accessor(container_type::iterator it,
				      pmem::obj::pool_base &pop)
    : non_volatile_accessor(pop), _it(it)
{
}

std::pair<pmem::obj::slice<const char *>, status>
stree::stree_accessor::read_range(size_t pos, size_t n)
{
	if (pos + n > _it->second.size())
		n = _it->second.size() - pos;

	return {_it->second.crange(pos, n), status::OK};
}

std::pair<pmem::obj::slice<char *>, status> stree::stree_accessor::write_range(size_t pos,
									       size_t n)
{
	if (pos + n > _it->second.size())
		n = _it->second.size() - pos;

	return {_it->second.range(pos, n), status::OK};
}

} // namespace kv
} // namespace pmem
