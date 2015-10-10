/**
* Animated sprites using spritebatch and atlas
*
* Author: Johan Yngman <johan.yngman@gmailcom>
*/

#include "animatedsprites.h"

#include <stdlib.h>

struct animatedsprites* animatedsprites_create()
{
	struct animatedsprites* as = (struct animatedsprites*)malloc(sizeof(struct animatedsprites));
	as->sprite_todraw_count = 0;
	spritebatch_create(&as->spritebatch);
	return as;
}

void animatedsprites_destroy(struct animatedsprites* animatedsprites)
{
	spritebatch_destroy(&animatedsprites->spritebatch);
	free(animatedsprites);
}

void animatedsprites_update(struct animatedsprites* animatedsprites, struct atlas* atlas, float delta_time)
{
	spritebatch_begin(&animatedsprites->spritebatch);

	for (int i = 0; i < animatedsprites->sprite_todraw_count; i++)
	{
		struct sprite* current_sprite = animatedsprites->sprites_todraw[i];

		if (!current_sprite->done)
		{
			current_sprite->frame_time += delta_time;

			while (current_sprite->frame_time > current_sprite->frame_length)
			{
				current_sprite->frame_time -= current_sprite->frame_length;

				if (current_sprite->frame_current < current_sprite->frame_start + current_sprite->frame_count - 1)
				{
					current_sprite->frame_current++;
				}
				else if (current_sprite->looping)
				{
					current_sprite->frame_current = current_sprite->frame_start;
				}
				else
				{
					current_sprite->done = 1;
				}
			}
		}

		int index = current_sprite->frame_current;

		vec2 tex_pos;
		tex_pos[0] = atlas->frames[index].x / (float)atlas->width;
		tex_pos[1] = atlas->frames[index].y / (float)atlas->height;

		vec2 tex_bounds;
		tex_bounds[0] = atlas->frames[index].width / (float)atlas->width;
		tex_bounds[1] = atlas->frames[index].height / (float)atlas->height;

		vec2 scale;
		scale[0] = atlas->frames[index].width * current_sprite->scale[0];
		scale[1] = atlas->frames[index].height * current_sprite->scale[1];

		spritebatch_add(&animatedsprites->spritebatch, current_sprite->position, scale, tex_pos, tex_bounds);
	}

	spritebatch_end(&animatedsprites->spritebatch);
}

void animatedsprites_render(struct animatedsprites* animatedsprites, struct shader *s, struct graphics *g, GLuint tex)
{
	spritebatch_render(&animatedsprites->spritebatch, s, g, tex);
}

void animatedsprites_add(struct animatedsprites* animatedsprites, struct sprite* sprite)
{
	animatedsprites->sprites_todraw[animatedsprites->sprite_todraw_count] = sprite;
	animatedsprites->sprite_todraw_count++;
}

void animatedsprites_clear(struct animatedsprites* animatedsprites)
{
	animatedsprites->sprite_todraw_count = 0;
}

void animatedsprites_playanimation(struct sprite* sprite, int frame_start, int frame_count, float frame_length, int looping)
{
	sprite->done = 0;
	sprite->looping = looping;
	sprite->frame_start = sprite->frame_current = frame_start;
	sprite->frame_count = frame_count;
	sprite->frame_length = frame_length;
}