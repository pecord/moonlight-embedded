/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_SDL

#include "sdl.h"
#include "input/sdlinput.h"

#include <Limelight.h>

#include <SDL_opengles2.h>

static bool done;
static int fullscreen_flags;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *bmp;

SDL_mutex *mutex;

typedef GLubyte* (APIENTRY * glGetString_Func)(unsigned int);
glGetString_Func glGetStringAPI = NULL;

void sdl_init(int width, int height, bool fullscreen) {
  int drv_index = -1;
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

  fullscreen_flags = fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0;
  window = SDL_CreateWindow("Moonlight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | fullscreen_flags);
  if(!window) {
    fprintf(stderr, "SDL: could not create window - exiting\n");
    exit(1);
  }

  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, ctx);

  glGetStringAPI = (glGetString_Func)SDL_GL_GetProcAddress("glGetString");

  SDL_Log("Available renderers:\n");
  SDL_Log("\n");
  for(int it = 0; it < SDL_GetNumRenderDrivers(); it++) {
      SDL_RendererInfo info;
      SDL_GetRenderDriverInfo(it,&info);

      SDL_Log("%s\n", info.name);

      if(strcmp("opengles2", info.name) == 0)
          drv_index = it;
  }
  SDL_Log("\n");
  SDL_Log("Vendor     : %s\n", glGetStringAPI(GL_VENDOR));
  SDL_Log("Renderer   : %s\n", glGetStringAPI(GL_RENDERER));
  SDL_Log("Version    : %s\n", glGetStringAPI(GL_VERSION));
  SDL_Log("Extensions : %s\n", glGetStringAPI(GL_EXTENSIONS));

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

  SDL_SetRelativeMouseMode(SDL_TRUE);
  renderer = SDL_CreateRenderer(window, drv_index, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    exit(1);
  }

  SDL_ShowCursor(SDL_DISABLE);

  bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!bmp) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    exit(1);
  }

  mutex = SDL_CreateMutex();
  if (!mutex) {
    fprintf(stderr, "Couldn't create mutex\n");
    exit(1);
  }


  sdlinput_init();
}

void sdl_loop() {
  SDL_Event event;
  while(!done && SDL_WaitEvent(&event)) {
    switch (sdlinput_handle_event(&event)) {
    case SDL_QUIT_APPLICATION:
      done = true;
      break;
    case SDL_TOGGLE_FULLSCREEN:
      fullscreen_flags ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
      SDL_SetWindowFullscreen(window, fullscreen_flags);
    case SDL_MOUSE_GRAB:
      SDL_SetRelativeMouseMode(SDL_TRUE);
      break;
    case SDL_MOUSE_UNGRAB:
      SDL_SetRelativeMouseMode(SDL_FALSE);
      break;
    default:
      if (event.type == SDL_QUIT)
        done = true;
      else if (event.type == SDL_USEREVENT) {
        if (event.user.code == SDL_CODE_FRAME) {
          if (SDL_LockMutex(mutex) == 0) {
            Uint8** data = ((Uint8**) event.user.data1);
            int* linesize = ((int*) event.user.data2);
            SDL_UpdateYUVTexture(bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, bmp, NULL, NULL);
            SDL_RenderPresent(renderer);
            SDL_UnlockMutex(mutex);
          } else
            fprintf(stderr, "Couldn't lock mutex\n");
        }
      }
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
}

#endif /* HAVE_SDL */
