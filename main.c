#include <raylib.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "fonts/DMSans-Regular.c"

// Comparisons
#define min(a, b) ((a) < (b) ? (a) : (b))
#define diff(a, b) ((a) > (b) ? ((a) - (b)) : ((b) - (a)))

// List
#define LIST_INIT_CAP 128

#define listAppend(l, v)                                                                           \
  do {                                                                                             \
    if ((l)->count >= (l)->capacity) {                                                             \
      (l)->capacity = (l)->capacity == 0 ? LIST_INIT_CAP : (l)->capacity * 2;                      \
      (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                          \
    }                                                                                              \
                                                                                                   \
    (l)->data[(l)->count++] = (v);                                                                 \
  } while (0)

#define listAppendMany(l, v, c)                                                                    \
  do {                                                                                             \
    if ((l)->count + (c) > (l)->capacity) {                                                        \
      if ((l)->capacity == 0) {                                                                    \
        (l)->capacity = LIST_INIT_CAP;                                                             \
      }                                                                                            \
                                                                                                   \
      while ((l)->count + (c) > (l)->capacity) {                                                   \
        (l)->capacity *= 2;                                                                        \
      }                                                                                            \
                                                                                                   \
      (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                          \
    }                                                                                              \
                                                                                                   \
    memcpy((l)->data + (l)->count, (v), (c) * sizeof(*(l)->data));                                 \
    (l)->count += (c);                                                                             \
  } while (0)

// Str
typedef struct {
  const char *data;
  size_t count;
} Str;

Str strSplit(Str *s, char c) {
  Str r = *s;

  for (size_t i = 0; i < s->count; ++i) {
    if (s->data[i] == c) {
      r.count = i;
      s->data += i + 1;
      s->count -= i + 1;
      return r;
    }
  }

  s->data += s->count;
  s->count = 0;
  return r;
}

// Font
#define FONT_BASE 60
#define FONTS_CAP 20

typedef struct {
  Font *data;
  size_t count;
  size_t capacity;
} Fonts;

Font fontsGet(Fonts *fs, size_t size) {
  for (size_t i = 0; i < fs->count; i++) {
    Font f = fs->data[i];
    if (diff(f.baseSize, size) <= 20) {
      return f;
    }
  }

  if (fs->count > FONTS_CAP) {
    for (size_t i = FONTS_CAP; i < fs->count; i++) {
      UnloadFont(fs->data[i]);
    }
    fs->count = FONTS_CAP;
  }

  Font f = LoadFontFromMemory(".ttf", font, font_len, size, 0, 0);
  if (IsFontReady(f)) {
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
  }
  listAppend(fs, f);
  return f;
}

// Slide
#define VIEWPORT 0.8

typedef struct {
  char *data;
  size_t count;
  size_t capacity;
} Buffer;

typedef struct {
  size_t lines;
  size_t width;
} Text;

typedef enum {
  SLIDE_TEXT,
  SLIDE_IMAGE,
  SLIDE_TEXTURE,
} SlideType;

typedef struct {
  bool ready;
  size_t start;

  SlideType type;
  union {
    Text text;
    Image image;
    Texture texture;
  };
} Slide;

void slideFree(Slide *s) {
  if (s->ready) {
    if (s->type == SLIDE_IMAGE) {
      UnloadImage(s->image);
    }

    if (s->type == SLIDE_TEXTURE) {
      UnloadTexture(s->texture);
    }
  }
}

void slideDraw(Slide *s, Fonts *fonts, Buffer *text, Buffer *paths) {
  int w = GetScreenWidth();
  int h = GetScreenHeight();

  if (s->type == SLIDE_TEXT) {
    if (!s->ready) {
      s->ready = true;
      s->text.width = 0;

      Font font = fontsGet(fonts, FONT_BASE);
      const char *p = text->data + s->start;
      for (size_t i = 0; i < s->text.lines; i++) {
        size_t width = MeasureTextEx(font, p, FONT_BASE, 0).x;
        if (s->text.width < width) {
          s->text.width = width;
        }

        p += strlen(p) + 1;
      }
    }

    float scale = min(w * VIEWPORT / s->text.width, h * VIEWPORT / FONT_BASE / s->text.lines);
    size_t size = FONT_BASE * scale;
    Vector2 position = {
      .x = (w - s->text.width * scale) / 2.0,
      .y = (h - size * s->text.lines) / 2.0,
    };

    Font font = fontsGet(fonts, size);
    const char *p = text->data + s->start;
    for (size_t i = 0; i < s->text.lines; i++) {
      DrawTextEx(font, p, position, size, 0, BLACK);
      position.y += size;
      p += strlen(p) + 1;
    }
  } else {
    if (s->type == SLIDE_IMAGE && s->ready) {
      Texture texture = LoadTextureFromImage(s->image);
      UnloadImage(s->image);

      GenTextureMipmaps(&texture);
      SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);

      s->type = SLIDE_TEXTURE;
      s->texture = texture;
    }

    if (s->type == SLIDE_TEXTURE) {
      float scale = min(w * VIEWPORT / s->texture.width, h * VIEWPORT / s->texture.height);
      Vector2 position = {
        .x = (w - s->texture.width * scale) / 2.0,
        .y = (h - s->texture.height * scale) / 2.0,
      };

      DrawTextureEx(s->texture, position, 0, scale, WHITE);
    }
  }
}

typedef struct {
  Slide *data;
  size_t count;
  size_t capacity;

  Buffer paths;
  pthread_t thread;
} Slides;

void *slidesLoad(void *arg) {
  Slides *s = arg;

  for (size_t i = 0; i < s->count; i++) {
    Slide *slide = &s->data[i];
    if (slide->type == SLIDE_IMAGE) {
      slide->image = LoadImage(s->paths.data + slide->start);
      slide->ready = true;
    }
  }

  return NULL;
}

void slidesStopLoader(Slides *s) {
  if (s->thread) {
    pthread_cancel(s->thread);
    pthread_join(s->thread, NULL);
    s->thread = 0;
  }
}

void slidesParse(Slides *s, Buffer *text, const char *file) {
  int count;
  unsigned char *data = LoadFileData(file, &count);
  if (!data) {
    return;
  }

  Str content = {(const char *)data, count};
  while (content.count) {
    Str line = strSplit(&content, '\n');
    while (line.count == 0 || *line.data == '#') {
      line = strSplit(&content, '\n');
    }

    Slide slide = {0};
    if (*line.data == '@') {
      slide.type = SLIDE_IMAGE;
      slide.start = s->paths.count;

      listAppendMany(&s->paths, line.data + 1, line.count - 1);
      listAppend(&s->paths, '\0');

      while (line.count) {
        line = strSplit(&content, '\n');
      }
    } else {
      slide.type = SLIDE_TEXT;
      slide.start = text->count;

      while (line.count) {
        if (*line.data == '\\') {
          if (line.count > 1) {
            listAppendMany(text, line.data + 1, line.count - 1);
            listAppend(text, '\0');
            slide.text.lines++;
          }
        } else if (*line.data != '#') {
          listAppendMany(text, line.data, line.count);
          listAppend(text, '\0');
          slide.text.lines++;
        }

        line = strSplit(&content, '\n');
      }

      if (slide.text.lines == 0) {
        continue;
      }
    }

    listAppend(s, slide);
  }

  UnloadFileData(data);

  if (pthread_create(&s->thread, NULL, slidesLoad, s)) {
    TraceLog(LOG_ERROR, "Could not start loader thread");
  }
}

// Main
int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "Slides");
  SetExitKey(KEY_Q);

  Fonts fonts = {0};
  Buffer text = {0};
  Slides slides = {0};

  if (argc == 2) {
    slidesParse(&slides, &text, argv[1]);
  }

  const char *message = "Drag and Drop slideshow file";
  size_t messageWidth = MeasureTextEx(fontsGet(&fonts, FONT_BASE), message, FONT_BASE, 0).x;

  size_t current = 0;
  while (!WindowShouldClose()) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    if (slides.count == 0) {
      float scale = min(w * VIEWPORT / messageWidth, h * VIEWPORT / FONT_BASE);
      size_t size = FONT_BASE * scale;
      Vector2 position = {
        .x = (w - messageWidth * scale) / 2.0,
        .y = (h - size) / 2.0,
      };

      DrawTextEx(fontsGet(&fonts, size), message, position, size, 0, BLACK);
    } else {
      slideDraw(&slides.data[current], &fonts, &text, &slides.paths);
    }
    EndDrawing();

    if (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_L) || IsKeyPressed(KEY_DOWN) ||
        IsKeyPressed(KEY_RIGHT)) {
      if (current + 1 < slides.count) {
        current++;
      }
    }

    if (IsKeyPressed(KEY_K) || IsKeyPressed(KEY_H) || IsKeyPressed(KEY_UP) ||
        IsKeyPressed(KEY_LEFT)) {
      if (current > 0) {
        current--;
      }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      Vector2 mouse = GetMousePosition();
      if (mouse.x >= 0.75 * w) {
        if (current + 1 < slides.count) {
          current++;
        }
      }

      if (mouse.x <= 0.25 * w) {
        if (current > 0) {
          current--;
        }
      }
    }

    if (IsFileDropped()) {
      FilePathList list = LoadDroppedFiles();

      if (list.count > 0) {
        slidesStopLoader(&slides);

        current = 0;
        text.count = 0;
        slides.paths.count = 0;

        for (size_t i = 0; i < slides.count; i++) {
          slideFree(&slides.data[i]);
        }
        slides.count = 0;

        slidesParse(&slides, &text, list.paths[0]);
      }

      UnloadDroppedFiles(list);
    }
  }

  slidesStopLoader(&slides);

  free(text.data);
  free(slides.paths.data);

  for (size_t i = 0; i < fonts.count; i++) {
    UnloadFont(fonts.data[i]);
  }
  free(fonts.data);

  for (size_t i = 0; i < slides.count; i++) {
    slideFree(&slides.data[i]);
  }
  free(slides.data);

  CloseWindow();
}
