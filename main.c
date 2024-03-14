#include <raylib.h>
#include <stdlib.h>
#include <string.h>

#include "fonts/DMSans-Regular.c"

// Comparisons
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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
    if (f.baseSize == size) {
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

typedef struct {
  bool ready;
  bool picture;
  size_t start;
  union {
    Text text;
    Texture image;
  };
} Slide;

void slideDraw(Slide *s, Fonts *fonts, Buffer *text, Buffer *paths) {
  int w = GetScreenWidth();
  int h = GetScreenHeight();

  if (s->picture) {
    if (!s->ready) {
      s->ready = true;
      s->image = LoadTexture(paths->data + s->start);
    }

    float scale = min(w * VIEWPORT / s->image.width, h * VIEWPORT / s->image.height);
    Vector2 position = {
      .x = (w - s->image.width * scale) / 2.0,
      .y = (h - s->image.height * scale) / 2.0,
    };

    DrawTextureEx(s->image, position, 0, scale, WHITE);
  } else {
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
  }
}

typedef struct {
  Slide *data;
  size_t count;
  size_t capacity;
} Slides;

void slidesLoad(Slides *s, Buffer *text, Buffer *paths, const char *file) {
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
      slide.picture = true;
      slide.start = paths->count;

      listAppendMany(paths, line.data + 1, line.count - 1);
      listAppend(paths, '\0');

      while (line.count) {
        line = strSplit(&content, '\n');
      }
    } else {
      slide.picture = false;
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
}

// Main
int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "Slides");
  SetExitKey(KEY_Q);

  Fonts fonts = {0};
  Buffer text = {0};
  Buffer paths = {0};
  Slides slides = {0};

  if (argc == 2) {
    slidesLoad(&slides, &text, &paths, argv[1]);
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
      slideDraw(&slides.data[current], &fonts, &text, &paths);
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
        current = 0;
        text.count = 0;
        paths.count = 0;
        slides.count = 0;
        slidesLoad(&slides, &text, &paths, list.paths[0]);
      }

      UnloadDroppedFiles(list);
    }
  }

  free(text.data);
  free(paths.data);

  for (size_t i = 0; i < fonts.count; i++) {
    UnloadFont(fonts.data[i]);
  }
  free(fonts.data);

  for (size_t i = 0; i < slides.count; i++) {
    Slide s = slides.data[i];
    if (s.picture) {
      UnloadTexture(s.image);
    }
  }
  free(slides.data);

  CloseWindow();
}
