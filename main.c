﻿#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "game.h"
#include "log.h"
#include "vfs.h"
#include "assets.h"

#include "core.h"
#include "core_argv.h"
#include "util_reload.h"

typedef struct lodge_settings* (*game_get_settings_fn_t)();

void* game_library = 0;

core_init_t game_init_fn = 0;
core_load_t game_assets_load_fn = 0;
core_release_t game_assets_release_fn = 0;
think_func_t game_think_fn = 0;
render_func_t game_render_fn = 0;
lodge_window_input_callback_t game_key_callback_fn = 0;
lodge_window_mousebutton_callback_t game_mousebutton_callback_fn = 0;
fps_func_t game_fps_callback_fn = 0;
core_console_init_t game_console_init_fn = 0;
core_init_memory_t game_init_memory_fn = 0;
game_get_settings_fn_t game_get_settings_fn = NULL;

void* load_shared_library(const char* filename)
{
	void* library = 0;

#ifdef _WIN32
	library = (void*)LoadLibrary(filename);
#else
	library = dlopen(filename, 2);
#endif

	if (!library)
	{
		errorf("Main", "Could not load library %s\n", filename);
	}

    return library;
}

void* load_function(void* library, const char* function_name)
{
	void* function = 0;

#ifdef _WIN32
	function = (void*)GetProcAddress((HINSTANCE)library, function_name);
#else
	function = dlsym(library, function_name);
#endif

	if (!function)
	{
		errorf("Main", "Could not load function %s\n", function_name);
	}

	return function;
}

/**
 * @return 0 on success, non-zero on failure.
 */
int free_shared_library(void* library)
{
#ifdef _WIN32
	return FreeLibrary((HINSTANCE)library) == 0;
#else
	return dlclose(library);
#endif
}

void* load_copy(const char* filename, unsigned int size, void* data)
{
	char name[256];

	char* tmp = strrchr(filename, (int)'/');
	if (tmp)
	{
		tmp++;
		int namebegin = strlen(filename) - strlen(tmp);
		strcpy(name, filename);
		strcpy(&name[namebegin], "runtime_");
		strcpy(&name[namebegin + 8], tmp);
	}
	else
	{
		strcpy(name, "runtime_");
		strcpy(name, filename);
	}

	FILE *fp;
	fp = fopen(name, "wb+");
	fwrite(data, sizeof(char), (size_t)size, fp);
	fclose(fp);

	return load_shared_library(name);
}

int first_load = 1;

void load_game(const char* filename, unsigned int size, void* data, void* userdata)
{
	debugf("Main", "Reloading game library %s\n", filename);

	if (game_library)
	{
		free_shared_library(game_library);
		debugf("Main", "Freeing old game library %s\n", filename);
		game_library = 0;
	}

	const char* absolute_path = vfs_get_absolute_path(filename);

	if (!absolute_path)
	{
		return;
	}


	game_library = load_copy(absolute_path, size, data);
	debugf("Main", "Load successful %s\n", filename);
	if (game_library)
	{
		game_init_fn = load_function(game_library, "game_init");
		game_assets_load_fn = load_function(game_library, "game_assets_load");
		game_assets_release_fn = load_function(game_library, "game_assets_release");
		game_think_fn = load_function(game_library, "game_think");
		game_render_fn = load_function(game_library, "game_render");
		game_key_callback_fn = load_function(game_library, "game_key_callback");
		game_mousebutton_callback_fn = load_function(game_library, "game_mousebutton_callback");
		game_fps_callback_fn = load_function(game_library, "game_fps_callback");
		game_console_init_fn = load_function(game_library, "game_console_init");
		game_init_memory_fn = load_function(game_library, "game_init_memory");
		game_get_settings_fn = load_function(game_library, "game_get_settings");

		if (game_init_fn &&
			game_assets_load_fn &&
			game_assets_release_fn &&
			game_think_fn &&
			game_render_fn &&
			game_key_callback_fn &&
			game_mousebutton_callback_fn &&
			game_fps_callback_fn &&
			game_console_init_fn &&
			game_init_memory_fn)
		{
			core_set_asset_callbacks(game_assets_load_fn, game_init_fn, game_assets_release_fn);
			core_set_key_callback(game_key_callback_fn);
			core_set_mousebutton_callback(game_mousebutton_callback_fn);
			core_set_fps_callback(game_fps_callback_fn);
			core_set_console_init_callback(game_console_init_fn);
			core_set_think_callback(game_think_fn);
			core_set_render_callback(game_render_fn);
			core_set_init_memory_callback(game_init_memory_fn);

			if (!first_load)
			{
				core_reload();
			}
		}
	}
}

int main(int argc, char **argv)
{
	/* Parse command line arguments. */
	struct core_argv args = { 0 };
	core_argv_parse(&args, argc, argv);

	/* Start the virtual file system */
	vfs_init(args.mount);

#ifdef LOAD_SHARED
	/* Load game library */
	size_t filesize;
	void* lib = vfs_get_file(args.game, &filesize);
	load_game(args.game, filesize, lib, 0);
	first_load = 0;

	if (!game_library)
	{
		return 0;
	}

	struct lodge_settings *settings = game_get_settings_fn();

#else
	core_set_think_callback(&game_think);
	core_set_render_callback(&game_render);
	core_set_asset_callbacks(&game_assets_load, &game_init, &game_assets_release);
	core_set_key_callback(&game_key_callback);
	core_set_mousebutton_callback(&game_mousebutton_callback);
	core_set_fps_callback(&game_fps_callback);
	core_set_init_memory_callback(&game_init_memory);
	core_set_console_init_callback(&game_console_init);
	struct lodge_settings *settings = game_get_settings();
#endif

	/* Sound setup */
	core_set_up_sound(&settings->sound_listener, settings->sound_distance_max);

	/* Initialize subsystems and run main loop. */
	core_setup(settings->window_title,
		settings->view_width, settings->view_height,
		settings->window_width, settings->window_height,
		args.window_mode, 1000000);

	vfs_run_callbacks();

#ifdef LOAD_SHARED
	/* Register game reload callback */
	vfs_register_callback(args.game, &load_game, 0);
#endif

	core_run();
	vfs_shutdown();

	return 0;
}
