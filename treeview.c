#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include "treeview.h"
#include "textrenderutils.h"

#define ITEMS_PER_BATCH 64

typedef enum {
    ItemType_DIR,
    ItemType_FILE,
    ItemType_OTHER,
} ItemType;

typedef struct Item Item;
struct Item {
    char   name[256];
    size_t name_len;
    ItemType type;
    bool     open;
    Item *children;
    Item *next;
};

typedef struct ItemBatch ItemBatch;
struct ItemBatch {
    ItemBatch *prev;
    Item       list[ITEMS_PER_BATCH];
};

typedef struct {
    Item *free_list;
    ItemBatch *tail;
    ItemBatch  head;
} ItemPool;

static void initBatch(ItemBatch *batch)
{
    for (size_t i = 0; i < ITEMS_PER_BATCH-1; i++)
        batch->list[i].next = &batch->list[i+1];
    batch->list[ITEMS_PER_BATCH-1].next = NULL;
    batch->prev = NULL;
}

static void linkBatch(ItemBatch *batch, ItemPool *pool)
{
    batch->prev = pool->tail;
    pool->tail = batch;

    batch->list[ITEMS_PER_BATCH-1].next = pool->free_list;
    pool->free_list = batch->list;
}

static void ItemPool_init(ItemPool *pool)
{
    ItemBatch *head = &pool->head;
    pool->tail = NULL;
    pool->free_list = NULL;
    initBatch(head);
    linkBatch(head, pool);
}

static void ItemPool_free(ItemPool *pool)
{
    ItemBatch *batch = pool->tail;
    while (batch->prev != NULL) {
        ItemBatch *prev = batch->prev;
        free(batch);
        batch = prev;
    }
}

static bool ItemPool_grow(ItemPool *pool)
{
    ItemBatch *batch = malloc(sizeof(ItemBatch));
    if (batch != NULL) {
        initBatch(batch);
        linkBatch(batch, pool);
        return true;
    }
    return false;
}

static Item *ItemPool_getSlot(ItemPool *pool)
{
    bool no_free_slots_left = (pool->free_list == NULL);

    if (no_free_slots_left) {
        bool failed_to_grow_pool = !ItemPool_grow(pool);
        if (failed_to_grow_pool)
            return NULL;
    }

    Item *item = pool->free_list;
    pool->free_list = item->next;

    memset(item, 0, sizeof(Item));
    return item;
}

static Item*
buildDirectoryTree(const char *path, size_t path_len,
                   const char *name, size_t name_len,
                   size_t max_depth, ItemPool *pool)
{
    if (max_depth == 0)
        return NULL;

    if (name_len >= 256)
        return NULL;

    char full_name[1024];
    if (path_len + name_len + 1 >= sizeof(full_name))
        return NULL;

    memcpy(full_name,                path, path_len);
    memcpy(full_name + path_len + 1, name, name_len);
    full_name[path_len] = '/';
    full_name[path_len + name_len + 1] = '\0';

    DIR *dir = opendir(full_name);
    if (dir == NULL)
        return NULL;

    Item *root;
    {
        root = ItemPool_getSlot(pool);
        if (root == NULL) {
            closedir(dir);
            return NULL;
        }
        strcpy(root->name, name);
        root->name_len = name_len;
        root->type = ItemType_DIR;
        root->open = false;
        // root->children can be left uninitialized for now
        root->next = NULL;
    }

    Item **tail = &root->children;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {

        char  *child_name = ent->d_name;
        size_t child_name_len = strlen(child_name);

        if (child_name[0] == '.')
            continue;

        ItemType type;
        bool failed = false;
        switch (ent->d_type) {
            case DT_DIR: type = ItemType_DIR;  break;
            case DT_REG: type = ItemType_FILE; break;
            case DT_UNKNOWN: {
                
                if (path_len + name_len + child_name_len + 1 >= sizeof(full_name))
                    failed = true;
                else {
                    memcpy(full_name + path_len + name_len + 1, child_name, child_name_len);
                    full_name[path_len + name_len + 1 + child_name_len] = '\0';

                    struct stat buffer;
                    if (lstat(full_name, &buffer))
                        // Failed to query file information
                        failed = true;
                    else {
                        switch (buffer.st_mode & S_IFMT) {
                            case S_IFDIR: type = ItemType_DIR;  break;
                            case S_IFREG: type = ItemType_FILE; break;
                            default: type = ItemType_OTHER; break;
                        }
                    }

                    full_name[path_len + name_len + 1] = '\0';
                }
            } break;
            default: type = ItemType_OTHER; break;
        }

        Item *item;
        if (failed)
            item = NULL;
        else {
            if (type == ItemType_DIR)
                item = buildDirectoryTree(full_name, path_len + name_len + 1, 
                                          child_name, child_name_len, max_depth-1,
                                          pool);
            else {
                item = ItemPool_getSlot(pool);
                if (item != NULL) {
                    assert(child_name_len < sizeof(item->name));
                    strcpy(item->name, child_name);
                    item->name_len = child_name_len;
                    item->type = type;
                    item->open = false;
                    item->children = NULL;
                }
            }
        }

        if (item != NULL) {
            *tail = item;
            tail = &item->next;
        }
    }
    *tail = NULL;

    closedir(dir);
    return root;
}

typedef struct {
    GUIElement base;
    Rectangle old_region;
    Scrollbar v_scroll;
    Scrollbar h_scroll;
    Item    *tree;
    ItemPool pool;
    Font     font;
    RenderTexture2D texture;
    float logic_w;
    float logic_h;
    char   path[1024];
    size_t path_len;
    const TreeViewStyle *style;
    void (*callback)(const char*, size_t, void*);
    void *userp;
} TreeView;

static void freeCallback(GUIElement *elem)
{
    TreeView *tv = (TreeView*) elem;
    Scrollbar_free(&tv->v_scroll);
    Scrollbar_free(&tv->h_scroll);
    UnloadRenderTexture(tv->texture);
    UnloadFont(tv->font);
    free(elem);
}

static void onResizeCallback(GUIElement *elem, 
                             Rectangle old_region)
{
    (void) old_region;
    Rectangle region = GUIElement_getRegion(elem);
    TreeView *tv = (TreeView*) elem;
    UnloadRenderTexture(tv->texture);
    tv->texture = LoadRenderTexture(region.width, region.height);
}

static size_t getLineHeight(const TreeViewStyle *style)
{
    return (style->auto_line_height) ? (style->font_size) : (style->line_height);
}


static int getVisibleSubtreeItemByIndex(Item *root, Item **stack, 
                                        size_t *curr_i, size_t queried_i,
                                        size_t *depth, size_t max_depth)
{
    if ((*depth) + 2 >= max_depth)
        return -1; // Error
    stack[(*depth)++] = root;
    
    Item *child = root->children;
    while (child != NULL) {

        if (*curr_i == queried_i) {
            stack[(*depth)++] = child;
            return 1; // Found
        }
        (*curr_i)++;

        if (child->type == ItemType_DIR && child->open == true) {
            int status = getVisibleSubtreeItemByIndex(child, stack, curr_i, 
                                                      queried_i, depth, 
                                                      max_depth);
            if (status != 0) 
                return status; // Either error or found
        }

        child = child->next;
    }

    (*depth)--;
    return 0; // Not found
}

static int getVisibleTreeItemByIndex(TreeView *tv, size_t queried_i, Item **stack, 
                                     size_t *depth, size_t max_depth)
{
    size_t curr_i = 0;
    *depth = 0;

    if (queried_i == 0) {
        if (max_depth == 0)
            return -1;
        stack[(*depth)++] = tv->tree;
        return 1;
    }

    return getVisibleSubtreeItemByIndex(tv->tree, stack, &curr_i, 
                                        queried_i, depth, max_depth);
}

static size_t concatPath(const char *base, size_t base_len,
                         Item **list, size_t count, 
                         char dst[static 1024])
{
    size_t w = 0; // Bytes written

    memcpy(dst, base, base_len);
    w += base_len;
    dst[w++] = '/';

    for (size_t i = 0; i < count; i++) {
        char  *src = list[i]->name;
        size_t len = list[i]->name_len;
        memcpy(dst + w, src, len);
        w += len;
        if (i+1 < count)
            dst[w++] = '/';
    }
    dst[w] = '\0';
    return w;
}

void printSubtree(FILE *stream, Item *root, size_t depth)
{
    Item *child = root->children;
    while (child != NULL) {
        
        const char *typename;
        switch (child->type) {
            case ItemType_DIR: typename = "dir"; break;
            case ItemType_FILE: typename = "file"; break;
            case ItemType_OTHER: typename = "other"; break;
        }

        for (size_t i = 0; i < 2*depth; i++)
            fprintf(stream, " ");
        fprintf(stream, "[%s] (%s)\n", child->name, typename);

        if (child->type == ItemType_DIR && child->open)
            printSubtree(stream, child, depth+1);

        child = child->next;
    }

}

void printTree(FILE *stream, Item *root)
{
    fprintf(stream, "<start>\n");
    fprintf(stream, "[%s] (dir)\n", root->name);
    printSubtree(stream, root, 1);
    fprintf(stream, "<end>\n");
}

static void getMinimumSize(GUIElement *elem, 
                           int *w, int *h)
{
    (void) elem;
    *w = 50;
    *h = 50;
}

static GUIElement*
onClickDownCallback(GUIElement *elem, 
                    int x, int y)
{
    TreeView *tv = (TreeView*) elem;

    bool on_thumb;
    if (Scrollbar_onClickDown(&tv->v_scroll, x, y)) {
        on_thumb = true;
    } else if (Scrollbar_onClickDown(&tv->h_scroll, x, y)) {
        on_thumb = true;
    } else {
        on_thumb = false;
        int i = (y - tv->style->padding_top) / getLineHeight(tv->style);
        assert(i >= 0);

        Item  *stack[32];
        size_t depth;
        int status = getVisibleTreeItemByIndex(tv, i, stack, &depth, sizeof(stack)/sizeof(stack[0]));

        if (status == 1) {
            Item *item = stack[depth-1];
            if (item->type == ItemType_DIR)
                item->open = !item->open;
            else {
                char path[1024];
                size_t len = concatPath(tv->path, tv->path_len, stack, depth, path);
                if (tv->callback != NULL)
                    tv->callback(path, len, tv->userp);
            }
        }
    }
    return NULL;
}

static float drawChildren(Item *parent, size_t depth,
                          size_t *visited_items, Font font,
                          const TreeViewStyle *style,
                          int x_scroll, int y_scroll)
{
    size_t line_height = getLineHeight(style);

    float max_w = 0;

    Item *child = parent->children;
    while (child != NULL) {
        /*
        switch (child->type) {
            case ItemType_DIR:break;
            case ItemType_FILE:break;
            case ItemType_OTHER:break;
        }
        */

        int off_x = style->padding_left + depth * style->subtree_padding_left;
        int off_y = style->padding_top  + line_height * (*visited_items);
        
        int real_x = off_x - x_scroll;
        int real_y = off_y - y_scroll;
        float text_w = renderString(font, child->name, child->name_len,
                                    real_x, real_y, style->font_size, 
                                    style->fgcolor);
        float line_w = off_x + text_w;
        if (line_w > max_w)
            max_w = line_w;

        (*visited_items)++;
        if (child->type == ItemType_DIR && child->open) {
            float max_w2 = drawChildren(child, depth+1, visited_items, font, style,
                                        x_scroll, y_scroll);
            if (max_w2 > max_w)
                max_w = max_w2;
        }

        child = child->next;
    }
    return max_w;
}

static float drawSubtree(Item *root, Font font, size_t *num,
                         const TreeViewStyle *style,
                          int x_scroll, int y_scroll)
{
    size_t visited_items = 0;
    float max_w = drawChildren(root, 1, &visited_items, font, style, x_scroll, y_scroll);
    *num = visited_items;
    return max_w;
}

static void drawCallback(GUIElement *elem)
{
    TreeView *tv = (TreeView*) elem;
    BeginTextureMode(tv->texture);
    ClearBackground(tv->style->bgcolor);

    int x_scroll = Scrollbar_getValue(&tv->h_scroll);
    int y_scroll = Scrollbar_getValue(&tv->v_scroll);

    size_t rows;
    float logic_w = drawSubtree(tv->tree, tv->font, &rows, tv->style,
                                x_scroll, y_scroll);

    size_t line_height = getLineHeight(tv->style);
    float logic_h = line_height * rows
                  + tv->style->padding_top;

    scrollbar_draw(&tv->v_scroll);
    scrollbar_draw(&tv->h_scroll);
    EndTextureMode();

    tv->logic_w = logic_w;
    tv->logic_h = logic_h;

    {
        RenderTexture2D target = tv->texture;
        Rectangle region = elem->region;
        Rectangle src, dst;
        src.x = 0;
        src.y = 0;
        src.width  =  region.width;
        src.height = -region.height;
        dst.x = region.x;
        dst.y = region.y;
        dst.width  = region.width;
        dst.height = region.height;
        Vector2 org = {0, 0};
        DrawTexturePro(target.texture, src, 
                       dst, org, 0, WHITE);
    }
}

static void tickCallback(GUIElement *elem, uint64_t time_in_ms)
{
    TreeView *tv = (TreeView*) elem;
    Scrollbar_tick(&tv->v_scroll, time_in_ms);
    Scrollbar_tick(&tv->h_scroll, time_in_ms);
}

static void onMouseWheelCallback(GUIElement *elem, int y)
{
    TreeView *tv = (TreeView*) elem;
    Scrollbar_addForce(&tv->v_scroll, 30 * y);
}

static void onMouseMotionCallback(GUIElement *elem, 
                                  int x, int y)
{
    TreeView *tv = (TreeView*) elem;

    if (Scrollbar_onMouseMotion(&tv->v_scroll, y)) {
    } else if (Scrollbar_onMouseMotion(&tv->h_scroll, x)) {
    }
}

static void clickUpCallback(GUIElement *elem, 
                            int x, int y)
{
    TreeView *tv = (TreeView*) elem;
    Scrollbar_clickUp(&tv->v_scroll);
    Scrollbar_clickUp(&tv->h_scroll);
}

static void 
getLogicalSizeCallback(GUIElement *elem, 
                       int *w, int *h)
{
    TreeView *tv = (TreeView*) elem;
    *w = tv->logic_w;
    *h = tv->logic_h;
}

static const GUIElementMethods methods = {
    .free = freeCallback,
    .tick = tickCallback,
    .draw = drawCallback,
    .clickUp = clickUpCallback,
    .onFocusLost = NULL,
    .onFocusGained = NULL,
    .onClickDown = onClickDownCallback,
    .offClickDown = NULL,
    .onMouseWheel = onMouseWheelCallback,
    .onMouseMotion = onMouseMotionCallback,
    .onArrowLeftDown = NULL,
    .onArrowRightDown = NULL,
    .onReturnDown = NULL,
    .onBackspaceDown = NULL,
    .onTextInput = NULL,
    .onPaste = NULL,
    .onCopy = NULL,
    .onCut = NULL,
    .onSave = NULL,
    .onOpen = NULL,
    .getHovered = NULL,
    .onResize = onResizeCallback,
    .getMinimumSize = getMinimumSize,
    .getLogicalSize = getLogicalSizeCallback,
};

GUIElement *TreeView_new(Rectangle region,
                         const char *name,
                         const char *full_path,
                         void (*callback)(const char*, size_t, void*),
                         const TreeViewStyle *style,
                         void *userp)
{
    size_t full_path_len = strlen(full_path);

    char full_path_copy1[1024];
    char full_path_copy2[1024];
    if (full_path_len >= sizeof(full_path_copy1) ||
        full_path_len >= sizeof(full_path_copy2))
        return NULL;
        
    strcpy(full_path_copy1, full_path);
    strcpy(full_path_copy2, full_path);

    const char *path =  dirname(full_path_copy1);
    const char *base = basename(full_path_copy2);
    size_t path_len = strlen(path);
    size_t base_len = strlen(base);

    TreeView *tv = malloc(sizeof(TreeView));
    if (tv == NULL)
        return NULL;

    tv->base.region = region;
    tv->base.methods = &methods;
    strncpy(tv->base.name, name, sizeof(tv->base.name));
    tv->base.name[sizeof(tv->base.name)-1] = '\0';

    strcpy(tv->path, path);
    tv->path_len = path_len;

    ItemPool *pool = &tv->pool;
    ItemPool_init(pool);

    Item *tree = buildDirectoryTree(path, path_len,
                                    base, base_len, 
                                    8, pool);
    if (tree == NULL) {
        ItemPool_free(pool);
        free(tv);
        return NULL;
    }
    printTree(stderr, tree);
    tv->tree = tree;
    if (style->font_data == NULL)
        tv->font = LoadFontEx(style->font_file, style->font_size, NULL, 250);
    else
        tv->font = LoadFontFromMemory(".ttf", style->font_data, style->font_data_size, 
                                      style->font_size, NULL, 250);
    tv->style = style;
    tv->texture = LoadRenderTexture(region.width, region.height);
    tv->userp = userp;
    tv->callback = callback;
    tv->logic_w = 0;
    tv->logic_h = 0;
    Scrollbar_init(&tv->v_scroll, ScrollbarDirection_VERTICAL,   (GUIElement*) tv, style->v_scroll);
    Scrollbar_init(&tv->h_scroll, ScrollbarDirection_HORIZONTAL, (GUIElement*) tv, style->h_scroll);

    return (GUIElement*) tv;
}