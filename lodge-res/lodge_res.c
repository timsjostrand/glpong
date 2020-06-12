#include "lodge_res.h"

#include "array.h"
#include "str.h"
#include "lodge_assert.h"
#include "lodge_platform.h"

#include "log.h"

#include <stdlib.h>

#define LODGE_RES_MAX				256
#define LODGE_RES_NAME_MAX			128
#define LODGE_RES_DEPS_MAX			32
#define LODGE_RES_USERDATA_MAX		16
#define LODGE_RES_DATA_COUNT_MAX	1024

struct lodge_res;

struct lodge_res_deps
{
	struct lodge_res_handle		deps[LODGE_RES_DEPS_MAX];
	uint32_t					count;
};

struct lodge_res
{
	struct lodge_res_desc		desc;

	char						names[LODGE_RES_MAX][LODGE_RES_NAME_MAX];
	uint32_t					ref_counts[LODGE_RES_MAX];
	uint32_t					count;

	array_t						data;
	uint32_t					data_indices[LODGE_RES_MAX];

	struct lodge_res_deps		deps[LODGE_RES_MAX];

	void*						userdata[LODGE_RES_USERDATA_MAX]; 

	// TODO(TS): reverse deps also?
};

// TODO(TS): sort by res_name, binary search get

static int64_t lodge_res_find_by_name(const struct lodge_res *res, strview_t name)
{
	for(uint32_t i = 0, count = res->count; i < count; i++) {
		strview_t res_name = strview_wrap(res->names[i]);

		if(strview_equals(res_name, name)) {
			ASSERT(i < LODGE_RES_MAX);
			return i;
		}
	}
	return -1;
}

struct lodge_res* lodge_res_new(struct lodge_res_desc desc)
{
	struct lodge_res* res = (struct lodge_res * )malloc(sizeof(struct lodge_res));
	lodge_res_new_inplace(res, desc);
	return res;
}

void lodge_res_new_inplace(struct lodge_res *res, struct lodge_res_desc desc)
{
	*res = (struct lodge_res) { 0 };

	res->desc = desc;
	res->data = array_new(desc.size, LODGE_RES_DATA_COUNT_MAX);
}

void lodge_res_free_inplace(struct lodge_res *res)
{
	array_free(res->data);
}

void lodge_res_free(struct lodge_res *res)
{
	lodge_res_free_inplace(res);
	free(res);
}

size_t lodge_res_sizeof()
{
	return sizeof(struct lodge_res);
}

const void* lodge_res_get_index(struct lodge_res *res, strview_t name, int64_t index)
{
	debugf("Res", "Get: `" STRVIEW_PRINTF_FMT "`:`" STRVIEW_PRINTF_FMT "`\n",
		STRVIEW_PRINTF_ARG(res->desc.name),
		STRVIEW_PRINTF_ARG(name)
	);

	if(index >= 0) {
		uint32_t *refcount = &res->ref_counts[index];
		//ASSERT(*refcount > 0);
		(*refcount)++;
		return array_get(res->data, (size_t)index);
	} else {
		index = res->count++;

		uint32_t *refcount = &res->ref_counts[index];
		ASSERT(*refcount == 0);

		void* data_element = array_append_no_init(res->data);
		const size_t data_index = array_count(res->data) - 1;

		res->data_indices[index] = data_index;

		strbuf_wrap_and(res->names[index], strbuf_set, name);

		// TODO(TS): check return of new_inplace

		if(!res->desc.new_inplace(res, name, data_element, res->desc.size)) {
			// FIXME(TS): roll back and clean up after failed new_inplace
			return NULL;
		}

		(*refcount)++;

		return data_element;
	}
}

const void* lodge_res_get(struct lodge_res *res, strview_t name)
{
	const int64_t index = lodge_res_find_by_name(res, name);
	return lodge_res_get_index(res, name, index);
}

static void lodge_res_release_index(struct lodge_res *res, size_t index)
{
	debugf("Res", "Release: `" STRVIEW_PRINTF_FMT "`:`" STRVIEW_PRINTF_FMT "`\n",
		STRVIEW_PRINTF_ARG(res->desc.name),
		STRVIEW_PRINTF_ARG(strview_wrap(res->names[index]))
	);

	uint32_t *refcount = &res->ref_counts[index];
	ASSERT(*refcount > 0);

	(*refcount)--;

	if(*refcount == 0) {
#if 0
		ASSERT_FAIL("Refcount == 0");

		const uint32_t data_index = res->data_indices[index];
		void *res_data = array_get(res->data, data_index);
		res->desc.free_inplace(res, name, res_data);
#endif
	}

	// TODO(TS): vacuum dependencies. maybe release dependencies also?
}

void lodge_res_release(struct lodge_res *res, strview_t name)
{
	const int64_t index = lodge_res_find_by_name(res, name);
	if(index < 0) {
		ASSERT_FAIL("Asset not found");
		return;
	}
	lodge_res_release_index(res, (size_t)index);
}

void lodge_res_reload(struct lodge_res *res, strview_t name)
{
	debugf("Res", "Reload: `" STRVIEW_PRINTF_FMT "`:`" STRVIEW_PRINTF_FMT "`\n",
		STRVIEW_PRINTF_ARG(res->desc.name),
		STRVIEW_PRINTF_ARG(name)
	);

	const int64_t index = lodge_res_find_by_name(res, name);
	if(index < 0) {
		ASSERT_FAIL("Asset not found");
		return;
	}

	ASSERT(index < LODGE_RES_MAX);

	// FIXME(TS): should try reload first, and roll back changes on failure

	if(res->desc.reload_inplace)
	{
		ASSERT_NOT_IMPLEMENTED();
	}
	else
	{
		const size_t data_index = res->data_indices[index];

		void* data_element = array_get(res->data, data_index);

		res->desc.free_inplace(res, name, data_element);

		if(!res->desc.new_inplace(res,
			name,
			data_element,
			res->desc.size)) {
			ASSERT_FAIL("TODO: implement rollback");
			return;
		}
	}

	for(int dep_index = 0, count = res->deps[index].count; dep_index < count; dep_index++) {
		struct lodge_res_handle *res_handle = &res->deps[index].deps[dep_index];
		lodge_res_reload(res_handle->resources, res_handle->id);
	}
}

static void lodge_res_append_dep(struct lodge_res *res, size_t index, struct lodge_res_handle dependency)
{
	ASSERT(index < LODGE_RES_MAX);
	struct lodge_res_deps *deps = &res->deps[index];

	ASSERT(deps->count + 1 < LODGE_RES_DEPS_MAX);
	if(deps->count + 1 < LODGE_RES_DEPS_MAX) {
		const uint32_t dep_index = res->deps[index].count++;
		ASSERT(dep_index < LODGE_RES_DEPS_MAX);
		res->deps[index].deps[dep_index] = dependency;
	}
}

static void lodge_res_remove_dep(struct lodge_res *res, size_t index, struct lodge_res_handle dependency)
{
	ASSERT(index < LODGE_RES_MAX);
	struct lodge_res_deps *deps = &res->deps[index];

	for(uint32_t dep_index = 0, count = deps->count; dep_index < count; dep_index++) {
		struct lodge_res_handle *handle = &deps->deps[dep_index];
		if(handle->resources == dependency.resources
			&& strview_equals(handle->id, dependency.id)) {
			// Not implemented for dep_index > 0, need to memshift and stuff
			ASSERT(dep_index == 0);
			deps->count--;
			return;
		}
	}

	ASSERT_FAIL("Dependency not found");
}

const void* lodge_res_get_depend(struct lodge_res *res, strview_t name, struct lodge_res_handle dependency)
{
	// TODO(TS): need to check if this res is already a dependency (only once to not trigger reload multiple times)
	const void* data = lodge_res_get(res, name);
	ASSERT(data);

	const int64_t index = lodge_res_find_by_name(res, name);
	if(index < 0) {
		ASSERT_FAIL("Asset not found");
		return NULL;
	}

	lodge_res_append_dep(res, (size_t)index, dependency);

	return data;
}

void lodge_res_release_depend(struct lodge_res *res, strview_t name, struct lodge_res_handle dependency)
{
	const int64_t index = lodge_res_find_by_name(res, name);
	if(index < 0) {
		ASSERT_FAIL("Asset not found");
		return;
	}

	lodge_res_release_index(res, (size_t)index);
	lodge_res_remove_dep(res, (size_t)index, dependency);
}

void lodge_res_set_userdata(struct lodge_res *res, size_t index, void *userdata)
{
	ASSERT(index < LODGE_RES_USERDATA_MAX);
	res->userdata[index] = userdata;
}

void* lodge_res_get_userdata(struct lodge_res *res, size_t index)
{
	ASSERT(index < LODGE_RES_USERDATA_MAX);
	return res->userdata[index];
}
